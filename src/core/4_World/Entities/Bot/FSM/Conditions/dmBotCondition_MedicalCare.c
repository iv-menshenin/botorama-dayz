//! dmBotCondition_MedicalCare — true when the bot has no imminent threat and has
//! the medication it needs (bandage/charcoal/tetracycline/vitamins; a splint is
//! always available via spawn). Gates the MedicalCare state. Mutually exclusive
//! with ImminentThreat() (anti-oscillation between fight/shoot and medical).
class dmBotCondition_MedicalCare : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		if (bot.HasImminentThreat())
			return false;
		return bot.AreNecessaryMedicationsAvailable();
	}
}
