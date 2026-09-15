//! dmBotIntent_PickUp — walk to a world item and take it into the inventory.
//!
//! It inherits the full path-following machinery from dmBotIntent_MoveTo
//! (steering, stuck detection, vault/climb, ladders, recovery) and only overrides
//! the goal (the item's position), the start (aim the route at the item) and the
//! action at the goal (take the item into the inventory). The item is static, but
//! the goal is refreshed each tick in case it shifts.
class dmBotIntent_PickUp : dmBotIntent_MoveTo
{
	EntityAI m_Item;
	bool m_PickupQueued = false;
	bool m_Evacuating = false;
	bool m_TakeToHands = false;
	ref array<EntityAI> m_EvacOrder;

	void dmBotIntent_PickUp()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "PickUp";
	}

	override void OnStart(dmAISurvivor bot)
	{
		if (m_Item)
			m_Goal = m_Item.GetPosition();
		super.OnStart(bot);

		m_ReachDistance = DM_PICKUP_REACH;
	}

	//! Цель = позиция предмета (предмет статичен, но обновляем на случай сдвига).
	override void UpdateGoal(dmAISurvivor bot, float pDt)
	{
		if (m_Item)
			m_Goal = m_Item.GetPosition();
	}

	//! Достигли предмета — поставить цепочку подбора в менеджер инвентаря, не
	//! завершая интент (ждём, пока вещь фактически ляжет, в OnUpdate).
	override void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		bot.SetMove(0.0, 0.0);

		if (!m_Item || m_Item.IsDamageDestroyed() || m_Item.IsSetForDeletion())
		{
			Fail();
			return;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			Fail();
			return;
		}

		//! Уже ждём выполнения цепочки — не перезапускаем.
		if (m_PickupQueued)
			return;
		m_PickupQueued = true;
		m_EvacOrder = null;

		//! Режим «в руки» (возврат выпавшего оружия): кладём предмет прямо в руки,
		//! а не в слот инвентаря (у голого бота может не быть слота/рюкзака).
		if (m_TakeToHands)
		{
			dmLoot.TakeToHands(pawn, ItemBase.Cast(m_Item));
			Finish();
			return;
		}

		dmInventoryFrame root = pawn.InventoryPickUp(ItemBase.Cast(m_Item));
		if (!root)
		{
			//! Нет места (нет свободного слота/карго) — пробуем репак.
			if (Evacuate(bot, pawn, ItemBase.Cast(m_Item)))
			{
				m_Evacuating = true;
				return;
			}
			bot.GetWishlist().Ignore(m_Item);
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] PickUp: нет места и репак невозможен, игнор " + m_Item.GetType());
			#endif
			Fail();
			return;
		}
		//! Не завершаем — ждём GetHierarchyRootPlayer() != null в OnUpdate.
	}

	//! Эвакуация-репак под конкретный предмет: выложить всё ненужное на пол
	//! (PLACEONGROUND), затем собрать обратно — новую вещь первой, затем остальные
	//! с конца (TAKEINTOCARGO, to=null → FindDestination). Фреймы ставятся
	//! параллельными Enqueue — фейл одного шага не прерывает процесс.
	bool Evacuate(dmAISurvivor bot, dmAISurvivorBase pawn, ItemBase item)
	{
		dmRequirements req = bot.GetRequirements();
		if (!req)
			return false;
		ref array<EntityAI> order = req.GetDiscardOrder();
		if (order.Count() == 0)
			return false;
		m_EvacOrder = order;
		dmInventoryFrames frames = pawn.GetInventoryFrames();
		if (!frames)
			return false;

		int i;
		ItemBase o;

		for (i = 0; i < order.Count(); i++)
		{
			o = ItemBase.Cast(order[i]);
			if (!o || o == item)
				continue;
			frames.Enqueue(dmInventoryFrame.Make(dmInventoryDoing.PLACEONGROUND, o, -1, null));
		}

		frames.Enqueue(dmInventoryFrame.Make(dmInventoryDoing.TAKEINTOCARGO, item, -1, null));
		for (i = order.Count() - 1; i >= 0; i--)
		{
			o = ItemBase.Cast(order[i]);
			if (!o || o == item)
				continue;
			frames.Enqueue(dmInventoryFrame.Make(dmInventoryDoing.TAKEINTOCARGO, o, -1, null));
		}

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] Evacuate: начинаю репак для " + item.GetType() + ", вещей=" + order.Count());
		#endif
		return true;
	}

	//! Игнор выложенных при репаке вещей, что не легли обратно (остались на полу) —
	//! чтобы не подбирать их заново. Обнуляет сохранённый порядок выброса.
	void IgnoreLeftovers(dmAISurvivor bot)
	{
		if (!m_EvacOrder)
			return;

		int i;
		EntityAI o;
		for (i = 0; i < m_EvacOrder.Count(); i++)
		{
			o = m_EvacOrder[i];
			if (o && o.GetHierarchyRootPlayer() == null)
				bot.GetWishlist().Ignore(o);
		}
		m_EvacOrder = null;
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());

		if (m_PickupQueued && !m_Evacuating && m_Item && m_Item.GetHierarchyRootPlayer() != null)
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] PickUp: вещь легла " + m_Item.GetType());
			#endif
			Finish();
			return;
		}

		if (m_Evacuating && pawn && pawn.GetInventoryFrames().IsEmpty())
		{
			IgnoreLeftovers(bot);
			if (m_Item && m_Item.GetHierarchyRootPlayer() != null)
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] Evacuate: репак завершён, вещь легла " + m_Item.GetType());
				#endif
				Finish();
				return;
			}
			bot.GetWishlist().Ignore(m_Item);
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] Evacuate: репак завершён, вещь не легла — игнор " + m_Item.GetType());
			#endif
			Fail();
			return;
		}

		if (m_Evacuating)
			return;

		if (m_PickupQueued && pawn && pawn.GetInventoryFrames().IsEmpty())
		{
			bot.GetWishlist().Ignore(m_Item);
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] PickUp: очередь пуста, вещь не легла — игнор " + m_Item.GetType());
			#endif
			Fail();
			return;
		}

		if (!m_Item || m_Item.IsDamageDestroyed() || m_Item.IsSetForDeletion())
		{
			Fail();
			return;
		}
		super.OnUpdate(bot, pDt);
	}
};
