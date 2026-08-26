//! dmBotState — base class for bot FSM states.
//!
//! Concrete states override OnEntry/OnExit/OnUpdate (and CanEnter for relevance).
//! The FSM reference and name are wired up by dmBotFSM.AddState(state, name).
//! A state owns its outgoing transitions; AddTransition is fluent.

class dmBotState
{
	static const int EXIT = 0;
	static const int CONTINUE = 1;

	private string m_Name;
	private ref dmBotFSM m_FSM;
	private ref array<ref dmBotTransition> m_Transitions;

	void dmBotState()
	{
		m_Transitions = new array<ref dmBotTransition>();
	}

	//! Called by dmBotFSM.AddState.
	void SetFSM(dmBotFSM fsm)
	{
		m_FSM = fsm;
	}

	//! Called by dmBotFSM.AddState.
	void SetName(string name)
	{
		m_Name = name;
	}

	string GetName()
	{
		return m_Name;
	}

	dmAISurvivor GetOwner()
	{
		return m_FSM.GetOwner();
	}

	dmBotState GetState(string name)
	{
		return m_FSM.GetState(name);
	}

	//! Add an outgoing transition with a weight. Returns it for fluent chaining
	//! (e.g. .BlockWhen(...)).
	dmBotTransition AddTransition(dmBotState to, float weight)
	{
		dmBotTransition t = new dmBotTransition(to, weight);
		m_Transitions.Insert(t);
		return t;
	}

	ref array<ref dmBotTransition> GetTransitions()
	{
		return m_Transitions;
	}

	//! Lifecycle (override in concrete states).
	void OnEntry(dmBotState from) {}
	void OnExit(dmBotState to) {}
	int OnUpdate(float pDt) { return CONTINUE; }

	//! Relevance guard: return false to block every transition INTO this state.
	bool CanEnter() { return true; }
}
