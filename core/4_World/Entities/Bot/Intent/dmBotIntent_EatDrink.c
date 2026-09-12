//! dmBotIntent_EatDrink — персональный рефлекс еды/питья (persistent, как TidyInventory).
//! PARALLEL + IDLE + NONE: не занимает MOVE/LOOK, ест на ходу, FSM не перекрывает.
//! Если нет боя и враг дальше DM_EAT_DRINK_ENEMY_RADIUS: при энергии < порога и пустом
//! животе — ест; при воде < порога и пустом животе — пьёт. Анимация additive проигрывается
//! DM_EAT_DRINK_ANIM_TIME, затем гасится, и предмет потребляется.
class dmBotIntent_EatDrink : dmBotIntent
{
	float m_Cooldown = 0.0;
	float m_AnimTimer = 0.0;
	int m_Phase = 0;              // 0 = проверка, 1 = анимация
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
			m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
			return;
		}

		if (m_Phase == 1)
		{
			m_AnimTimer += pDt;
			if (m_AnimTimer >= DM_EAT_DRINK_ANIM_TIME)
			{
				StopAnim(pawn);
				Consume(pawn);
				m_Phase = 0;
				m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
			}
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

		//! Еда.
		if (pawn.GetStatEnergy().Get() < DM_EAT_DRINK_ENERGY && pawn.GetStomach().GetStomachVolume() == 0.0)
		{
			m_Item = dmLoot.FindEdible(pawn, false);
			if (m_Item)
			{
				StartAnim(pawn, false);
				return;
			}
		}

		//! Вода.
		if (pawn.GetStatWater().Get() < DM_EAT_DRINK_WATER && pawn.GetStomach().GetStomachVolume() == 0.0)
		{
			m_Item = dmLoot.FindEdible(pawn, true);
			if (m_Item)
			{
				StartAnim(pawn, true);
				return;
			}
		}

		m_Cooldown = DM_EAT_DRINK_SCAN_INTERVAL;
	}

	void StartAnim(dmAISurvivorBase pawn, bool drink)
	{
		if (pawn.GetCommandModifier_Action())
			return;
		int cmd = DayZPlayerConstants.CMD_ACTIONMOD_EAT;
		if (drink)
			cmd = DayZPlayerConstants.CMD_ACTIONMOD_DRINK;
		m_ActionCB = pawn.AddCommandModifier_Action(cmd, dmBotActionAnimCB);
		m_AnimTimer = 0.0;
		m_Phase = 1;
	}

	void StopAnim(dmAISurvivorBase pawn)
	{
		if (!m_ActionCB)
			return;
		if (pawn.GetCommandModifier_Action() == m_ActionCB)
			pawn.DeleteCommandModifier_Action(m_ActionCB);
		m_ActionCB = null;
	}

	void Consume(dmAISurvivorBase pawn)
	{
		if (!m_Item)
			return;
		pawn.Consume(m_Item, 1.0, EConsumeType.ITEM_SINGLE_TIME);
		m_Item = null;
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
	}
}
