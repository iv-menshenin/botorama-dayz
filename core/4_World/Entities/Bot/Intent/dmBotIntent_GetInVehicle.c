//! dmBotIntent_GetInVehicle — walk to a vehicle's crew entry point and board it.
//!
//! It inherits the full path-following machinery from dmBotIntent_MoveTo (steering,
//! stuck detection, vault/climb, ladders, recovery) and aims the route at the seat's
//! entry point (Transport.CrewEntryWS). DM_GETIN_REACH is the reach distance at
//! which the entry point counts as reached. OnReachedGoal starts the vanilla
//! vehicle command (pawn.GetInVehicle) and finishes — the vehicle command then owns
//! the body on the engine side; on failure it fails the intent.
class dmBotIntent_GetInVehicle : dmBotIntent_MoveTo
{
	Transport m_Transport;
	int m_Seat = -1;
	float m_ReadyToSit = 0.0;
	float m_CommandFinishWait = 0.0;

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

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.GetInVehicle");
		#endif

		if ( !m_CommandInvoked )
		{
			super.OnUpdate(bot, pDt);
		}

		if ( m_ReadyToSit > 0.0 )
		{
			m_ReadyToSit -= pDt;
			if ( m_ReadyToSit > 0.0 ) return;
			m_ReadyToSit = 0.0;

			if ( m_Pawn && m_Seat >= 0 && !m_CommandInvoked )
			{
				if (m_Pawn.GetInVehicle(m_Transport, m_Seat))
					m_CommandInvoked = true;
				else
					Fail();
			} else
				Fail();
			return;
		}

		if ( m_CommandFinishWait > 0.0 )
		{
			m_CommandFinishWait -= pDt;
			if ( m_CommandFinishWait < 0.0 ) m_CommandFinishWait = 0.0;
		}
	}

	override void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		bot.SetMove(0.0, 0.0);
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] GetInVehicle.OnReachedGoal pos=" + pos);
		#endif

		if (m_Transport && m_ReadyToSit == 0.0 && !m_CommandInvoked)
		{
			vector vehicleSitPos;
			vector vehicleSitDir;
			m_Transport.CrewEntryWS(m_Seat, vehicleSitPos, vehicleSitDir);
			m_Pawn.SetPosition(vehicleSitPos);
			m_Pawn.SetOrientation(Vector(vehicleSitDir.VectorToAngles()[0], 0.0, 0.0));
			m_ReadyToSit = 1.0;
		}
	}

	override void Finish()
	{
		if ( m_Pawn && m_CommandInvoked )
		{
			m_Pawn.GetOutVehicle();
			m_CommandFinishWait = 2.5;
		}
		super.Finish();
	}

	override void Fail()
	{
		if ( m_Pawn && m_CommandInvoked ) m_Pawn.GetOutVehicle();
		super.Fail();
	}

	override bool IsFinished()
	{
		if ( m_CommandFinishWait > 0.0 ) return false;

		return super.IsFinished();
	}

	override bool IsContinuous()
	{
		return true;
	}
}
