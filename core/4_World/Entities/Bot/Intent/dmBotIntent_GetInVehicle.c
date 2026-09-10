//! dmBotIntent_GetInVehicle — walk to a vehicle's crew entry point and board it,
//! then own the whole seating/exit lifecycle (open/close the car door around the
//! get-in/get-out animations).
//!
//! It inherits the full path-following machinery from dmBotIntent_MoveTo (steering,
//! stuck detection, vault/climb, ladders, recovery) and aims the route at the seat's
//! entry point (Transport.CrewEntryWS). DM_GETIN_REACH is the reach distance at
//! which the entry point counts as reached. OnReachedGoal opens the car door and a
//! phase machine drives the vanilla vehicle command (pawn.GetInVehicle / GetOutVehicle)
//! between the open/close door polls, so the door is always opened before the body
//! moves and closed after it settles.
class dmBotIntent_GetInVehicle : dmBotIntent_MoveTo
{
	static const int PHASE_WALK = 0;      // движемся к точке входа (MoveTo)
	static const int PHASE_OPEN_IN = 1;   // ждём открытия двери перед посадкой
	static const int PHASE_ENTERING = 2;  // ждём окончания get-in анимации
	static const int PHASE_SEATED = 3;    // сидим в машине (идл)
	static const int PHASE_OPEN_OUT = 4;  // ждём открытия двери перед выходом
	static const int PHASE_EXITING = 5;   // ждём детача (окончания get-out)
	static const int PHASE_CLOSING = 6;   // ждём закрытия двери после выхода
	static const int PHASE_DONE = 7;      // выход завершён

	Transport m_Transport;
	int m_Seat = -1;
	int m_Phase = PHASE_WALK;
	float m_PhaseTimer = 0.0;
	bool m_DoorOpened = false;
	float m_LastDoorLogTime = 0.0;

	dmAISurvivorBase m_Pawn;
	bool m_CommandInvoked = false;

