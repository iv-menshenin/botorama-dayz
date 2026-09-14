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
	void OnNoise(EntityAI source, vector position, float strength, int type)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return;

		vector botPos = m_Bot.GetPosition();
		vector d = position - botPos;
		if (d.LengthSq() > strength * strength)
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("[Noise] OnNoise too far: noice_position=" + position + " bot_position=" + m_Bot.GetPosition() + " strength=" + strength);
			#endif
			return;
		}

		//! Bullet impact / positional noise has no source entity — heard but no
		//! target to update.
		if (!source)
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("[Noise] OnNoise no target: noice_position=" + position + " bot_position=" + m_Bot.GetPosition() + " strength=" + strength);
			#endif
			return;
		}

		EntityAI root = source.GetHierarchyRootPlayer();
		if (!root)
			root = source;

		//! Ignore the bot's own noise (footsteps/shot) — don't target itself.
		if (root == m_Bot.GetPawn())
			return;

		//! Выстрел (любого стрелка) — регистрируем для уворота от прицела.
		if (type == dmNoiseType.SHOT)
			m_Bot.OnGunshot(root);

		float attractiveness = DM_NOISE_ATTRACTIVENESS_NOISE;
		if (type == dmNoiseType.SHOT || type == dmNoiseType.BULLETIMPACT)
			attractiveness = DM_NOISE_ATTRACTIVENESS_SHOT;

		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("[Noise] OnNoise I HEAR YOU: noice_position=" + position + " bot_position=" + m_Bot.GetPosition() + " strength=" + strength);
		#endif
		m_Bot.HearNoise(root, position, attractiveness);
	}
};
