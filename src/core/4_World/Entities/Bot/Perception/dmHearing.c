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

		#ifdef DM_PERCEPTION_DEBUG
		string ssrc = "null";
		if (source) ssrc = source.GetType();
		dmBotLog.Debug(m_Bot.GetName(), "[Noise] OnNoise IN type=" + type + " src=" + ssrc + " pos=" + position);
		dmBotLog.Debug(m_Bot.GetName(), "[Noise] OnNoise IN strength=" + strength + " bot=" + m_Bot.GetPosition());
		#endif

		vector botPos = m_Bot.GetPosition();
		vector d = position - botPos;
		if (d.LengthSq() > strength * strength)
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug(m_Bot.GetName(), "[Noise] OnNoise too far: noice_position=" + position + " bot_position=" + m_Bot.GetPosition() + " strength=" + strength);
			#endif
			return;
		}

		//! Bullet impact / noise with a coordinate but no source entity — heard but no
		//! target to update.
		if (!source)
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug(m_Bot.GetName(), "[Noise] OnNoise no target: noice_position=" + position + " bot_position=" + m_Bot.GetPosition() + " strength=" + strength);
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

		//! Услышал чужой выстрел/взрыв — реплика HEARD_SHOT (с гейтами).
		if ((type == dmNoiseType.SHOT || type == dmNoiseType.EXPLOSION) && ShouldSayHeardShot(root, position))
			m_Bot.Say(dmVoiceCategory.HEARD_SHOT);

		//! Услышал попадание пули рядом (чужой выстрел) — реплика GOT_SHOT [1..2].
		if (type == dmNoiseType.BULLETIMPACT)
			m_Bot.Say(dmVoiceCategory.GOT_SHOT, 1, 2);

		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug(m_Bot.GetName(), "[Noise] OnNoise I HEAR YOU: noice_position=" + position + " bot_position=" + m_Bot.GetPosition() + " strength=" + strength);
		#endif

		//! Звук «летит» до бота: добавление таргета откладывается на dist / скорость звука.
		float delayMs = d.Length() / DM_SOUND_SPEED * 1000.0;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(HearNoiseDelayed, (int)delayMs, false, root, position, type);
	}

	//! Отложенное добавление таргета (CallLater) — пришёл момент, когда звук «долетел».
	void HearNoiseDelayed(EntityAI entity, vector position, int type)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return;
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug(m_Bot.GetName(), "[Noise] OnNoise sound arrived: position=" + position + " type=" + type);
		#endif
		m_Bot.HearNoise(entity, position, type);
	}

	//! Стоит ли говорить HEARD_SHOT: стрелок не идентифицирован, дальше 500 м, бот не в бою.
	bool ShouldSayHeardShot(EntityAI root, vector position)
	{
		dmTarget t = m_Bot.FindTarget(root);
		if (t && t.Identified())
			return false;
		if (vector.Distance(m_Bot.GetPosition(), position) < DM_HEARD_SHOT_MIN_DISTANCE)
			return false;
		if (m_Bot.IsInCombat())
			return false;
		return true;
	}
};