	void dmBotIntent_GetInVehicle()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "GetInVehicle";
	}

	override void OnStart(dmAISurvivor bot)
	{
		vector entry;
		vector dir;
		if (m_Transport)
			m_Transport.CrewEntryWS(m_Seat, entry, dir);
		else
			entry = bot.GetPosition();

		m_Pawn = bot.GetPawn();
		m_Goal = entry;
		m_ReachDistance = DM_GETIN_REACH;
		super.OnStart(bot);
	}

	//! Animation-source name of this seat's door, or "" when there is no door
	//! (no CarScript, no door selection, or the door is missing).
	string GetCarDoorAnimSource()
	{
		CarScript car = CarScript.Cast(m_Transport);
		if (!car)
			return "";

		string sel = car.GetDoorSelectionNameFromSeatPos(m_Seat);
		if (sel == "")
			return "";

		string slot = car.GetDoorInvSlotNameFromSeatPos(m_Seat);
		if (car.GetCarDoorsState(slot) == CarDoorState.DOORS_MISSING)
			return "";

		return car.GetAnimSourceFromSelection(sel);
	}

	//! Open/close the car door (animation phase 1.0/0.0) and remember the state.
	void SetCarDoorOpen(bool open)
	{
		CarScript car = CarScript.Cast(m_Transport);
		string anim = GetCarDoorAnimSource();
		if (car && anim != "")
		{
			float phase = 0.0;
			if (open)
				phase = 1.0;
			car.SetAnimationPhase(anim, phase);

			#ifdef DM_BOT_DEBUG_FSM
			float now = GetGame().GetTickTime();
			if (now - m_LastDoorLogTime >= 2.0)
			{
				m_LastDoorLogTime = now;
				if (open)
					dmBotLog.Debug("[FSM] GetInVehicle: door open seat=" + m_Seat);
				else
					dmBotLog.Debug("[FSM] GetInVehicle: door close seat=" + m_Seat);
			}
			#endif
		}
		m_DoorOpened = open;
	}

	//! True when the door is at the requested state. A missing door is always
	//! considered "at phase" (nothing to wait for).
	bool IsCarDoorAtPhase(bool open)
	{
		string anim = GetCarDoorAnimSource();
		if (anim == "")
			return true;

		CarScript car = CarScript.Cast(m_Transport);
		if (!car)
			return true;

		float phase = car.GetAnimationPhase(anim);
		if (open)
			return phase >= DM_CAR_DOOR_OPEN_PHASE;
		return phase <= DM_CAR_DOOR_CLOSED_PHASE;
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.GetInVehicle");
		#endif

		if ( !m_CommandInvoked )
		{
			super.OnUpdate(bot, pDt);
		}

		if ( m_Phase == PHASE_OPEN_IN )
		{
			m_PhaseTimer -= pDt;
			if ( IsCarDoorAtPhase(true) || m_PhaseTimer <= 0.0 )
			{
				if ( m_Pawn && m_Seat >= 0 )
				{
					if ( m_Pawn.GetInVehicle(m_Transport, m_Seat) )
						m_CommandInvoked = true;
					else
						Fail();
				}
			else
				Fail();
			m_Phase = PHASE_ENTERING;
			m_PhaseTimer = DM_CAR_DOOR_ANIM_TIMEOUT;
		}
		return;
	}

	if ( m_Phase == PHASE_ENTERING )
	{
		m_PhaseTimer -= pDt;
		HumanCommandVehicle cmd = null;
		if ( m_Pawn )
			cmd = m_Pawn.GetCommand_Vehicle();
		if ( (cmd && !cmd.IsGettingIn()) || m_PhaseTimer <= 0.0 )
		{
			SetCarDoorOpen(false);
			m_Phase = PHASE_SEATED;
		}
		return;
	}

	if ( m_Phase == PHASE_SEATED )
	{
		return;
	}

	if ( m_Phase == PHASE_OPEN_OUT )
	{
		m_PhaseTimer -= pDt;
		if ( IsCarDoorAtPhase(true) || m_PhaseTimer <= 0.0 )
		{
			if ( m_Pawn )
				m_Pawn.GetOutVehicle();
			m_Phase = PHASE_EXITING;
			m_PhaseTimer = DM_CAR_DOOR_ANIM_TIMEOUT;
		}
		return;
	}

	if ( m_Phase == PHASE_EXITING )
	{
		m_PhaseTimer -= pDt;
		bool detached = true;
		if ( m_Pawn && m_Pawn.GetCommand_Vehicle() )
			detached = false;
		if ( detached || m_PhaseTimer <= 0.0 )
		{
			SetCarDoorOpen(false);
			m_Phase = PHASE_CLOSING;
			m_PhaseTimer = DM_CAR_DOOR_TIMEOUT;
		}
		return;
	}

	if ( m_Phase == PHASE_CLOSING )
	{
		m_PhaseTimer -= pDt;
		if ( IsCarDoorAtPhase(false) || m_PhaseTimer <= 0.0 )
		{
			m_Phase = PHASE_DONE;
		}
		return;
	}
	}

	override void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		bot.SetMove(0.0, 0.0);
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] GetInVehicle.OnReachedGoal pos=" + pos);
		#endif

		if (m_Transport && m_Phase == PHASE_WALK && !m_CommandInvoked)
		{
			vector vehicleSitPos;
			vector vehicleSitDir;
			m_Transport.CrewEntryWS(m_Seat, vehicleSitPos, vehicleSitDir);
			m_Pawn.SetPosition(vehicleSitPos);
			m_Pawn.SetOrientation(Vector(vehicleSitDir.VectorToAngles()[0], 0.0, 0.0));
			SetCarDoorOpen(true);
			m_Phase = PHASE_OPEN_IN;
			m_PhaseTimer = DM_CAR_DOOR_TIMEOUT;
		}
	}

	//! Start the get-out flow: open the door, then let the phase machine drive
	//! GetOutVehicle and the closing poll. GetOutVehicle is NOT called here — it
	//! happens in the OPEN_OUT phase once the door is open.
	override void Finish()
	{
		if ( m_Pawn && m_CommandInvoked && m_Phase == PHASE_SEATED )
		{
			SetCarDoorOpen(true);
			m_Phase = PHASE_OPEN_OUT;
			m_PhaseTimer = DM_CAR_DOOR_TIMEOUT;
		}
		super.Finish();
	}

	override void Fail()
	{
		if ( m_DoorOpened ) SetCarDoorOpen(false);
		if ( m_Pawn && m_CommandInvoked ) m_Pawn.GetOutVehicle();
		super.Fail();
	}

	//! The intent stays "not finished" through the whole exit flow (open door →
	//! get out → close door), even though Finish() already flagged it.
	override bool IsFinished()
	{
		if ( m_Phase == PHASE_OPEN_OUT || m_Phase == PHASE_EXITING || m_Phase == PHASE_CLOSING )
			return false;

		return super.IsFinished();
	}

	//! Once the bot is seated (m_CommandInvoked) the intent lives through the
	//! whole vehicle occupation — the auto-deadline (DM_INTENT_MAX_AGE) must not
	//! kill it while the bot is inside.
	override bool IsExpired()
	{
		if ( m_CommandInvoked )
			return false;

		return super.IsExpired();
	}

	override void OnCancel(dmAISurvivor bot)
	{
		if ( m_DoorOpened ) SetCarDoorOpen(false);
		super.OnCancel(bot);
	}

	override bool IsContinuous()
	{
		return true;
	}
}
