//! dmBotCondition — composable, stateless predicate over a bot's state.
//!
//! Used as a transition guard (BlockWhen/Require) or inside a state's CanEnter().
//! Leaves override Evaluate(bot); composites And/Or/Not combine leaves without
//! subclass explosion (one class per primitive condition, not per transition).

class dmBotCondition
{
	//! True when the condition holds for the given bot. Override in leaves.
	bool Evaluate(dmAISurvivor bot)
	{
		return false;
	}

	dmBotCondition And(dmBotCondition other)
	{
		return new dmBotCondition_And(this, other);
	}

	dmBotCondition Or(dmBotCondition other)
	{
		return new dmBotCondition_Or(this, other);
	}

	dmBotCondition Not()
	{
		return new dmBotCondition_Not(this);
	}
}

class dmBotCondition_And : dmBotCondition
{
	private ref dmBotCondition m_A;
	private ref dmBotCondition m_B;

	void dmBotCondition_And(dmBotCondition a, dmBotCondition b)
	{
		m_A = a;
		m_B = b;
	}

	override bool Evaluate(dmAISurvivor bot)
	{
		return m_A.Evaluate(bot) && m_B.Evaluate(bot);
	}
}

class dmBotCondition_Or : dmBotCondition
{
	private ref dmBotCondition m_A;
	private ref dmBotCondition m_B;

	void dmBotCondition_Or(dmBotCondition a, dmBotCondition b)
	{
		m_A = a;
		m_B = b;
	}

	override bool Evaluate(dmAISurvivor bot)
	{
		return m_A.Evaluate(bot) || m_B.Evaluate(bot);
	}
}

class dmBotCondition_Not : dmBotCondition
{
	private ref dmBotCondition m_A;

	void dmBotCondition_Not(dmBotCondition a)
	{
		m_A = a;
	}

	override bool Evaluate(dmAISurvivor bot)
	{
		return !m_A.Evaluate(bot);
	}
}
