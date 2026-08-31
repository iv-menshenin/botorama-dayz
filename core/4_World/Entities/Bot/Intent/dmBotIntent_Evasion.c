//! dmBotIntent_Evasion — strafe sideways while the strike cooldown recharges.
//!
//! CRITICAL + PARALLEL. Active only while the strike cooldown is > 0. It flips the
//! strafe direction every DM_MELEE_EVADE_SWITCH_TIME seconds; the body stays
//! turned toward the target via the Fighting state's HoldLook (FULL).
class dmBotIntent_Evasion : dmBotIntent
{
	EntityAI m_TargetEntity;
	float m_Sign = 1.0;
	float m_SwitchTimer = 0.0;

	void dmBotIntent_Evasion()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "Evasion";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		if (bot.GetMeleeCooldown() <= 0.0)
			return;

		m_SwitchTimer += pDt;
		if (m_SwitchTimer >= DM_MELEE_EVADE_SWITCH_TIME)
		{
			m_SwitchTimer = 0.0;
			m_Sign = -m_Sign;
		}

		bot.SetMove(m_Sign * 90.0, DM_MELEE_EVADE_SPEED);
	}
}
