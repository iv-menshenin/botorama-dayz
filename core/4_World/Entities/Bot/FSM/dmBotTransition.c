//! dmBotTransition — a weighted edge between two states.
//!
//! Holds the destination (m_To) and a weight. Optionally carries block/require
//! conditions (dmBotCondition) that Gate() evaluates against the bot.

class dmBotTransition
{
	private ref dmBotState m_To;
	private float m_Weight;
	private ref dmBotCondition m_Block;     // блокирует, если Evaluate() == true
	private ref dmBotCondition m_Require;   // требует Evaluate() == true

	void dmBotTransition(dmBotState to, float weight)
	{
		m_To = to;
		m_Weight = weight;
	}

	dmBotState GetDestination()
	{
		return m_To;
	}

	float GetWeight()
	{
		return m_Weight;
	}

	//! Fluent: transition is excluded when the condition is met.
	dmBotTransition BlockWhen(dmBotCondition cond)
	{
		m_Block = cond;
		return this;
	}

	//! Fluent: transition is allowed only when the condition is met.
	dmBotTransition Require(dmBotCondition cond)
	{
		m_Require = cond;
		return this;
	}

	//! True when this edge is currently allowed.
	bool Guard(dmAISurvivor bot)
	{
		if (m_Block && m_Block.Evaluate(bot))
			return false;
		if (m_Require && !m_Require.Evaluate(bot))
			return false;
		return true;
	}
}
