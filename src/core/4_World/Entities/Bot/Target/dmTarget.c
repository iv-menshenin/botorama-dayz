//! dmTarget — the bot's goal target: memory + evaluation (survives FSM transitions).
//!
//! A target is what the bot wants to acquire or destroy. The bot may not know the
//! target's live position (out of visibility) — it then searches by m_LastPosition
//! or by m_ClassEntity (e.g. "BandageDressing" when m_Entity is null).
//!
//! Memory (T4): a target that hides behind a wall/brush stays in the list with its
//! last known position until it times out — m_LastPosition / m_HasLOS / m_LastContact.
//! Evaluation: m_Threat / m_Friendly drive combat choices; attention (m_Attraction)
//! lives in dmTargetSound and is exposed via virtual accessors.

enum dmTargetType
{
	ACQUIRE,   // завладеть (предмет)
	DESTROY    // уничтожить (существо/игрок)
};

//! dmTarget — abstract base class (cannot be created directly). Общая память/перцепция
//! цели. Конкретные типы — подклассы: dmTargetDummy (фантом), dmTargetSound (звук) и
//! опознанные (dmTargetPlayer/Zombie/Animal/Entity). Инстанцировать только подклассы.
class dmTarget
{
	dmTargetType m_Type = dmTargetType.DESTROY;
	EntityAI m_Entity;              // конкретная сущность (может быть null)
	string m_ClassEntity;           // класс для поиска/лута, если m_Entity == null (ACQUIRE)

	vector m_LastPosition;          // последняя известная позиция
	float m_LastPositionSpread = 0.0; // радиус вероятной позиции цели (центр m_LastPosition); 0 при видимости, растёт по шуму
	float m_LastDistance;           // последнее известное расстояние до цели
	bool m_HasLOS = false;          // видна в последнем скане
	bool m_EverSeen = false;        // бот хоть раз видел цель (LOS-успех); false = создана слухом и ещё не видна
	bool m_EverHeard = false;       // бот хоть раз слышал цель (шум от неё создал/обновил звуковую цель)
	float m_LastSeen = 0.0;         // GetGame().GetTickTime() последнего зрительного контакта; 0 = никогда не видел
	float m_LastContact = 0.0;      // GetGame().GetTickTime() последнего контакта
	float m_LastDamage = 0.0;       // GetGame().GetTickTime() последнего полученного урона

	//! Абсолютное время (GetGame().GetTickTime()) следующей LOS-проверки этой цели.
	//! 0.0 = проверка нужна немедленно (сразу после открытия).
	float m_NextLOSUpdate = 0.0;

	float m_Threat = 0.0;           // опасность (0..1)
	bool m_Friendly = false;        // не атаковать

	//! Точка прицеливания: живая позиция цели, если она видна, иначе последняя
	//! известная — бот не должен целиться в невидимую цель (телепатия запрещена).
	vector GetAimPosition()
	{
		if (!m_HasLOS)
			return m_LastPosition;
		return GetLivePosition();
	}

	//! Видима ли цель в данный момент
	bool IsVisible()
	{
		return m_HasLOS;
	}

	//! Живое положение сущности в мире, НЕ зависит от видимости: игрок → костная
	//! позиция головы, иначе GetPosition; без сущности — последняя известная позиция.
	//! НЕ использовать для прицела (там GetAimPosition, учитывающий видимость).
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

	//! Враг сейчас в замахе/атаке мили (мы «ожидаем удара» от него).
	bool IsWindingUp()
	{
		if (!m_Entity)
			return false;
		DayZPlayerImplement dp = DayZPlayerImplement.Cast(m_Entity);
		if (dp && dp.GetCommand_Melee2())
			return true;          // игрок/бот в MELEE2
		DayZInfected zi = DayZInfected.Cast(m_Entity);
		if (zi && zi.GetCommand_Attack())
			return true;          // зомби в attack-команде
		return false;
	}

	//! Корпус врага направлен в сторону точки myPos (атакует «меня», а не кого-то ещё).
	bool IsFacingMe(vector myPos)
	{
		if (!m_Entity)
			return false;
		vector enemyPos = m_Entity.GetPosition();
		vector toMe = myPos - enemyPos;
		toMe[1] = 0.0;
		if (toMe.LengthSq() < 0.0001)
			return false;
		toMe.Normalize();
		vector enemyDir = m_Entity.GetDirection();
		enemyDir[1] = 0.0;
		if (enemyDir.LengthSq() < 0.0001)
			return false;
		enemyDir.Normalize();
		return vector.Dot(enemyDir, toMe) > 0.5;  // ~±60°, согласовано с PVP_MAX_BLOCKABLE_ANGLE
	}

	//! Последняя известная позиция цели (на высоте головы) — использовать ТОЛЬКО
	//! для взгляда (LookAtPoint), не как точку движения/поиска.
	vector LastPosition()
	{
		return m_LastPosition;
	}

	//! Радиус неопределённости последней известной позиции; 0 при точной видимости,
	//! растёт по шуму.
	float LastPositionSpread()
	{
		return m_LastPositionSpread;
	}

