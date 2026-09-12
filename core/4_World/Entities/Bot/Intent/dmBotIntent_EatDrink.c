//! dmBotIntent_EatDrink — персональный рефлекс еды/питья (persistent, как TidyInventory).
//! PARALLEL + IDLE + NONE: не занимает MOVE/LOOK, ест на ходу, FSM не перекрывает.
//! 3-фазная стейт-машина: check (найти еду/воду, выбросить плохую еду, разморозить) ->
//! open (закрытая консерва -> _Opened) -> eat (потребление по тикам, полный предмет за
//! DM_EAT_DRINK_FULL_TIME). Анимация additive гасится, когда предмет кончился.
class dmBotIntent_EatDrink : dmBotIntent
{
	float m_Cooldown = 0.0;
	float m_AnimTimer = 0.0;
	int m_Phase = 0;              // 0 = check, 1 = open, 2 = eat
	ItemBase m_Item;
	HumanCommandActionCallback m_ActionCB;

	void dmBotIntent_EatDrink()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.IDLE;
		m_Manage = dmBotIntentsChannel.NONE;
	}

	override string GetIntentName()
	{
		return "EatDrink";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return;

		if (bot.IsInCombat())
		{
			StopAnim(pawn);
			m_Phase = 0;
			m_Item = null;
			m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
			return;
		}

		if (m_Phase == 1)
		{
			PhaseOpen(pawn, pDt);
			return;
		}

		if (m_Phase == 2)
		{
			PhaseEat(pawn, pDt);
			return;
		}

		if (m_Cooldown > 0.0)
		{
			m_Cooldown -= pDt;
			return;
		}

		if (EnemyNear(bot))
		{
			m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
			return;
		}

		HandleFoodState(pawn);

		ItemBase item;
		if (pawn.GetStatEnergy().Get() < DM_EAT_DRINK_ENERGY && pawn.GetStomach().GetStomachVolume() == 0.0)
		{
			item = dmLoot.FindEdible(pawn, false);
			if (item)
			{
				BeginEat(pawn, item);
				return;
			}
		}

		if (pawn.GetStatWater().Get() < DM_EAT_DRINK_WATER && pawn.GetStomach().GetStomachVolume() == 0.0)
		{
			item = dmLoot.FindEdible(pawn, true);
			if (item)
			{
				BeginEat(pawn, item);
				return;
			}
		}

		m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
	}

	//! Взять съедобное в руки; закрытое — открыть (фаза open), иначе сразу есть.
	void BeginEat(dmAISurvivorBase pawn, ItemBase item)
	{
		if (!dmLoot.TakeToHands(pawn, item))
		{
			#ifdef DM_BOT_DEBUG_BODY
			dmBotLog.Debug("[EatDrink] TakeToHands failed " + item.GetType());
			#endif
			m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
			return;
		}

		m_Item = ItemBase.Cast(pawn.GetHumanInventory().GetEntityInHands());
		if (!m_Item)
		{
			m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
			return;
		}

		Edible_Base b = Edible_Base.Cast(m_Item);
		if (b && !b.IsOpen())
		{
			b.Open();
			m_Phase = 1;
			m_AnimTimer = 0.0;
			#ifdef DM_BOT_DEBUG_BODY
			dmBotLog.Debug("[EatDrink] open " + m_Item.GetType());
			#endif
			return;
		}

		StartAnim(pawn);
		m_Phase = 2;
		m_AnimTimer = 0.0;
	}

	//! Фаза 1: ждём, пока закрытая консерва заменится в руках на X_Opened (ReplaceEdibleWithNew).
	void PhaseOpen(dmAISurvivorBase pawn, float pDt)
	{
		m_AnimTimer += pDt;

		EntityAI h = pawn.GetHumanInventory().GetEntityInHands();
		if (h && h.GetType().IndexOf("_Opened") != -1)
		{
			m_Item = ItemBase.Cast(h);
			StartAnim(pawn);
			m_Phase = 2;
			m_AnimTimer = 0.0;
			#ifdef DM_BOT_DEBUG_BODY
			dmBotLog.Debug("[EatDrink] opened " + h.GetType());
			#endif
			return;
		}

		if (m_AnimTimer >= DM_EAT_DRINK_OPEN_TIMEOUT)
		{
			m_Phase = 0;
			m_Item = null;
			m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
			#ifdef DM_BOT_DEBUG_BODY
			dmBotLog.Debug("[EatDrink] open timeout");
			#endif
		}
	}

	//! Фаза 2: потребление по тикам (полный предмет за DM_EAT_DRINK_FULL_TIME).
	void PhaseEat(dmAISurvivorBase pawn, float pDt)
	{
		m_AnimTimer += pDt;

		if (!pawn.GetCommandModifier_Action())
			StartAnim(pawn);

		float portion = m_Item.GetQuantityMax() / DM_EAT_DRINK_FULL_TIME * pDt;
		pawn.Consume(m_Item, portion, EConsumeType.ITEM_CONTINUOUS);

		if (!m_Item || m_Item.GetQuantity() <= 0.0)
		{
			StopAnim(pawn);
			m_Item = null;
			m_Phase = 0;
			m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
			#ifdef DM_BOT_DEBUG_BODY
			dmBotLog.Debug("[EatDrink] item consumed");
			#endif
		}
	}

	//! Выбросить плохую еду (сырое мясо/труп/сгоревшее/гнилое) и разморозить съедобное.
	void HandleFoodState(dmAISurvivorBase pawn)
	{
		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int i;
		for (i = 0; i < items.Count(); i++)
		{
			ItemBase item = ItemBase.Cast(items[i]);
			if (!item)
				continue;
			Edible_Base edible = Edible_Base.Cast(item);
			if (!edible)
				continue;

			if (edible.GetIsFrozen())
			{
				edible.SetFrozen(false);
				#ifdef DM_BOT_DEBUG_BODY
				dmBotLog.Debug("[EatDrink] unfroze " + item.GetType());
				#endif
			}

			if ((edible.IsMeat() && edible.CanBeCooked()) || edible.IsCorpse() || edible.IsFoodBurned() || edible.IsFoodRotten())
			{
				dmInventoryFrame drop = dmInventoryFrame.Make(dmInventoryDoing.PLACEONGROUND, item);
				pawn.GetInventoryFrames().Enqueue(drop);
				#ifdef DM_BOT_DEBUG_BODY
				dmBotLog.Debug("[EatDrink] drop bad food " + item.GetType());
				#endif
			}
		}
	}

	void StartAnim(dmAISurvivorBase pawn)
	{
		if (pawn.GetCommandModifier_Action())
			return;
		int cmd = DayZPlayerConstants.CMD_ACTIONMOD_EAT;
		if (dmLoot.IsDrink(m_Item))
			cmd = DayZPlayerConstants.CMD_ACTIONMOD_DRINK;
		m_ActionCB = pawn.AddCommandModifier_Action(cmd, dmBotActionAnimCB);
	}

	void StopAnim(dmAISurvivorBase pawn)
	{
		if (!m_ActionCB)
			return;
		if (pawn.GetCommandModifier_Action() == m_ActionCB)
			pawn.DeleteCommandModifier_Action(m_ActionCB);
		else
			m_ActionCB.Cancel();
		m_ActionCB = null;
	}

	bool EnemyNear(dmAISurvivor bot)
	{
		dmTarget t = bot.GetHostileTarget();
		if (!t)
			return false;
		vector botPos = bot.GetPosition();
		vector tPos = t.m_LastPosition;
		if (t.m_Entity && t.m_Entity.IsAlive())
			tPos = t.m_Entity.GetPosition();
		return vector.Distance(botPos, tPos) < DM_EAT_DRINK_ENEMY_RADIUS;
	}

	override void OnCancel(dmAISurvivor bot)
	{
		super.OnCancel(bot);
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
			StopAnim(pawn);
		m_Phase = 0;
		m_Item = null;
	}
}
