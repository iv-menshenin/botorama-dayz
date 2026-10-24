//! dmBotCondition_ImminentThreat — true when a known hostile target is closer than
//! its danger distance (type-aware: zombie vs player/health-based). Gates the
//! MedicalCare -> Fighting/Shooting escape (mutually exclusive with MedicalCare()).
class dmBotCondition_ImminentThreat : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.HasImminentThreat();
	}
}
