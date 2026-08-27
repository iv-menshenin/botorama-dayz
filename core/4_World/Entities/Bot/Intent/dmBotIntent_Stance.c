//! dmBotIntent_Stance — hold a posture (stand / crouch / prone).
//!
//! Writes the desired stance to the brain's "stance" channel every tick. The
//! arbitration resets the channel to ERECT each tick (the background "stand"
//! intent), so the highest-priority stance intent wins; when it is removed
//! (deadline or FSM state exit), the bot automatically stands back up.
class dmBotIntent_Stance : dmBotIntent
{
	int m_Stance;

	void dmBotIntent_Stance()
	{
		m_Stance = DayZPlayerConstants.STANCEIDX_CROUCH;
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		bot.SetStance(m_Stance);
	}
}
