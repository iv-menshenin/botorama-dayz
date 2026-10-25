//! dmBotState_Rushing — rush a hostile target that is beyond weapon range.
//!
//! Inherits target resolution, aim-mode selection, sprint and combat entry from
//! dmBotState_Shooting, but adds three phases on top of it (driven by the target):
//!   - фаза 1 (стрельба): цель видима И в досягаемости оружия -> super.OnUpdate
//!     (прицел + огонь, как в Shooting);
//!   - фаза 3 (уворот): цель видима И смотрит на бота -> EvadeAim (личный интент);
//!   - фаза 2 (подход): иначе -> MoveTo к последней известной позиции цели,
//!     подъём оружия (sticky) при подходе ближе DM_RUSH_RAISE_DIST к цели движения.
//! EXITs when the target disappears (yields back to Flanking/Shooting/Idle via the
//! preset transitions).
class dmBotState_Rushing : dmBotState_Shooting
{
	ref dmBotIntent_MoveTo m_Move;

	override dmBotCondition CreateGate()
	{
		return dmBotConditions.Rushing(true);
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		if (!m_TargetEntity || !m_TargetEntity.IsAlive()) ResolveTarget();
		if (!m_TargetEntity) return EXIT;

		dmTarget t = bot.FindTarget(m_TargetEntity);
		if (!t)
		{
			//! Цель забыта (выпала из m_Targets), но сущность жива — пере-резолв.
			ResolveTarget();
			if (!m_TargetEntity) return EXIT;
			t = bot.FindTarget(m_TargetEntity);
			if (!t) return EXIT;
		}

		bool visible = t.m_HasLOS;
		bool inRange = bot.CanReach(t);

		// фаза 1: видима + в досягаемости → стрельба
		if (visible && inRange)
		{
			if (m_Move) { m_Move.Finish(); m_Move = null; }
			return super.OnUpdate(pDt);
		}

		// вне стрельбы — снять стрелковые интенты (если были в фазе 1)
		FinishShootingIntents();

		// фаза 3: видима + смотрит на бота → уворот
		if (visible && bot.IsTargetFacingBot(t))
		{
			if (m_Move) { m_Move.Finish(); m_Move = null; }
			dmAISurvivorBase pawn3 = dmAISurvivorBase.Cast(bot.GetPawn());
			if (pawn3) { pawn3.ClearWeaponHold(); pawn3.RaiseWeapon(false); }
			bot.EvadeAim(m_TargetEntity, true);
			return CONTINUE;
		}

		// фаза 2: подход
		EnsureMove(bot, t);
		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		if (m_Move) m_Move.Finish();
		super.OnExit(to); // Shooting.OnExit финишит Aim/HitTo/Look
	}

	//! Финиш стрелковых интентов при уходе из фазы 1 (прицел/удар/взгляд).
	void FinishShootingIntents()
	{
		if (m_Aim) { m_Aim.Finish(); m_Aim = null; }
		if (m_HitTo) { m_HitTo.Finish(); m_HitTo = null; }
		if (m_Look) { m_Look.Finish(); m_Look = null; }
	}

	//! MoveTo к последней известной позиции цели; репат при смещении цели > DM_RUSH_REPATH_DIST;
	//! подъём оружия (sticky) при подходе ближе DM_RUSH_RAISE_DIST к цели движения.
	void EnsureMove(dmAISurvivor bot, dmTarget t)
	{
		if (m_Move && (m_Move.IsFinished() || m_Move.IsExpired()))
			m_Move = null;

		if (m_Move)
		{
			vector goal = m_Move.m_Goal;
			vector newGoal = t.m_LastPosition;
			vector delta = newGoal - goal;
			delta[1] = 0.0;
			if (delta.Length() > DM_RUSH_REPATH_DIST)
			{
				m_Move.Finish();
				m_Move = null;
			}
		}

		if (!m_Move)
		{
			m_Move = new dmBotIntent_MoveTo();
			m_Move.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			m_Move.m_Goal = t.m_LastPosition;
			bot.AddFSMIntent(m_Move);
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
		{
			if (vector.Distance(bot.GetPosition(), m_Move.m_Goal) < DM_RUSH_RAISE_DIST)
				pawn.RaiseWeaponSticky(DM_CAMP_HOLD_SECONDS);
			else
			{
				pawn.ClearWeaponHold();
				pawn.RaiseWeapon(false);
			}
		}
	}
}
