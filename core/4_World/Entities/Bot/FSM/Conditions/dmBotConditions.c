//! dmBotConditions — static factories for the built-in leaf conditions.
//!
//! Kept separate from dmBotCondition so the base class/file stays lean. Extend
//! this factory when adding new built-in conditions.

class dmBotConditions
{
	static dmBotCondition LowHealth()
	{
		return new dmBotCondition_LowHealth();
	}

	static dmBotCondition NoAmmo()
	{
		return new dmBotCondition_NoAmmo();
	}

	static dmBotCondition HasPlayerSigns()
	{
		return new dmBotCondition_HasPlayerSigns();
	}

	static dmBotCondition FollowFar()
	{
		return new dmBotCondition_FollowFar();
	}

	static dmBotCondition ThreatInRange()
	{
		return new dmBotCondition_ThreatInRange();
	}

	static dmBotCondition DefendInRange()
	{
		return new dmBotCondition_DefendInRange();
	}
}
