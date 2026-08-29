//! dmBotState_Idle — stand still and scan around (head, optionally body). Does
//! NOT touch the follow target: it just idles until the FSM transitions away.
class dmBotState_Idle : dmBotState
{
	ref dmBotIntent_LookAround m_Scan;
	float m_TotalTimer;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.INTERRUPTIBLE;
	}

	override void OnEntry(dmBotState from)
	{
		m_Scan = null;
		m_TotalTimer = 0.0;
		CreateScan();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Idle.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		if (m_Scan && (m_Scan.IsFinished() || m_Scan.IsExpired()))
			m_Scan = null;
		if (!m_Scan)
			CreateScan();

		m_TotalTimer += pDt;
		if (m_TotalTimer >= 300.0)
			return EXIT;

		return CONTINUE;
	}

	void CreateScan()
	{
		m_Scan = new dmBotIntent_LookAround();
		m_Scan.m_AllowBodyTurn = true;
		m_Scan.m_Turn = dmBotLookTurn.NONE;
		m_Scan.m_Priority = dmBotIntentPriority.DESIRABLE;
		GetOwner().AddFSMIntent(m_Scan);
	}
}
