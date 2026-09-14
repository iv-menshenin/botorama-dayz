//! dmBotIntentPool — a collection of active intents with simple lifecycle.
//!
//! Pure collection: it does not own OnStart (the bot does that on add). It ticks
//! ages, silently removes finished/expired intents (OnCancel cleanup), and can
//! clear everything (used on FSM state transition).

class dmBotIntentPool
{
	private ref array<ref dmBotIntent> m_Intents;

	void dmBotIntentPool()
	{
		m_Intents = new array<ref dmBotIntent>();
	}

	void Insert(dmBotIntent intent)
	{
		m_Intents.Insert(intent);
	}

	//! OnCancel each intent, then drop all (FSM transition).
	void Clear(dmAISurvivor bot)
	{
		int i;
		for (i = 0; i < m_Intents.Count(); i++)
			m_Intents[i].OnCancel(bot);
		m_Intents.Clear();
	}

	//! Tick ages; remove finished/expired intents (with OnCancel cleanup).
	void Tick(dmAISurvivor bot, float pDt)
	{
		int i = m_Intents.Count() - 1;
		while (i >= 0)
		{
			dmBotIntent intent = m_Intents[i];
			intent.TickAge(pDt);
			if (intent.IsFinished() || intent.IsExpired())
			{
				intent.OnCancel(bot);
				m_Intents.RemoveItem(intent);
			}
			i--;
		}
	}

	bool Has(dmBotIntent intent)
	{
		int i;
		for (i = 0; i < m_Intents.Count(); i++)
		{
			if (m_Intents[i] == intent)
				return true;
		}
		return false;
	}

	int Count()
	{
		return m_Intents.Count();
	}

	ref array<ref dmBotIntent> GetIntents()
	{
		return m_Intents;
	}
}
