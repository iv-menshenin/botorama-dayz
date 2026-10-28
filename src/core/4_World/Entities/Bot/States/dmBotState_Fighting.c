//! dmBotState_Fighting — reactive melee engagement (mini-FSM skeleton).
//!
//! PREEMPTIVE "melee fight" state (no raised stance — strikes come from ERECT).
//! A minimal phase machine driving the fight; this atom implements only the
//! APPROACH phase — the bot resolves a hostile target and closes the distance.
//! Strike / body-turn / block / evade phases arrive in follow-up atoms.
//! Target resolution runs on entry and every DM_FIGHT_RETARGET_INTERVAL seconds,
//! always taking the nearest hostile from dmAISurvivor.GetHostileTarget().

enum dmFightPhase
{
	FIGHT_FIRST_MOVE,  // зомби: стартовый блок (позже)
	FIGHT_APPROACH,
	FIGHT_TURN,
	FIGHT_STRIKE,
	FIGHT_BLOCK,
	FIGHT_EVADE,
};

enum dmFightEnemyType
{
	FIGHT_ZOMBIE,
	FIGHT_PLAYER,
	FIGHT_ANIMAL,
};

class dmBotState_Fighting : dmBotState
{
	EntityAI m_TargetEntity;
	ref dmTarget m_Target;
	ref dmBotIntent_Approach m_Approach;
	ref dmBotIntent_HoldLook m_Look;
	ref dmBotIntent_MeleeStrike m_Strike;
	ref dmBotIntent_MeleeBlock m_Block;
	ref dmBotIntent_MeleeEvade m_Evade;
	float m_RetargetTimer;
	float m_PreferredSpeed;
	ref dmInventoryFrame m_EquipRoot;

