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

	static dmBotCondition HasHostile()
	{
		return new dmBotCondition_HasTarget(false, true);
	}

	static dmBotCondition HasUnknownTarget()
	{
		return new dmBotCondition_HasTarget(true, false);
	}

	static dmBotCondition CanShoot()
	{
		return new dmBotCondition_CanShoot();
	}

	static dmBotCondition MedicalCare()
	{
		return new dmBotCondition_MedicalCare();
	}

	static dmBotCondition Thirsty()
	{
		return new dmBotCondition_Thirsty();
	}

	static dmBotCondition LastPositionSpreadLessOrEqual(float spread)
	{
		return new dmBotCondition_LastPositionSpread(spread, true);
	}

	static dmBotCondition LastPositionSpreadGreatOrEqual(float spread)
	{
		return new dmBotCondition_LastPositionSpread(spread, false);
	}
}
