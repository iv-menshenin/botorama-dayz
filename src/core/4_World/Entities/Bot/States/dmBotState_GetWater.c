//! dmBotState_GetWater — бот наполняет пустую бутылку водой из ближайшего источника
//! (колодец/корыто/цистерна-водовоз). PREEMPTIVE: жажда вытесняет мирные состояния.
//! Flow: источник из реестра зданий на входе -> MoveTo (CRITICAL+PARALLEL) -> взять
//! пустую бутылку в руки -> анимация набора (CMD_ACTIONFB_FILLBOTTLEWELL) -> наполнить
//! чистой водой (SetLiquidType(LIQUID_WATER) + SetQuantityMax) -> вернуть в карго -> EXIT.
class dmBotState_GetWater : dmBotState
{
	Building m_Source;
	Building m_FailedSource;   // последний недостижимый источник (exclude при следующем входе)
	ItemBase m_Bottle;
	ref dmBotIntent_MoveTo m_Move;
	HumanCommandActionCallback m_ActionCB;
	float m_FillTimer;
	bool m_Filling;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.PREEMPTIVE;
	}

	override void OnEntry(dmBotState from)
	{
		super.OnEntry(from);

		m_Source = null;
		m_Bottle = null;
		m_Move = null;
		m_ActionCB = null;
		m_FillTimer = 0.0;
		m_Filling = false;

		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return;

		m_Bottle = dmLoot.FindEmptyBottle(pawn);
		if (!m_Bottle)
			return;

		array<Building> exclude = null;
		if (m_FailedSource)
		{
			exclude = new array<Building>();
			exclude.Insert(m_FailedSource);
		}
		m_Source = dmLiveBuildingRegistry.Get().GetNearestWaterSource(pawn.GetPosition(), DM_GET_WATER_SEARCH_RADIUS, exclude);
		if (m_Source)
		{
			#ifdef DM_BOT_DEBUG_BODY
			dmBotLog.Debug("[GetWater] нашёл источник " + m_Source.GetType() + ", иду");
			#endif

			m_Move = new dmBotIntent_MoveTo();
			m_Move.m_Goal = m_Source.GetPosition();
			m_Move.m_ReachDistance = DM_GET_WATER_REACH;
			m_Move.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Move);
		}
		else
		{
			#ifdef DM_BOT_DEBUG_BODY
			dmBotLog.Debug("[GetWater] нет источника в радиусе");
			#endif
		}
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return EXIT;

		//! В бою — прервать наполнение: у GetWater нет исходящих переходов, поэтому
		//! выходим явно (fallback → Exploration → боевой preemption сработает там).
		if (bot.IsInCombat())
		{
			m_CooldownGameTime = GetGame().GetTickTime() + DM_GET_WATER_RETRY_COOLDOWN;
			StopAnim(pawn);
			return EXIT;
		}

		//! Источник не найден или удалился — EXIT (бот вернётся к прошлому состоянию).
		if (!m_Source || m_Source.IsSetForDeletion())
		{
			m_CooldownGameTime = GetGame().GetTickTime() + DM_GET_WATER_RETRY_COOLDOWN;
			return EXIT;
		}

		//! Фаза движения к источнику.
		if (!m_Filling)
		{
			if (m_Move && !m_Move.IsFinished() && !m_Move.IsExpired())
				return CONTINUE;

			if (!m_Move || m_Move.IsFailed() || m_Move.IsExpired())
			{
				//! Не дошёл (источник недостижим) — запомнить и EXIT, анти-цикл.
				m_FailedSource = m_Source;
				#ifdef DM_BOT_DEBUG_BODY
				dmBotLog.Debug("[GetWater] не дошёл до источника, выход");
				#endif
				m_CooldownGameTime = GetGame().GetTickTime() + DM_GET_WATER_RETRY_COOLDOWN;
				return EXIT;
			}

			//! Дошёл: бутылку в руки, анимация набора.
			if (!StartFill(pawn))
			{
				m_CooldownGameTime = GetGame().GetTickTime() + DM_GET_WATER_RETRY_COOLDOWN;
				return EXIT;
			}
			m_Filling = true;
			return CONTINUE;
		}

		//! Фаза наполнения: копим таймер, на пороге — залить воду и завершить.
		m_FillTimer = m_FillTimer + pDt;
		if (m_FillTimer < DM_GET_WATER_FILL_TIME)
			return CONTINUE;

		FillBottle(pawn);
		return EXIT;
	}

	override void OnExit(dmBotState to)
	{
		if (m_Move)
		{
			m_Move.Finish();
			m_Move = null;
		}

		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return;

		StopAnim(pawn);

		//! Вернуть бутылку из рук в карго, если она всё ещё в руках.
		EntityAI inHands = pawn.GetHumanInventory().GetEntityInHands();
		if (inHands == m_Bottle)
			dmLoot.TakeIntoCargo(pawn, m_Bottle);
	}

	//! Взять бутылку в руки и проиграть анимацию набора (гейт: если уже есть команда).
	bool StartFill(dmAISurvivorBase pawn)
	{
		if (!m_Bottle || !dmLoot.TakeToHands(pawn, m_Bottle))
			return false;

		if (!pawn.GetCommandModifier_Action())
			m_ActionCB = pawn.AddCommandModifier_Action(DayZPlayerConstants.CMD_ACTIONFB_FILLBOTTLEWELL, dmBotActionAnimCB);

		#ifdef DM_BOT_DEBUG_BODY
		dmBotLog.Debug("[GetWater] наполняю " + m_Bottle.GetType());
		#endif
		return true;
	}

	//! Залить чистую воду в бутылку, погасить анимацию, вернуть бутылку в карго.
	void FillBottle(dmAISurvivorBase pawn)
	{
		StopAnim(pawn);
		if (m_Bottle)
		{
			m_Bottle.SetLiquidType(LIQUID_WATER);
			m_Bottle.SetQuantityMax();
		}
		#ifdef DM_BOT_DEBUG_BODY
		if (m_Bottle)
			dmBotLog.Debug("[GetWater] наполнил " + m_Bottle.GetType());
		#endif
		if (m_Bottle)
			dmLoot.TakeIntoCargo(pawn, m_Bottle);
		m_Bottle = null;
		m_FailedSource = null;
	}

	//! Погасить анимацию набора (если она активна).
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
}
