//! dmBotState_Shelter — укрытие от песчаной бури.
//!
//! PREEMPTIVE: бот, оказавшись географически в фронте бури (intensity > 0.6) и без
//! видимой hostile-цели, спринтует (низко, на корточках) к ближайшему зданию с
//! дверьми и ждёт в «точке укрытия» (середина между дверями, снап на navmesh),
//! пока буря не пройдёт (intensity <= 0.1) или не появится видимая hostile-цель.
//!
//! Выход НЕ диктуется состоянием: hostile → вытеснение через рёбра на
//! shoot/fight/medical/getwater (все PREEMPTIVE); «буря прошла» → кооперативный
//! EXIT здесь, в OnUpdate. Гистерезис входа/выхода реализуется порогами: вход 0.6
//! (в кондишене), выход 0.1 (здесь) — без дополнительного флага.
class dmBotState_Shelter : dmBotState
{
	Building m_Building;
	vector m_ShelterPoint;
	ref dmBotIntent_MoveTo m_Move;
	ref dmBotIntent_Stance m_Stance;
	ref dmBotIntent_LookAround m_Scan;
	float m_PreferredSpeed;
	bool m_Reached;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.PREEMPTIVE;
	}

	override void OnEntry(dmBotState from)
	{
		super.OnEntry(from);

		dmAISurvivor bot = GetOwner();

		m_Building = null;
		m_ShelterPoint = vector.Zero;
		m_Move = null;
		m_Stance = null;
		m_Scan = null;
		m_Reached = false;
		m_PreferredSpeed = bot.GetPreferredSpeed();

		m_Building = dmLiveBuildingRegistry.Get().GetNearestBuildingWithDoors(bot.GetPosition(), DM_SHELTER_SEARCH_RADIUS, null);
		if (!m_Building)
		{
			dmBotLog.Error(bot.GetName(), "[Shelter] нет здания с дверьми в радиусе — укрыться негде");
			return;
		}

		ComputeShelterPoint(bot);

		bot.SetPreferredSpeed(3.0);

		m_Stance = CreateStanceIntent();
		bot.AddFSMIntent(m_Stance);

		m_Move = CreateMoveIntent();
		bot.AddFSMIntent(m_Move);

		#ifdef DM_BOT_DEBUG_FSM_SHELTER
		dmBotLog.Debug(bot.GetName(), "[FSM] Shelter.entry здание=" + m_Building.GetType());
		dmBotLog.Debug(bot.GetName(), "[FSM] Shelter: точка укрытия=" + m_ShelterPoint);
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();

		if (!m_Building)
			return EXIT;

		//! Буря прошла (intensity <= 0.1 или !IsActive) — выходим, даже если ещё бежим.
		if (!dmSandstorm.IsPointInStorm(bot.GetPosition(), DM_SANDSTORM_EXIT_INTENSITY))
		{
			#ifdef DM_BOT_DEBUG_FSM_SHELTER
			dmBotLog.Debug(bot.GetName(), "[FSM] Shelter: буря прошла — EXIT");
			#endif
			return EXIT;
		}

		EnsureStance(bot);

		if (!m_Reached)
			return StepMove(bot);

		EnsureScan(bot);
		return CONTINUE;
	}

	//! MOVE: спринт к точке укрытия. Done → WAIT; Failed → EXIT (недостижимо).
	int StepMove(dmAISurvivor bot)
	{
		if (m_Move && m_Move.IsFinished())
		{
			if (m_Move.IsFailed())
			{
				dmBotLog.Error(bot.GetName(), "[Shelter] точка укрытия недостижима — EXIT");
				return EXIT;
			}
			m_Move = null;
			m_Reached = true;

			#ifdef DM_BOT_DEBUG_FSM_SHELTER
			dmBotLog.Debug(bot.GetName(), "[FSM] Shelter: MOVE -> WAIT");
			#endif
			return CONTINUE;
		}

		if (m_Move && m_Move.IsExpired())
			m_Move = null;
		if (!m_Move)
		{
			m_Move = CreateMoveIntent();
			bot.AddFSMIntent(m_Move);
		}
		return CONTINUE;
	}

	//! Точка укрытия: среднее XZ уникальных дверей (дедуп по позиции, как в
	//! dmCampCoverFinder.StepDoorsProbe), затем снап на navmesh. 0 дверей → позиция здания.
	void ComputeShelterPoint(dmAISurvivor bot)
	{
		int doorCount = m_Building.GetDoorCount();
		ref map<string, bool> seen = new map<string, bool>();
		float sumX = 0.0;
		float sumZ = 0.0;
		int unique = 0;
		int i;
		vector dp;
		int kx;
		int kz;
		string key;

		for (i = 0; i < doorCount; i++)
		{
			dp = m_Building.GetDoorSoundPos(i);
			kx = Math.Floor(dp[0] * 2.0 + 0.5);
			kz = Math.Floor(dp[2] * 2.0 + 0.5);
			key = kx.ToString() + ":" + kz.ToString();
			if (seen.Contains(key))
				continue;
			seen.Insert(key, true);
			sumX = sumX + dp[0];
			sumZ = sumZ + dp[2];
			unique = unique + 1;
		}

		vector midpoint = m_Building.GetPosition();
		if (unique > 0)
		{
			midpoint[0] = sumX / (float)unique;
			midpoint[2] = sumZ / (float)unique;
		}

		vector sampled;
		if (bot.SampleNavmesh(midpoint, sampled))
			m_ShelterPoint = sampled;
		else
			m_ShelterPoint = midpoint;
	}

	private dmBotIntent_MoveTo CreateMoveIntent()
	{
		dmBotIntent_MoveTo m = new dmBotIntent_MoveTo();
		m.m_Goal = m_ShelterPoint;
		m.m_ReachDistance = DM_SHELTER_REACH;
		m.m_Priority = dmBotIntentPriority.CRITICAL;
		m.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		return m;
	}

	private dmBotIntent_Stance CreateStanceIntent()
	{
		dmBotIntent_Stance s = new dmBotIntent_Stance();
		s.m_Stance = DayZPlayerConstants.STANCEIDX_CROUCH;
		s.m_Priority = dmBotIntentPriority.CRITICAL;
		s.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		s.m_Deadline = -1.0;
		return s;
	}

	void EnsureStance(dmAISurvivor bot)
	{
		if (m_Stance && (m_Stance.IsFinished() || m_Stance.IsExpired()))
			m_Stance = null;
		if (!m_Stance)
		{
			m_Stance = CreateStanceIntent();
			bot.AddFSMIntent(m_Stance);
		}
	}

	//! Скан вокруг (как Idle.CreateScan), но сидя: голова/тело крутятся, стойка — корточки.
	void EnsureScan(dmAISurvivor bot)
	{
		if (m_Scan && (m_Scan.IsFinished() || m_Scan.IsExpired()))
			m_Scan = null;
		if (!m_Scan)
		{
			m_Scan = new dmBotIntent_LookAround();
			m_Scan.m_AllowBodyTurn = true;
			m_Scan.m_Turn = dmBotLookTurn.NONE;
			m_Scan.m_Priority = dmBotIntentPriority.DESIRABLE;
			bot.AddFSMIntent(m_Scan);
		}
	}

	override void OnExit(dmBotState to)
	{
		dmAISurvivor bot = GetOwner();

		if (m_Move) m_Move.Finish();
		if (m_Stance) m_Stance.Finish();
		if (m_Scan) m_Scan.Finish();

		//! Снять корточки — встать в полный рост (арбитраж и так вернёт ERECT без
		//! stance-интента, но стойку возвращаем явно).
		bot.SetStance(DayZPlayerConstants.STANCEIDX_ERECT);

		bot.SetPreferredSpeed(m_PreferredSpeed);

		#ifdef DM_BOT_DEBUG_FSM_SHELTER
		dmBotLog.Debug(bot.GetName(), "[FSM] Shelter.exit");
		#endif
	}
}
