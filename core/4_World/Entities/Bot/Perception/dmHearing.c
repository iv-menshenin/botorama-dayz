//! dmHearing — perception (hearing): receives dmNoiseSystem signals and updates
//! the bot's target memory by noise (distance filter -> hierarchy root -> HearNoise).

class dmHearing
{
	ref dmAISurvivor m_Bot;

	void dmHearing(dmAISurvivor bot)
	{
		m_Bot = bot;
		dmNoiseSystem.SI_OnNoiseAdded.Insert(OnNoise);
	}

	//! Обработчик шума: фильтр дистанции → резолв корня → обновить/добавить цель.
	void OnNoise(EntityAI source, vector position, float strength)
	{
		if (!m_Bot || !m_Bot.IsSpawned() || !source)
			return;

		vector botPos = m_Bot.GetPosition();
		vector d = position - botPos;
		if (d.LengthSq() > strength * strength)
			return;

		EntityAI root = source.GetHierarchyRootPlayer();
		if (!root)
			root = source;

		m_Bot.HearNoise(root, position);
	}
};
