//! dmBotFSM — finite state machine container for a bot.
//!
//! Owned by a dmAISurvivor (the brain). Each tick Update() runs the current
//! state's OnUpdate(); when it returns EXIT, SelectTransition() rolls the state's
//! outgoing transitions (weighted random) after filtering by the destination's
//! CanEnter() and each transition's Guard().

class dmBotFSM
{
	private ref dmAISurvivor m_Owner;
	private ref array<ref dmBotState> m_States;
	private ref dmBotState m_CurrentState;
	private string m_DefaultState;

	void dmBotFSM(dmAISurvivor owner)
	{
		m_Owner = owner;
		m_States = new array<ref dmBotState>();
	}

	dmAISurvivor GetOwner()
	{
		return m_Owner;
	}

	void AddState(dmBotState state, string name)
	{
		state.SetFSM(this);
		state.SetName(name);
		m_States.Insert(state);
	}

	void SetDefaultState(string name)
	{
		m_DefaultState = name;
	}

	dmBotState GetState(string name)
	{
		for (int i = 0; i < m_States.Count(); i++)
		{
			if (m_States[i].GetName() == name)
				return m_States[i];
		}
		return null;
	}

	dmBotState GetCurrentState()
	{
		return m_CurrentState;
	}

	//! Enter a state by name (default state when name is empty).
	bool Start(string name = "")
	{
		dmBotState dst;
		if (name != "")
			dst = GetState(name);
		else if (m_DefaultState != "")
			dst = GetState(m_DefaultState);
		else if (m_States.Count() > 0)
			dst = m_States[0];

		if (!dst)
			return false;

		dmBotState src = m_CurrentState;
		if (src && src != dst)
			src.OnExit(dst);

		m_CurrentState = dst;
		dst.OnEntry(src);
		return true;
	}

	//! Advance one tick. Called by the bot's OnUpdate.
	void Update(float pDt)
	{
		if (!m_CurrentState)
		{
			Start();
			return;
		}

		if (m_CurrentState.OnUpdate(pDt) != dmBotState.EXIT)
			return;

		dmBotState dst;
		if (SelectTransition(dst))
		{
			dmBotState src = m_CurrentState;
			src.OnExit(dst);
			m_CurrentState = dst;
			dst.OnEntry(src);
		}
	}

	//! Weighted-random pick among eligible transitions of the current state.
	private bool SelectTransition(out dmBotState dst)
	{
		int i;
		ref array<ref dmBotTransition> transitions = m_CurrentState.GetTransitions();
		ref array<ref dmBotTransition> eligible = new array<ref dmBotTransition>();
		float total = 0.0;

		for (i = 0; i < transitions.Count(); i++)
		{
			dmBotTransition t = transitions[i];
			dmBotState to = t.GetDestination();

			if (!to || !to.CanEnter())           // состояние неактуально
				continue;
			if (!t.Guard(m_Owner))               // ребро заблокировано/не разрешено
				continue;
			if (t.GetWeight() <= 0.0)            // отключён
				continue;

			eligible.Insert(t);
			total += t.GetWeight();
		}

		if (eligible.Count() == 0 || total <= 0.0)
			return false;

		float r = Math.RandomFloat01() * total;  // [0, total)
		float acc = 0.0;
		for (i = 0; i < eligible.Count(); i++)
		{
			acc += eligible[i].GetWeight();
			if (r < acc)
			{
				dst = eligible[i].GetDestination();
				return true;
			}
		}

		//! Защита от float-погрешности (r почти равен total).
		dst = eligible[eligible.Count() - 1].GetDestination();
		return true;
	}
}
