//! dmBotState_MedicalCare — ИИ-лечение: не в бою бот лечит себя по очереди шагов
//! (бинт -> шина -> уголь -> тетрациклин -> витамины; обезбол — бонус, дедуплицирован).
//!
//! Каждый шаг — отдельный dmBotIntent_MedicalAction; условие шага пере-проверяется
//! перед исполнением (здоровье могло измениться). Очередь пуста -> EXIT.
//! INTERRUPTIBLE: бой вытесняет.

//! Шаг очереди лечения (int-константа).
enum dmMedicalStep
{
	STEP_BANDAGE,       // перевязка
	STEP_PAINKILLER,    // бонус-обезбол (дедуп)
	STEP_SPLINT,        // шина
	STEP_CHARCOAL,      // уголь
	STEP_TETRACYCLINE,  // антибиотик
	STEP_VITAMINS       // мультивитамины (бонус/холод)
}

class dmBotState_MedicalCare : dmBotState
{
	ref dmBotIntent_MedicalAction m_Intent;
	ref array<int> m_Queue;
	int m_QueueIdx;

	//! Фаза «укрытие» перед лечением: активна только при входе с флагом
	//! IsBulletDanger — бот сначала находит укрытие и доходит до него, потом лечится.
	bool m_SeekingCover = false;
	ref dmBotIntent_MoveTo m_CoverMove;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.INTERRUPTIBLE;
	}

	override void OnEntry(dmBotState from)
	{
		m_Intent = null;
		m_Queue = new array<int>();
		m_QueueIdx = 0;
		m_SeekingCover = false;
		m_CoverMove = null;
		BuildQueue();

		//! Триггер фазы укрытия: явный, только при входе с флагом пулевой опасности.
		dmAISurvivor bot = GetOwner();
		if (bot.IsBulletDanger())
		{
			m_SeekingCover = true;
			StartCoverSearch();
		}

		#ifdef DM_BOT_DEBUG_MEDICAL
		dmBotLog.Debug("[Medical] entry: steps=" + m_Queue.Count());
		#endif
	}

	override void OnExit(dmBotState to)
	{
		if (m_CoverMove)
		{
			m_CoverMove.Finish();
			m_CoverMove = null;
		}
		m_SeekingCover = false;

		dmAISurvivor bot = GetOwner();
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		//! Вернуть остаток предмета из рук в инвентарь (бинт/таблетки с остатком количества),
		//! чтобы бот не ходил с ними в руках после лечения.
		EntityAI inHands = pawn.GetHumanInventory().GetEntityInHands();
		if (inHands)
		{
			ItemBase item = ItemBase.Cast(inHands);
			if (item)
				dmLoot.TakeIntoCargo(pawn, item);
		}
	}

	override int OnUpdate(float pDt)
	{
		//! Фаза укрытия: ждём завершения MoveTo (дошёл/провал/истечение). По любому
		//! исходу снимаем флаг опасности и переходим к обычной очереди лечения.
		if (m_SeekingCover)
		{
			if (m_CoverMove && !m_CoverMove.IsFinished() && !m_CoverMove.IsExpired())
				return CONTINUE;

			dmAISurvivor bot = GetOwner();
			bot.SetBulletDanger(false);

			#ifdef DM_BOT_DEBUG_COVER
			dmBotLog.Debug("[Medical] cover done -> heal");
			#endif

			m_SeekingCover = false;
			m_CoverMove = null;
		}

		//! Активный шаг ещё выполняется — ждём завершения.
		if (m_Intent && !m_Intent.IsFinished() && !m_Intent.IsExpired())
			return CONTINUE;

		//! Шаг завершился/истёк — сбрасываем.
		if (m_Intent)
			m_Intent = null;

		//! Очередь исчерпана — лечение завершено.
		if (m_QueueIdx >= m_Queue.Count())
		{
			#ifdef DM_BOT_DEBUG_MEDICAL
			dmBotLog.Debug("[Medical] queue done, exit");
			#endif
			return EXIT;
		}

		int step = m_Queue[m_QueueIdx];
		m_QueueIdx = m_QueueIdx + 1;

		//! Пере-проверить условие шага (здоровье могло измениться); не выполнено — пропустить.
		if (!StepStillNeeded(step))
		{
			#ifdef DM_BOT_DEBUG_MEDICAL
			dmBotLog.Debug("[Medical] skip step=" + step);
			#endif
			return CONTINUE;
		}

		CreateIntent(step);
		return CONTINUE;
	}

	//! Запустить поиск укрытия: threatPoint — позиция ближайшей враждебной цели
	//! (живая -> m_Entity.GetPosition(); иначе m_LastPosition; нет цели -> позиция бота).
	//! Нашёл — CRITICAL+PARALLEL MoveTo; не нашёл — сброс флага (лечимся на месте).
	void StartCoverSearch()
	{
		dmAISurvivor bot = GetOwner();

		#ifdef DM_BOT_DEBUG_COVER
		dmBotLog.Debug("[Medical] cover phase: enter (bullet danger)");
		#endif

		vector threatPoint = bot.GetPosition();
		dmTarget hostile = bot.GetHostileTarget();
		if (hostile)
		{
			if (hostile.m_Entity)
				threatPoint = hostile.m_Entity.GetPosition();
			else
				threatPoint = hostile.m_LastPosition;
		}

		vector dest;
		if (dmCoverFinder.FindSafeCover(bot, threatPoint, DM_MEDICAL_COVER_MIN_DIST, DM_MEDICAL_COVER_MAX_DIST, DM_COVER_SAFE_RADIUS, dest))
		{
			#ifdef DM_BOT_DEBUG_COVER
			dmBotLog.Debug("[Medical] cover found: dest=" + dest);
			#endif

			m_CoverMove = new dmBotIntent_MoveTo();
			m_CoverMove.m_Goal = dest;
			m_CoverMove.m_ReachDistance = DM_PATROL_REACH_DISTANCE;
			m_CoverMove.m_Priority = dmBotIntentPriority.CRITICAL;
			m_CoverMove.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_CoverMove);
		}
		else
		{
			#ifdef DM_BOT_DEBUG_COVER
			dmBotLog.Debug("[Medical] cover not found: heal in place");
			#endif
			m_SeekingCover = false;
			bot.SetBulletDanger(false);
		}
	}

	//! Собрать очередь шагов (порядок: бинт -> обезбол -> шина -> обезбол -> уголь ->
	//! тетрациклин -> витамины; обезбол дедуплицирован).
	void BuildQueue()
	{
		dmAISurvivor bot = GetOwner();
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		ModifiersManager mngr = pawn.GetModifiersManager();
		bool painkillerActive = false;
		if (mngr)
			painkillerActive = mngr.IsModifierActive(eModifiers.MDF_PAINKILLERS);

		bool painkillerQueued = false;

		//! 1. Бинт + бонус-обезбол (только при низком HP).
		if (pawn.IsBleeding() && dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.BANDAGE))
		{
			m_Queue.Insert(dmMedicalStep.STEP_BANDAGE);
			if (pawn.GetHealth01() < DM_MEDICAL_PAINKILLER_HEALTH_THRESHOLD && !painkillerActive && dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.PAINKILLER))
			{
				m_Queue.Insert(dmMedicalStep.STEP_PAINKILLER);
				painkillerQueued = true;
			}
		}

		//! 2. Шина + бонус-обезбол (всегда после шины, дедуп).
		if (pawn.GetBrokenLegs() == eBrokenLegs.BROKEN_LEGS)
		{
			m_Queue.Insert(dmMedicalStep.STEP_SPLINT);
			if (!painkillerQueued && !painkillerActive && dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.PAINKILLER))
			{
				m_Queue.Insert(dmMedicalStep.STEP_PAINKILLER);
				painkillerQueued = true;
			}
		}

		//! 3. Уголь.
		if (mngr && mngr.IsModifierActive(eModifiers.MDF_SALMONELLA) && !mngr.IsModifierActive(eModifiers.MDF_CHARCOAL) && dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.CHARCOAL))
			m_Queue.Insert(dmMedicalStep.STEP_CHARCOAL);

		//! 4. Тетрациклин.
		if (mngr && (mngr.IsModifierActive(eModifiers.MDF_INFLUENZA) || mngr.IsModifierActive(eModifiers.MDF_COMMON_COLD)) && !mngr.IsModifierActive(eModifiers.MDF_ANTIBIOTICS) && dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.TETRACYCLINE))
			m_Queue.Insert(dmMedicalStep.STEP_TETRACYCLINE);

		//! 5. Витамины (бонус; при холоде — единственный шаг).
		if (mngr && !mngr.IsModifierActive(eModifiers.MDF_IMMUNITYBOOST) && dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.VITAMINS))
			m_Queue.Insert(dmMedicalStep.STEP_VITAMINS);
	}

	//! Условие шага ещё актуально (пере-проверка по текущему здоровью).
	bool StepStillNeeded(int step)
	{
		dmAISurvivor bot = GetOwner();
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return false;

		ModifiersManager mngr = pawn.GetModifiersManager();

		if (step == dmMedicalStep.STEP_BANDAGE)
			return pawn.IsBleeding();

		if (step == dmMedicalStep.STEP_PAINKILLER)
		{
			if (mngr && mngr.IsModifierActive(eModifiers.MDF_PAINKILLERS))
				return false;
			return dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.PAINKILLER) != null;
		}

		if (step == dmMedicalStep.STEP_SPLINT)
			return pawn.GetBrokenLegs() == eBrokenLegs.BROKEN_LEGS;

		if (step == dmMedicalStep.STEP_CHARCOAL)
		{
			if (mngr && mngr.IsModifierActive(eModifiers.MDF_CHARCOAL))
				return false;
			return mngr && mngr.IsModifierActive(eModifiers.MDF_SALMONELLA) && dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.CHARCOAL) != null;
		}

		if (step == dmMedicalStep.STEP_TETRACYCLINE)
		{
			if (mngr && mngr.IsModifierActive(eModifiers.MDF_ANTIBIOTICS))
				return false;
			return mngr && (mngr.IsModifierActive(eModifiers.MDF_INFLUENZA) || mngr.IsModifierActive(eModifiers.MDF_COMMON_COLD)) && dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.TETRACYCLINE) != null;
		}

		if (step == dmMedicalStep.STEP_VITAMINS)
		{
			if (mngr && mngr.IsModifierActive(eModifiers.MDF_IMMUNITYBOOST))
				return false;
			return dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.VITAMINS) != null;
		}

		return false;
	}

	//! Создать интент под шаг и запустить его.
	void CreateIntent(int step)
	{
		dmAISurvivor bot = GetOwner();
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		dmBotIntent_MedicalAction intent = new dmBotIntent_MedicalAction();
		ItemBase item = null;

		#ifdef DM_BOT_DEBUG_MEDICAL
		dmBotLog.Debug("[Medical] CreateIntent: step=" + step);
		#endif

		if (step == dmMedicalStep.STEP_BANDAGE)
		{
			item = dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.BANDAGE);
			intent.m_Kind = dmMedicalActionKind.ACTION_BANDAGE;
		}
		else if (step == dmMedicalStep.STEP_PAINKILLER)
		{
			item = dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.PAINKILLER);
			intent.m_Kind = dmMedicalActionKind.ACTION_PILL;
			intent.m_AnimCmd = DayZPlayerConstants.CMD_ACTIONMOD_EAT_TABLET;
		}
		else if (step == dmMedicalStep.STEP_SPLINT)
		{
			item = dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.SPLINT);
			intent.m_Kind = dmMedicalActionKind.ACTION_SPLINT;
		}
		else if (step == dmMedicalStep.STEP_CHARCOAL)
		{
			item = dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.CHARCOAL);
			intent.m_Kind = dmMedicalActionKind.ACTION_PILL;
			intent.m_AnimCmd = DayZPlayerConstants.CMD_ACTIONMOD_EAT_TABLET;
		}
		else if (step == dmMedicalStep.STEP_TETRACYCLINE)
		{
			item = dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.TETRACYCLINE);
			intent.m_Kind = dmMedicalActionKind.ACTION_PILL;
			intent.m_AnimCmd = DayZPlayerConstants.CMD_ACTIONMOD_EAT_TABLET;
		}
		else if (step == dmMedicalStep.STEP_VITAMINS)
		{
			item = dmLoot.FindMedicalItem(pawn, dmMedicalItemKind.VITAMINS);
			intent.m_Kind = dmMedicalActionKind.ACTION_PILL;
			intent.m_AnimCmd = DayZPlayerConstants.CMD_ACTIONMOD_EAT_PILL;
		}

		//! Шина может быть без предмета (спавн в руки); прочие — только при наличии.
		if (!item && step != dmMedicalStep.STEP_SPLINT)
			return;

		intent.m_Item = item;
		m_Intent = intent;
		GetOwner().AddFSMIntent(m_Intent);

		#ifdef DM_BOT_DEBUG_MEDICAL
		dmBotLog.Debug("[Medical] start step=" + step);
		#endif
	}
}
