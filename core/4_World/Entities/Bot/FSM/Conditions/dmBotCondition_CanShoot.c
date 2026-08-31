//! dmBotCondition_CanShoot — true when the bot has a firearm in hands AND ammo
//! for it (i.e. it should engage at range, not melee).
class dmBotCondition_CanShoot : dmBotCondition
{
	override bool Evaluate(dmAISurvivor bot)
	{
		return bot.HasFirearmInHands() && !bot.HasNoAmmo();
	}
}
