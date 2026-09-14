//! dmBotCondition_MedicalCare — true when the bot is out of combat and has the
//! medication it needs (bandage/charcoal/tetracycline/vitamins; a splint is
//! always available via spawn). Gates the MedicalCare state.
class dmBotCondition_MedicalCare : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		if (bot.IsInCombat())
			return false;
		return bot.AreNecessaryMedicationsAvailable();
	}
}
