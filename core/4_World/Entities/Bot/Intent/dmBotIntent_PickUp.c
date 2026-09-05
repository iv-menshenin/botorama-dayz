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
	ref dmInventoryFrame m_Root;
	bool m_PickupQueued = false;

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
	//! завершая интент (ждём IsAllDone() в OnUpdate).
	override void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		bot.SetMove(0.0, 0.0);

		//! Уже ждём выполнения цепочки — не перезапускаем.
		if (m_PickupQueued)
			return;
		m_PickupQueued = true;

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

		m_Root = pawn.InventoryPickUp(ItemBase.Cast(m_Item));
		if (!m_Root)
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] InventoryPickUp: нет цепочки (нет рюкзака/слота)");
			#endif
			Fail();
			return;
		}
		//! Не завершаем — ждём IsAllDone() в OnUpdate.
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		if (m_PickupQueued && m_Root && m_Root.IsAllDone())
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] PickUp: цепочка инвентаря завершена");
			#endif
			Finish();
		}
	}
};