	dmFightPhase m_Phase;
	dmFightEnemyType m_EnemyType;
	float m_PhaseTimer;
	dmFightPhase m_LastLoggedPhase;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.PREEMPTIVE;
	}

	override bool CanEnter()
	{
		dmTarget t = GetOwner().GetHostileTarget();
		bool ok = super.CanEnter();
		#ifdef DM_BOT_DEBUG_FSM_FIGHTING
		if (t)
			dmBotLog.Debug("[FSM] Fighting: CanEnter=" + ok + " hostile=" + t.m_Entity);
		else
			dmBotLog.Debug("[FSM] Fighting: CanEnter=" + ok + " hostile=null");
		#endif
		return ok;
	}

	override void OnEntry(dmBotState from)
	{
		m_TargetEntity = null;
		m_Target = null;
		m_Approach = null;
		m_Look = null;
		m_Strike = null;
		m_Block = null;
		m_Evade = null;
		m_RetargetTimer = 0.0;
		m_EquipRoot = null;

		dmAISurvivor bot = GetOwner();
		bot.SetInCombat(true);
		m_PreferredSpeed = bot.GetPreferredSpeed();
		bot.SetPreferredSpeed(3.0);

		ResolveTarget();

		m_Phase = dmFightPhase.FIGHT_APPROACH;
		m_PhaseTimer = 0.0;
		m_LastLoggedPhase = dmFightPhase.FIGHT_FIRST_MOVE; // != APPROACH: первый тик залогирует смену фазы
		DetectEnemyType();

		#ifdef DM_BOT_DEBUG_FSM_FIGHTING
		if (m_TargetEntity)
			dmBotLog.Debug("[FSM] Fighting.entry target=" + m_TargetEntity.GetType());
		else
			dmBotLog.Debug("[FSM] Fighting.entry (нет цели)");
		#endif

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[FSM] Fighting.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		if (!m_TargetEntity || !m_TargetEntity.IsAlive()) ResolveTarget();
		if (!m_TargetEntity) return EXIT;

		m_RetargetTimer += pDt;
		if (m_RetargetTimer >= DM_FIGHT_RETARGET_INTERVAL)
		{
			m_RetargetTimer = 0.0;
			ResolveTarget();
		}
		if (!m_TargetEntity)
			return EXIT;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
			EnsureMeleeEquipped(bot, pawn);

		vector botPos = bot.GetPosition();
		vector tPos = m_TargetEntity.GetPosition();
		vector d = tPos - botPos;
		d[1] = 0.0;
		float dist = d.Length() - 0.5;
		float reach = GetMeleeReach(bot);
		float yawToTarget = d.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float yawDiff = dmAISurvivor.AngleDiff(yawToTarget, bodyYaw);

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[FSM] Fighting dist=" + dist + " reach=" + reach + " yawDiff=" + yawDiff);
		#endif

		// 1) выход фазы
		if (PhaseExited(pDt))
			m_Phase = DecideNextPhase();
		// 2) действие фазы
		switch (m_Phase)
		{
			case dmFightPhase.FIGHT_STRIKE:
				Phase_Strike(bot);
				break;
			case dmFightPhase.FIGHT_TURN:
				Phase_Turn(bot);
				break;
			case dmFightPhase.FIGHT_BLOCK:
				Phase_Block(bot);
				break;
			case dmFightPhase.FIGHT_EVADE:
				Phase_Evade(bot);
				break;
			default:
				Phase_Approach(bot);
				break;
		}
		m_PhaseTimer += pDt;

		// edge-лог смены фазы
		if (m_Phase != m_LastLoggedPhase)
		{
			m_LastLoggedPhase = m_Phase;
			#ifdef DM_BOT_DEBUG_FSM_FIGHTING
			dmBotLog.Debug("[FSM] Fighting phase=" + PhaseName(m_Phase));
			#endif
		}
		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		#ifdef DM_BOT_DEBUG_FSM_FIGHTING
		dmBotLog.Debug("[FSM] Fighting.exit");
		#endif

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[FSM] Fighting.exit");
		#endif

		if ( m_Approach ) m_Approach.Finish();
		if ( m_Look ) { m_Look.Finish(); m_Look = null; }
		if ( m_Strike ) { m_Strike.Finish(); m_Strike = null; }
		if ( m_Block ) { m_Block.Finish(); m_Block = null; }
		if ( m_Evade ) { m_Evade.Finish(); m_Evade = null; }

		dmAISurvivor bot = GetOwner();
		if ( bot )
		{
			bot.SetInCombat(false);
			bot.ClearMeleeDanger();
			bot.SetPreferredSpeed(m_PreferredSpeed);
		}
	}

	//! Хард-блокер выхода: активная фаза мили при живой угрозе — не покидать
	//! состояние (ни кооперативным EXIT, ни вытеснением). Побочных эффектов нет —
	//! сброс мили-флагов выполняется в OnExit.
	override bool CanExit()
	{
		dmAISurvivor bot = GetOwner();
		if ( bot.GetHostileTarget() != null && bot.IsMeleeEngaged() )
			return false;    // активная фаза мили: нельзя выйти (и не прервать вытеснением)
		return super.CanExit();
	}

	void ResolveTarget()
	{
		dmAISurvivor bot = GetOwner();
		dmTarget t = bot.GetHostileTarget();
		EntityAI newEntity = null;
		if (t)
			newEntity = t.m_Entity;

		if (newEntity != m_TargetEntity)
		{
			//! Цель сменилась (или исчезла) — сбросить интент, чтобы он
			//! пересоздался под новую цель (иначе подходит к старой).
			if (m_Approach) { m_Approach.Finish(); m_Approach = null; }
			if (m_Look) { m_Look.Finish(); m_Look = null; }
			if (m_Strike) { m_Strike.Finish(); m_Strike = null; }
			if (m_Block) { m_Block.Finish(); m_Block = null; }
			if (m_Evade) { m_Evade.Finish(); m_Evade = null; }

			#ifdef DM_BOT_DEBUG_FIGHT
			dmBotLog.Debug("[FSM] Fighting: цель сменилась, интенты сброшены");
			#endif
		}

		m_Target = t;
		m_TargetEntity = newEntity;
	}

	void DetectEnemyType()
	{
		m_EnemyType = dmFightEnemyType.FIGHT_ZOMBIE;
		if (!m_TargetEntity) return;
		if (PlayerBase.Cast(m_TargetEntity)) m_EnemyType = dmFightEnemyType.FIGHT_PLAYER;
		else if (AnimalBase.Cast(m_TargetEntity)) m_EnemyType = dmFightEnemyType.FIGHT_ANIMAL;
	}

	void EnsureApproach(dmAISurvivor bot)
	{
		if (m_Approach && (m_Approach.IsFinished() || m_Approach.IsExpired()))
			m_Approach = null;
		if (!m_Approach)
		{
			m_Approach = new dmBotIntent_Approach();
			m_Approach.m_TargetEntity = m_TargetEntity;
			//! Гистерезис 2×рейнджа вернём в финальном атоме — сейчас нужен стоп ровно в зоне удара.
			m_Approach.m_ReachDistance = 1.0 * GetMeleeReach(bot);
			m_Approach.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Approach.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Approach);
		}
	}

	void Phase_Approach(dmAISurvivor bot)
	{
		EnsureApproach(bot);
		m_Approach.m_Active = true;
		if (m_Look) m_Look.m_Active = false;   // подход сам доворачивает через KeepLookAtGoal
	}

	void EnsureLook(dmAISurvivor bot)
	{
		if (m_Look && (m_Look.IsFinished() || m_Look.IsExpired()))
			m_Look = null;
		if (!m_Look)
		{
			m_Look = new dmBotIntent_HoldLook();
			m_Look.m_Entity = m_TargetEntity;
			m_Look.m_Turn = dmBotLookTurn.FULL;
			m_Look.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Look.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Look);
		}
	}

	void EnsureStrike(dmAISurvivor bot)
	{
		if (m_Strike && (m_Strike.IsFinished() || m_Strike.IsExpired()))
			m_Strike = null;
		if (!m_Strike)
		{
			m_Strike = new dmBotIntent_MeleeStrike();
			m_Strike.m_TargetEntity = m_TargetEntity;
			m_Strike.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Strike.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Strike);
		}
	}

	void Phase_Strike(dmAISurvivor bot)
	{
		EnsureStrike(bot);
		m_Strike.m_Active = true;
		if (m_Approach) m_Approach.m_Active = false;  // стоим, бьём
		if (m_Look) m_Look.m_Active = false;
	}

	void Phase_Turn(dmAISurvivor bot)
	{
		if (m_Approach) m_Approach.m_Active = false;   // стоим на месте — доворот без движения
		EnsureLook(bot);
		m_Look.m_Active = true;                        // HoldLook (FULL) доворачивает каждый тик
	}

	void EnsureBlock(dmAISurvivor bot)
	{
		if (m_Block && (m_Block.IsFinished() || m_Block.IsExpired()))
			m_Block = null;
		if (!m_Block)
		{
			m_Block = new dmBotIntent_MeleeBlock();
			m_Block.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Block.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Block);
		}
	}

	void Phase_Block(dmAISurvivor bot)
	{
		EnsureBlock(bot);
		m_Block.m_Active = true;
		if (m_Approach) m_Approach.m_Active = false;
		EnsureLook(bot);
		m_Look.m_Active = true;   // корпус к врагу — иначе EvaluateHit_Player не гасит урон
	}

	void EnsureEvade(dmAISurvivor bot)
	{
		if (m_Evade && (m_Evade.IsFinished() || m_Evade.IsExpired()))
			m_Evade = null;
		if (!m_Evade)
		{
			m_Evade = new dmBotIntent_MeleeEvade();
			m_Evade.m_TargetEntity = m_TargetEntity;
			m_Evade.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Evade.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Evade);
		}
	}

	void Phase_Evade(dmAISurvivor bot)
	{
		EnsureEvade(bot);
		m_Evade.m_Active = true;
		if (m_Approach) m_Approach.m_Active = false;
		if (m_Look) m_Look.m_Active = false;   // ролл сам разворачивает
	}

	bool PhaseExited(float pDt)
	{
		switch (m_Phase)
		{
			case dmFightPhase.FIGHT_APPROACH:
				return InStrikeRange();      // dist <= StrikeRange() — в зоне удара
			case dmFightPhase.FIGHT_TURN:
				return AngleAllowsStrike();   // довернулся до 20° — можно бить
			case dmFightPhase.FIGHT_STRIKE:
				return m_Strike && m_Strike.IsFinished();   // интент завершил удар
			case dmFightPhase.FIGHT_BLOCK:
				return (m_Block && m_Block.IsFinished()) || !EnemyIsWindingUp();   // удар/таймаут ИЛИ враг закончил melee2
			case dmFightPhase.FIGHT_EVADE:
				return m_Evade && m_Evade.IsFinished();   // ролл завершён
		}
		return true;
	}

	dmFightPhase DecideNextPhase()
	{
		if (EnemyTooClose() && EnemyIsWindingUp() && EnemyIsFacingMe())
		{
			if (AngleAllowsBlock())
				return dmFightPhase.FIGHT_BLOCK;
			return dmFightPhase.FIGHT_EVADE;
		}
		if (!InStrikeRange()) return dmFightPhase.FIGHT_APPROACH;
		if (!AngleAllowsStrike()) return dmFightPhase.FIGHT_TURN;
		return dmFightPhase.FIGHT_STRIKE;
	}

	string PhaseName(dmFightPhase p)
	{
		switch (p)
		{
			case dmFightPhase.FIGHT_FIRST_MOVE:
				return "FIRST_MOVE";
			case dmFightPhase.FIGHT_APPROACH:
				return "APPROACH";
			case dmFightPhase.FIGHT_TURN:
				return "TURN";
			case dmFightPhase.FIGHT_STRIKE:
				return "STRIKE";
			case dmFightPhase.FIGHT_BLOCK:
				return "BLOCK";
			case dmFightPhase.FIGHT_EVADE:
				return "EVADE";
		}
		return "UNKNOWN";
	}

	float DistToTarget()
	{
		if (!m_TargetEntity) return 99999.0;
		vector botPos = GetOwner().GetPosition();
		vector tPos = m_TargetEntity.GetPosition();
		vector d = tPos - botPos;
		d[1] = 0.0;
		return d.Length() - 0.5;
	}

	float StrikeRange() { return GetMeleeReach(GetOwner()); }

	bool InStrikeRange() { return DistToTarget() <= StrikeRange(); }

	//! Signed yaw difference (degrees) between the body facing and the target direction.
	float AngleToTarget()
	{
		if (!m_TargetEntity) return 0.0;
		vector botPos = GetOwner().GetPosition();
		vector tPos = m_TargetEntity.GetPosition();
		vector d = tPos - botPos;
		d[1] = 0.0;
		float yawToTarget = d.VectorToAngles()[0];
		float bodyYaw = GetOwner().GetOrientation()[0];
		return dmAISurvivor.AngleDiff(yawToTarget, bodyYaw);
	}

	bool AngleAllowsStrike()
	{
		return Math.AbsFloat(AngleToTarget()) <= DM_MELEE_FACE_ANGLE;
	}

	//! Враг вплотную — блок/эвейд уже уместен, а не удар.
	bool EnemyTooClose()
	{
		return DistToTarget() <= DM_MELEE_BLOCK_RANGE;
	}

	//! Враг сейчас в замахе/атаке мили (мы «ожидаем удара» от него).
	bool EnemyIsWindingUp()
	{
		return m_Target && m_Target.IsWindingUp();
	}

	//! Корпус врага направлен на бота (атакует «меня», а не кого-то ещё).
	bool EnemyIsFacingMe()
	{
		return m_Target && m_Target.IsFacingMe(GetOwner().GetPosition());
	}

	//! Угол корпуса к врагу позволяет блок (гашение по EvaluateHit_Player).
	bool AngleAllowsBlock()
	{
		return Math.AbsFloat(AngleToTarget()) <= DM_MELEE_BLOCK_ANGLE;
	}

	//! Держать в руках целый мили-предмет: если в руках не мили (или мили сломан),
	//! собрать цепочку «stash текущего → PUTINTOHANDS мили» и поставить в очередь.
	//! Re-runnable каждый тик — так бот меняет сломанный мили на целый без пересоздания стейта.
	void EnsureMeleeEquipped(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		if (m_EquipRoot)
		{
			if (!m_EquipRoot.IsAllFinished())
				return;              // цепочка ещё выполняется
			m_EquipRoot = null;      // цепочка завершена
		}

		EntityAI inHands = pawn.GetItemInHands();
		if (inHands && bot.EntityIsMelee(inHands) && !inHands.IsRuined())
			return;                  // уже целый мили в руках

		EntityAI melee = bot.SelectMeleeWeapon();
		if (!melee)
			return;                  // мили нет — кулаки

		dmInventoryFrame equip = dmInventoryFrame.Make(dmInventoryDoing.PUTINTOHANDS, ItemBase.Cast(melee));

		dmInventoryFrame root = equip;
		if (inHands && inHands != melee)
		{
			ItemBase current = ItemBase.Cast(inHands);
			if (current.IsRuined())
			{
				root = dmInventoryFrame.Make(dmInventoryDoing.PLACEONGROUND, current);
				root.m_OnSuccess = equip;
			}
			else
			{
				EntityAI bag = pawn.GetInventory().FindAttachment(InventorySlots.GetSlotIdFromString("Back"));
				if (bag)
				{
					dmInventoryFrame stash = dmInventoryFrame.Make(dmInventoryDoing.TAKEINTOCARGO, current, -1, bag);
					dmInventoryFrame drop = dmInventoryFrame.Make(dmInventoryDoing.PLACEONGROUND, current);
					stash.m_OnSuccess = equip;
					stash.m_OnFail = drop;
					drop.m_OnSuccess = equip;
					drop.m_OnFail = equip;
					root = stash;
				}
				else
				{
					root = dmInventoryFrame.Make(dmInventoryDoing.PLACEONGROUND, current);
					root.m_OnSuccess = equip;
				}
			}
		}

		pawn.GetInventoryFrames().Enqueue(root);
		m_EquipRoot = root;

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[FSM] Fighting: мили " + melee.GetType());
		#endif
	}

	//! Weapon reach (melee combat GetRange()) with a static fallback.
	float GetMeleeReach(dmAISurvivor bot)
	{
		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			dmBotMeleeCombat mc = dmBotMeleeCombat.Cast(pawn.GetMeleeCombat());
			if (mc)
				return mc.GetWeaponReach(InventoryItem.Cast(pawn.GetItemInHands()));
		}

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("[FSM] Fighting: нет dmBotMeleeCombat - fallback");
		#endif
		return DM_MELEE_REACH;
	}
}
