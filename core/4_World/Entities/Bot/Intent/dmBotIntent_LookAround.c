//! dmBotIntent_LookAround — stand and periodically look in different directions.
//!
//! IMPORTANT: it re-asserts the current look direction EVERY tick. The arbitration
//! resets the look channel to forward each tick (LookForward), so a look intent
//! must re-write it every tick (like HoldLook) — otherwise the head snaps back to
//! center between direction changes.
//!
//! On a random interval it rolls a direction: forward, a held head turn (±angle),
//! or (only when m_AllowBodyTurn) a full body turn via LookAtYaw(FULL).
class dmBotIntent_LookAround : dmBotIntent
{
	bool m_AllowBodyTurn = false;
	dmBotLookTurn m_Turn = dmBotLookTurn.AUTO;

	float m_Interval = 10.0;
	float m_Timer = 0.0;

	float m_CurYaw = 0.0;
	float m_HoldTimer = 0.0;
	float m_HoldDuration = 0.0;
	float m_BodyTarget = 0.0;
	int m_Phase = 0;   // 0 = forward, 1 = head hold, 2 = body turn

	override void OnStart(dmAISurvivor bot)
	{
		m_Timer = 0.0;
		m_Interval = RandomInterval();
		m_CurYaw = 0.0;
		m_HoldTimer = 0.0;
		m_HoldDuration = 0.0;
		m_BodyTarget = 0.0;
		m_Phase = 0;
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		if (m_Phase == 1)
		{
			m_HoldTimer -= pDt;
			if (m_HoldTimer <= 0.0)
			{
				m_CurYaw = 0.0;
				m_Phase = 0;
			}
			bot.LookAtDirection(m_CurYaw, 0.0, m_Turn);
			return;
		}

		if (m_Phase == 2)
		{
			bot.LookAtYaw(m_BodyTarget, dmBotLookTurn.FULL);
			if (Math.AbsFloat(bot.GetYawTo(m_BodyTarget)) <= 2.0)
			{
				m_CurYaw = 0.0;
				m_Phase = 0;
			}
			return;
		}

		m_Timer += pDt;
		if (m_Timer >= m_Interval)
		{
			m_Timer = 0.0;
			m_Interval = RandomInterval();
			PickEvent(bot);
		}

		bot.LookAtDirection(m_CurYaw, 0.0, m_Turn);
	}

	void PickEvent(dmAISurvivor bot)
	{
		int roll;
		if (m_AllowBodyTurn)
			roll = Math.RandomIntInclusive(0, 4);
		else
			roll = Math.RandomIntInclusive(0, 2);

		float sign;
		float bodyYaw;

		if (roll == 0)
		{
			m_CurYaw = 0.0;
			m_Phase = 0;
		}
		else if (roll == 1 || roll == 2)
		{
			sign = 1.0;
			if (roll == 2)
				sign = -1.0;
			m_CurYaw = sign * Math.RandomFloat(DM_SCAN_ANGLE_MIN, DM_SCAN_ANGLE_MAX);
			m_HoldDuration = Math.RandomFloat(DM_SCAN_HOLD_MIN, DM_SCAN_HOLD_MAX);
			m_HoldTimer = m_HoldDuration;
			m_Phase = 1;
		}
		else
		{
			bodyYaw = bot.GetOrientation()[0];
			sign = 1.0;
			if (roll == 4)
				sign = -1.0;
			m_BodyTarget = bodyYaw + sign * Math.RandomFloat(DM_SCAN_BODY_ANGLE_MIN, DM_SCAN_BODY_ANGLE_MAX);
			m_Phase = 2;
		}
	}

	float RandomInterval()
	{
		return Math.RandomFloatInclusive(DM_SCAN_INTERVAL_MIN, DM_SCAN_INTERVAL_MAX);
	}
}
