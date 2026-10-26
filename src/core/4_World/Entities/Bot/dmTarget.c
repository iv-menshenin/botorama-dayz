//! dmTarget — the bot's goal target: memory + evaluation (survives FSM transitions).
//!
//! A target is what the bot wants to acquire or destroy. The bot may not know the
//! target's live position (out of visibility) — it then searches by m_LastPosition
//! or by m_ClassEntity (e.g. "BandageDressing" when m_Entity is null).
//!
//! Memory (T4): a target that hides behind a wall/brush stays in the list with its
//! last known position until it times out — m_LastPosition / m_HasLOS / m_LastContact.
//! Evaluation: m_Threat / m_Attractiveness / m_Friendly drive combat/loot choices.

enum dmTargetType
{
	ACQUIRE,   // завладеть (предмет)
	DESTROY    // уничтожить (существо/игрок)
};

class dmTarget
{
	dmTargetType m_Type = dmTargetType.DESTROY;
	EntityAI m_Entity;              // конкретная сущность (может быть null)
	string m_ClassEntity;           // класс для поиска/лута, если m_Entity == null (ACQUIRE)

	vector m_LastPosition;          // последняя известная позиция
	float m_LastPositionSpread = 0.0; // радиус вероятной позиции цели (центр m_LastPosition); 0 при видимости, растёт по шуму
	float m_LastDistance;           // последнее известное расстояние до цели
	bool m_HasLOS = false;          // видна в последнем скане
	float m_LastContact = 0.0;      // GetGame().GetTickTime() последнего контакта
	float m_LastDamage = 0.0;       // GetGame().GetTickTime() последнего полученного урона

	//! Абсолютное время (GetGame().GetTickTime()) следующей LOS-проверки этой цели.
	//! 0.0 = проверка нужна немедленно (сразу после открытия).
	float m_NextLOSUpdate = 0.0;

	float m_Threat = 0.0;           // опасность (0..1)
	float m_Attractiveness = 0.0;   // привлекательность (0..1)
	bool m_Friendly = false;        // не атаковать

	//! Точка прицеливания: живая позиция цели, если она видна, иначе последняя
	//! известная — бот не должен целиться в невидимую цель (телепатия запрещена).
	vector GetAimPosition()
	{
		if (!m_HasLOS)
			return m_LastPosition;
		return GetLivePosition();
	}

	//! Живое положение сущности в мире, НЕ зависит от видимости — использовать
	//! ТОЛЬКО для LOS-рейкаста (игрок → костная позиция головы, иначе GetPosition;
	//! без сущности — последняя известная позиция).
	vector GetLivePosition()
	{
		if (!m_Entity)
			return m_LastPosition;

		PlayerBase player;
		if (Class.CastTo(player, m_Entity))
		{
			int bone = player.GetBoneIndexByName("Head");
			if (bone >= 0)
				return player.GetBonePositionWS(bone);
		}

		return m_Entity.GetPosition();
	}

	//! Бот знает о существовании этой цели, знает её примерное или точное
	//! местоположение и может вести поиск. false — если цели нет в мире или нет
	//! ни одной известной позиции (m_LastPosition == vector.Zero).
	bool IsValidTarget()
	{
		if (!m_Entity)
			return false;
		if (m_LastPosition == vector.Zero)
			return false;
		return true;
	}

	//! Последняя известная позиция цели (на высоте головы) — использовать ТОЛЬКО
	//! для взгляда (LookAtPoint), не как точку движения/поиска.
	vector LastPosition()
	{
		return m_LastPosition;
	}

	//! Точка для поиска/приближения к цели (MoveTo): m_LastPosition + случайное
	//! отклонение в радиусе разброса, снэпнутое на навмеш (m_LastPosition на
	//! высоте головы, навмеш — на земле).
	vector ApproximatePosition()
	{
		//! Если нет известной позиции — возвращает vector.Zero (искать негде);
		//! вызывающий обязан отфильтровать цель через IsValidTarget().
		if (m_LastPosition == vector.Zero)
			return vector.Zero;

		float spread = m_LastPositionSpread;
		if (spread < DM_HUNT_SPREAD_MIN)
			spread = DM_HUNT_SPREAD_MIN;

		float ang = Math.RandomFloat01() * Math.PI2;
		float minR = spread / 3.0;
		float radius = minR + Math.RandomFloat01() * (spread - minR);

		vector goal = m_LastPosition;
		goal[0] = goal[0] + Math.Cos(ang) * radius;
		goal[2] = goal[2] + Math.Sin(ang) * radius;

		vector sampled = goal;
		if (dmBotPathfinder.SampleNavmeshPosition(goal, DM_PATH_SAMPLE_RADIUS, sampled) && sampled != vector.Zero)
		{
			vector offset = sampled - goal;
			offset[1] = 0.0;
			if (offset.Length() <= DM_PATH_SAMPLE_RADIUS)
				return sampled;
		}
		return goal;
	}
}