	//! Обновить память о позиции цели из потребителя; продюсеры (dmVision/регистрация
	//! целей) пишут поля напрямую — сюда только когда состоянию нужно освежить память.
	void RememberPosition(vector pos)
	{
		m_LastPosition = pos;
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

	//! Классификаторы типа цели. В базе возвращают false — переопределяются в подклассах
	//! (dmTargetPlayer/Zombie/Animal/Entity), когда цель опознана.
	bool IsPlayer()
	{
		return false;
	}
	bool IsPlayerAI()
	{
		return false;   // именно ИИ-бот (dmAISurvivorBase); IsPlayer ⊇ IsPlayerAI
	}
	bool IsZombie()
	{
		return false;
	}
	bool IsAnimal()
	{
		return false;
	}
	bool IsEntity()
	{
		return false;   // всё неживое (не Player/Zombie/Animal)
	}

	//! Цель опознана (Player/Zombie/Animal/Entity), а не фантом/звук.
	bool Identified()
	{
		return IsPlayer() || IsZombie() || IsAnimal() || IsEntity();
	}

	//! Фабрика: фантом (пререгистрация зрением, тип не определён).
	static dmTargetDummy CreateDummy(EntityAI entity)
	{
		dmTargetDummy t = new dmTargetDummy();
		t.m_Type = dmTargetType.DESTROY;
		t.m_Entity = entity;
		return t;
	}

	//! Фабрика: опознанная цель по типу сущности. Неживое → Dummy (dmTargetEntity пока нет).
	static dmTarget CreateIdentification(EntityAI entity)
	{
		dmTarget t;
		if (ZombieBase.Cast(entity)) t = new dmTargetZombie();
		else if (AnimalBase.Cast(entity)) t = new dmTargetAnimal();
		else if (PlayerBase.Cast(entity)) t = new dmTargetPlayer();
		else t = new dmTargetDummy();
		t.m_Type = dmTargetType.DESTROY;
		t.m_Entity = entity;
		return t;
	}

	//! Угроза «слышимой» цели по классу сущности (зеркалит CreateIdentification).
	static float ThreatForEntity(EntityAI entity)
	{
		if (PlayerBase.Cast(entity)) return DM_TARGET_THREAT_PLAYER;
		if (ZombieBase.Cast(entity)) return DM_TARGET_THREAT_ZOMBIE;
		if (AnimalBase.Cast(entity)) return DM_TARGET_THREAT_ANIMAL;
		return DM_NOISE_THREAT;   // неживое/неизвестное — старый плоский порог
	}

	//! Фабрика: звуковая цель по типу шума (dmNoiseType).
	static dmTargetSound CreateSound(EntityAI entity, int noiseType)
	{
		dmTargetSound t;
		if (noiseType == dmNoiseType.STEP) t = new dmTargetFootsteps();
		else if (noiseType == dmNoiseType.SHOT) t = new dmTargetShooting();
		else if (noiseType == dmNoiseType.SCREAM) t = new dmTargetScream();
		else if (noiseType == dmNoiseType.BULLETIMPACT) t = new dmTargetDroppedBullet();
		else if (noiseType == dmNoiseType.EXPLOSION) t = new dmTargetExplosion();
		else if (noiseType == dmNoiseType.DROP) t = new dmTargetDrop();
		else t = new dmTargetNoise();
		t.m_Type = dmTargetType.DESTROY;
		t.m_Entity = entity;
		return t;
	}

	//! Типобезопасные геттеры сущности. В базе null — переопределяются в подклассах:
	//! GetPlayer() — только для Player, GetZombie() — только Zombie, и т.д.
	PlayerBase GetPlayer()
	{
		return null;
	}
	ZombieBase GetZombie()
	{
		return null;
	}
	AnimalBase GetAnimal()
	{
		return null;
	}

	//! Общий доступ к сущности (вместо прямого m_Entity). Инкапсуляция: поле m_Entity
	//! со временем перестанет читаться извне напрямую.
	EntityAI GetEntity()
	{
		return m_Entity;
	}

	//! Полевые акцессоры (инкапсуляция полей m_* для внешних потребителей).
	bool HasLOS()                          { return m_HasLOS; }
	void SetHasLOS(bool v)                 { m_HasLOS = v; }
	float LastContact()                    { return m_LastContact; }
	void SetLastContact(float v)           { m_LastContact = v; }
	float LastDamage()                     { return m_LastDamage; }
	void SetLastDamage(float v)            { m_LastDamage = v; }
	float LastDistance()                   { return m_LastDistance; }
	void SetLastDistance(float v)          { m_LastDistance = v; }
	float NextLOSUpdate()                  { return m_NextLOSUpdate; }
	void SetNextLOSUpdate(float v)         { m_NextLOSUpdate = v; }
	float Threat()                         { return m_Threat; }
	void SetThreat(float v)                { m_Threat = v; }
	bool Friendly()                        { return m_Friendly; }
	void SetFriendly(bool v)               { m_Friendly = v; }
	bool EverSeen()                        { return m_EverSeen; }
	void SetEverSeen(bool v)               { m_EverSeen = v; }
	bool HaveEverSeen()                    { return m_EverSeen; }
	bool HaveEverHeard()                   { return m_EverHeard; }
	void SetEverHeard(bool v)              { m_EverHeard = v; }
	void SetLastSeen(float v)              { m_LastSeen = v; }
	float LastSeen()                       { return m_LastSeen; }
	//! Сколько секунд прошло с последнего зрительного контакта; -1 если никогда не видел.
	float LastSeenTime()
	{
		if (m_LastSeen <= 0.0)
			return -1.0;
		return GetGame().GetTickTime() - m_LastSeen;
	}
	void SetLastPosition(vector v)         { m_LastPosition = v; }
	void SetLastPositionSpread(float v)    { m_LastPositionSpread = v; }

	//! Интерес разузнать свежую цель (0 у не-звуковых целей; в dmTargetSound — реальное значение).
	float GetAttraction() { return 0.0; }
	void SetAttraction(float v) { }
	float GetAttentionCooldownUntil() { return 0.0; }
	void SetAttentionCooldownUntil(float v) { }
}