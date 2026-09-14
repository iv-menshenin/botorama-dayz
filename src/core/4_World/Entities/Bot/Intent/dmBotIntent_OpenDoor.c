//! dmBotIntent_OpenDoor — open a closed door cleanly (EXCLUSIVE, MOVE channel).
//!
//! While it runs (EXCLUSIVE) it owns the MOVE channel, so parallel movement
//! intents (MoveTo/FollowTo) go dormant and don't shove the bot into the door.
//! Lifecycle: back away from the door to a safe distance (so the swinging door
//! doesn't push the bot), then OpenDoor + hand animation, then wait until the
//! door is fully open.

//! Trivial callback type for the door-open hand animation. The instance is created
//! by Human.AddCommandModifier_Action(typename), so this is only a type marker. The
//! explicit empty constructor is required because the base has a private constructor.
class dmBotActionAnimCB : HumanCommandActionCallback
{
	void dmBotActionAnimCB()
	{
	}
}

class dmBotIntent_OpenDoor : dmBotIntent
{
	Building m_Building;
	int m_DoorIdx;
	vector m_StartPos;
	int m_Phase;          // 0 = back away, 1 = wait for open
	float m_PhaseTimer;
	HumanCommandActionCallback m_ActionCB;

	void dmBotIntent_OpenDoor()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "OpenDoor";
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);

		if (!m_Building)
		{
			Finish();
			return;
		}

		m_Phase = 0;
		m_PhaseTimer = DM_DOOR_STEP_BACK_TIMEOUT;
		m_ActionCB = null;
		m_StartPos = bot.GetPosition();
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		if (m_Phase == 0)
		{
			vector botPos = bot.GetPosition();
			vector d = botPos - m_StartPos;
			d[1] = 0.0;
			float dist = d.Length();
			m_PhaseTimer -= pDt;

			if (dist >= DM_DOOR_STEP_BACK_DIST || m_PhaseTimer <= 0.0)
			{
				m_Building.OpenDoor(m_DoorIdx);
				bot.SetMove(0.0, 0.0);   // стоп: не пятиться, пока дверь открывается

				//! Hand animation: additive upper-body modifier over MOVE
				//! (CMD_ACTIONMOD_OPENDOORFW). Gate: skip if a full-body action or
				//! another modifier is already active (raise/melee/climb/ladder).
				dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
				if (pawn && !pawn.GetCommandModifier_Action() && !pawn.GetCommand_Action())
					m_ActionCB = pawn.AddCommandModifier_Action(DayZPlayerConstants.CMD_ACTIONMOD_OPENDOORFW, dmBotActionAnimCB);

				m_Phase = 1;
				m_PhaseTimer = DM_DOOR_OPEN_TIMEOUT;

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[Bot] OpenDoor: open doorIdx=" + m_DoorIdx);
				#endif
				return;
			}

			bot.SetMove(180.0, DM_DOOR_OPEN_STEPBACK_SPEED);
			return;
		}

		//! Wait for the door to fully open (or the timeout).
		m_PhaseTimer -= pDt;
		if (m_Building.IsDoorOpened(m_DoorIdx) || m_PhaseTimer <= 0.0)
		{
			StopAction(bot);
			Finish();
		}
	}

	override void OnCancel(dmAISurvivor bot)
	{
		super.OnCancel(bot);

		StopAction(bot);
		bot.SetMove(0.0, 0.0);
	}

	//! Remove the door-open hand-animation modifier (if still active). Guarded so a
	//! one-shot animation that already auto-finished doesn't leave a dangling ref.
	void StopAction(dmAISurvivor bot)
	{
		if (!m_ActionCB) return;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn && pawn.GetCommandModifier_Action() == m_ActionCB)
			pawn.DeleteCommandModifier_Action(m_ActionCB);
		m_ActionCB = null;
	}
}
