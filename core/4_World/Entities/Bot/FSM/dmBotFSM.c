//! dmBotFSM — finite state machine container for a bot.
//!
//! Owned by a dmAISurvivor (the brain). Hybrid transition model:
//!  - cooperative: when the current state's OnUpdate returns EXIT, roll a weighted
//!    random transition among its eligible outgoing transitions.
//!  - preemptive: when the current state is INTERRUPTIBLE, periodically (every
//!    s_PreemptInterval) check transitions to PREEMPTIVE states whose guards are
//!    open; if any, a weighted random pick MUST transition (evict the state).

class dmBotFSM
{
	private ref dmAISurvivor m_Owner;
	private ref array<ref dmBotState> m_States;
	private ref dmBotState m_CurrentState;
	private string m_DefaultState;
	private float m_PreemptTimer = 0.0;

	//! Global preemption evaluation interval (seconds).
	static float s_PreemptInterval = DM_FSM_PREEMPT_INTERVAL;

	static void SetPreemptInterval(float seconds)
	{
		s_PreemptInterval = seconds;
	}

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
		int i;
		for (i = 0; i < m_States.Count(); i++)
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

		TransitionTo(dst);
		return true;
	}

	//! Advance one tick. Called by the bot's OnUpdate.
	void Update(float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("FSM.Update");
		#endif

		if (!m_CurrentState)
		{
			Start();
			return;
		}

		dmBotState dst;

		if (m_CurrentState.OnUpdate(pDt) == dmBotState.EXIT)
		{
			if (!SelectTransition(dst))
				dst = FallbackState();
			if (dst)
				TransitionTo(dst);
			return;
		}

		if (m_CurrentState.GetKind() == dmBotStateKind.INTERRUPTIBLE)
		{
			m_PreemptTimer += pDt;
			if (m_PreemptTimer >= s_PreemptInterval)
			{
				m_PreemptTimer = 0.0;
				if (SelectPreemptive(dst))
					TransitionTo(dst);
			}
		}
	}

	//! Change the current state (OnExit old, clear FSM intents, OnEntry new).
	private void TransitionTo(dmBotState dst)
	{
		dmBotState src = m_CurrentState;
		if (src && src != dst)
		{
			src.OnExit(dst);
			m_Owner.ClearFSMIntents();
		}

		m_CurrentState = dst;
		m_PreemptTimer = 0.0;
		dst.OnEntry(src);

		#ifdef DM_BOT_DEBUG_FSM
		if (src && src != dst)
			dmBotLog.Debug("[FSM] transition " + src.GetName() + " -> " + dst.GetName());
		else
			dmBotLog.Debug("[FSM] enter " + dst.GetName());
		#endif
	}

	//! Default state (Idle) when no eligible transition exists — safety net.
	private dmBotState FallbackState()
	{
		dmBotState dst = null;
		if (m_DefaultState != "")
			dst = GetState(m_DefaultState);
		if (!dst && m_States.Count() > 0)
			dst = m_States[0];
		return dst;
	}

	//! Weighted-random pick among eligible transitions (cooperative EXIT).
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
			if (!to || !to.CanEnter())
				continue;
			if (!t.Guard(m_Owner))
				continue;
			if (t.GetWeight() <= 0.0)
				continue;
			eligible.Insert(t);
			total += t.GetWeight();
		}

		return RollWeighted(eligible, total, dst);
	}

	//! Weighted-random pick among transitions to PREEMPTIVE states (preemption).
	private bool SelectPreemptive(out dmBotState dst)
	{
		int i;
		ref array<ref dmBotTransition> transitions = m_CurrentState.GetTransitions();
		ref array<ref dmBotTransition> eligible = new array<ref dmBotTransition>();
		float total = 0.0;

		for (i = 0; i < transitions.Count(); i++)
		{
			dmBotTransition t = transitions[i];
			dmBotState to = t.GetDestination();
			if (!to || to.GetKind() != dmBotStateKind.PREEMPTIVE)
				continue;
			if (!to.CanEnter())
				continue;
			if (!t.Guard(m_Owner))
				continue;
			if (t.GetWeight() <= 0.0)
				continue;
			eligible.Insert(t);
			total += t.GetWeight();
		}

		#ifdef DM_BOT_DEBUG_FSM
		if (eligible.Count() == 0)
			dmBotLog.Debug("[FSM] preemptive из '" + m_CurrentState.GetName() + "': нет подходящего перехода");
		else
			dmBotLog.Debug("[FSM] preemptive из '" + m_CurrentState.GetName() + "': кандидатов " + eligible.Count());
		#endif

		return RollWeighted(eligible, total, dst);
	}

	//! Weighted random among a pre-filtered list (higher weight = more likely).
	private bool RollWeighted(array<ref dmBotTransition> eligible, float total, out dmBotState dst)
	{
		if (eligible.Count() == 0 || total <= 0.0)
			return false;

		float r = Math.RandomFloat01() * total;  // [0, total)
		float acc = 0.0;
		int i;
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
