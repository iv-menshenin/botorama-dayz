//! dmCampCoverFinder — глобальный кэшируемый поиск укрытия для состояния Kamping.
//!
//! Внешний синглтон (как dmRoadGraphManager): потиковый асинхронный алгоритм —
//! Tick(timeslice) продвигает ПЕРЕДНИЙ job очереди на один шаг (одна дверь/одна
//! точка за тик), поэтому тяжёлый скан/рейкасты не замораживают серверный кадр.
//! Результаты кэшируются по (здание) для внешних дверей и по (здание, дверь) для
//! точек укрытия, и выдаются повторно без пересчёта.
//!
//! Смысл — «видеть дверь, из которой выйдет цель» (не спрятаться от угрозы, как
//! dmCoverFinder): бот ищет препятствие напротив внешней двери и точку ЗА ним,
//! откуда видно дверь. Детали геометрии — docs/research/camping-doors.md.

//! Фаза одного job'а: одна фаза (или её под-шаг) на Tick.
enum dmCampPhase
{
	DOORS_PROBE,   // внешние двери здания, по одной за тик
	SCANBOX,       // сканбокс препятствий напротив двери (один шаг)
	CANDIDATES,    // точки-кандидаты за препятствиями (один шаг)
	VALIDATE,      // валидация по одной точке за тик
	DONE
};

//! Кандидат: точка стояния + объект-укрытие (для LOS-проверки «ближайший хит — само укрытие»).
class dmCampCandidate
{
	vector m_Point;
	EntityAI m_Cover;   // managed, без ref
};

//! Асинхронный job: фазовая машина с полным состоянием между тиками.
class dmCampCoverJob
{
	ref dmCampCoverFinder m_Owner;
	ref dmAISurvivor m_Bot;     // null для doors-only job
	Building m_Building;        // managed
	int m_DoorIdx;              // -1 для doors-only job
	string m_Key;               // ключ дедупа (building для дверей, building#door для укрытия)
	bool m_DoorsOnly;

	int m_Phase;
	vector m_Center;
	int m_DoorCount;
	ref array<int> m_Doors;

	vector m_DoorPos;           // XZ двери + высота груди
	vector m_OutDir;            // ось «наружу» (2D, нормирована)
	vector m_PerpDir;           // перпендикуляр (2D)
	ref array<EntityAI> m_Obstacles;
	ref array<ref dmCampCandidate> m_Candidates;
	int m_ValidateIdx;
	ref array<vector> m_Results;
	vector m_BotPos;            // позиция бота, зафиксирована на старте VALIDATE

	void dmCampCoverJob(dmCampCoverFinder owner, Building building, int doorIdx, dmAISurvivor bot, bool doorsOnly)
	{
		m_Owner = owner;
		m_Building = building;
		m_DoorIdx = doorIdx;
		m_Bot = bot;
		m_DoorsOnly = doorsOnly;

		m_Key = dmCampCoverFinder.KeyOf(building);
		if (!doorsOnly)
			m_Key = dmCampCoverFinder.KeyOf(building, doorIdx);

		m_Phase = dmCampPhase.DOORS_PROBE;
		m_DoorCount = building.GetDoorCount();
		m_Doors = new array<int>();
		m_Obstacles = new array<EntityAI>();
		m_Candidates = new array<ref dmCampCandidate>();
		m_ValidateIdx = 0;
		m_Results = new array<vector>();

		vector center = building.GetWorldPosition();
		center[1] = 0.0;
		m_Center = center;
	}

	bool IsDone()
	{
		return m_Phase == dmCampPhase.DONE;
	}

	string GetKey()
	{
		return m_Key;
	}

	//! Один асинхронный шаг: ровно одна фаза (или под-шаг двери/точки).
	void Step()
	{
		if (m_Phase == dmCampPhase.DOORS_PROBE)
			StepDoorsProbe();
		else if (m_Phase == dmCampPhase.SCANBOX)
			StepScanBox();
		else if (m_Phase == dmCampPhase.CANDIDATES)
			StepCandidates();
		else if (m_Phase == dmCampPhase.VALIDATE)
			StepValidate();
	}

	//! A. Все двери — одним шагом (детект внешних дверей убран: все двери внешние).
	void StepDoorsProbe()
	{
		// cover-job: если двери уже готовы (параллельный doors-job успел) — пропустить A.
		if (!m_DoorsOnly && m_Owner.IsDoorsCached(m_Building))
		{
			m_Phase = dmCampPhase.SCANBOX;
			PrepareScanBox();
			return;
		}

		ref map<string, bool> seen = new map<string, bool>();
		int i;
		vector dp;
		int kx;
		int kz;
		string key;
		for (i = 0; i < m_DoorCount; i++)
		{
			dp = m_Building.GetDoorSoundPos(i);
			kx = Math.Floor(dp[0] * 2.0 + 0.5);
			kz = Math.Floor(dp[2] * 2.0 + 0.5);
			key = kx.ToString() + ":" + kz.ToString();
			if (seen.Contains(key))
				continue;
			seen.Insert(key, true);
			m_Doors.Insert(i);
		}

		m_Owner.CacheDoors(m_Building, m_Doors);

		#ifdef DM_BOT_DEBUG_CAMP
		dmBotLog.Debug("[Camp] doors done building=" + m_Building.GetType() + " doors=" + m_Doors.Count() + "/" + m_DoorCount);
		#endif

		if (m_DoorsOnly)
			m_Phase = dmCampPhase.DONE;
		else
		{
			m_Phase = dmCampPhase.SCANBOX;
			PrepareScanBox();
		}
	}

	//! Подготовка к SCANBOX: позиция двери, ось «наружу», перпендикуляр.
	void PrepareScanBox()
	{
		vector doorPos = m_Building.GetDoorSoundPos(m_DoorIdx);
		float groundY = GetGame().SurfaceY(doorPos[0], doorPos[2]);
		doorPos[1] = groundY + DM_CAMP_DOOR_EYE_HEIGHT;
		m_DoorPos = doorPos;

		vector outDir = doorPos - m_Center;
		outDir[1] = 0.0;
		float lenSq = outDir.LengthSq();
		if (lenSq < 0.001)
			outDir = Vector(1.0, 0.0, 0.0);
		else
			outDir.Normalize();
		m_OutDir = outDir;
		m_PerpDir = Vector(-outDir[2], 0.0, outDir[0]);

		#ifdef DM_BOT_DEBUG_CAMP
		dmBotLog.Debug("[Camp] Kamping: prepare door=" + m_DoorIdx + " center=<" + m_Center[0] + "," + m_Center[2] + ">");
		dmBotLog.Debug("[Camp] Kamping: prepare doorPos=<" + m_DoorPos[0] + "," + m_DoorPos[2] + ">");
		dmBotLog.Debug("[Camp] Kamping: prepare outDir=<" + m_OutDir[0] + "," + m_OutDir[2] + ">");
		#endif

		m_Obstacles.Clear();
		m_Candidates.Clear();
		m_ValidateIdx = 0;
		m_Results.Clear();
	}

	//! B. Сканбокс препятствий в конусе напротив двери (один шаг): STATIC + DYNAMIC
	//! двумя вызовами; сканбокс — по AABB, вбирающему весь конус, фильтр — InCone.
	void StepScanBox()
	{
		float halfLen = DM_CAMP_CONE_LENGTH * 0.5;
		float halfWid = DM_CAMP_CONE_HALF_WIDTH;
		float centerX = m_DoorPos[0] + m_OutDir[0] * halfLen;
		float centerZ = m_DoorPos[2] + m_OutDir[2] * halfLen;
		float extX = Math.AbsFloat(m_OutDir[0]) * halfLen + Math.AbsFloat(m_PerpDir[0]) * halfWid;
		float extZ = Math.AbsFloat(m_OutDir[2]) * halfLen + Math.AbsFloat(m_PerpDir[2]) * halfWid;
		float groundY = GetGame().SurfaceY(centerX, centerZ);

		vector min = Vector(centerX - extX, groundY, centerZ - extZ);
		vector max = Vector(centerX + extX, groundY + DM_CAMP_SCAN_HEIGHT, centerZ + extZ);

		array<EntityAI> statics = new array<EntityAI>();
		DayZPlayerUtils.SceneGetEntitiesInBox(min, max, statics, QueryFlags.STATIC);
		array<EntityAI> dynamics = new array<EntityAI>();
		DayZPlayerUtils.SceneGetEntitiesInBox(min, max, dynamics, QueryFlags.DYNAMIC);

		int i;
		EntityAI e;
		bool inCone;
		bool isObs;
		for (i = 0; i < statics.Count(); i++)
		{
			e = statics[i];
			inCone = InCone(e.GetPosition());
			isObs = IsObstacle(e, m_Building);
			LogScanItem(e, inCone, isObs);
			if (inCone && isObs)
				m_Obstacles.Insert(e);
		}
		for (i = 0; i < dynamics.Count(); i++)
		{
			e = dynamics[i];
			inCone = InCone(e.GetPosition());
			isObs = IsObstacle(e, m_Building);
			LogScanItem(e, inCone, isObs);
			if (inCone && isObs)
				m_Obstacles.Insert(e);
		}

		#ifdef DM_BOT_DEBUG_CAMP
		dmBotLog.Debug("[Camp] scanbox statics=" + statics.Count() + " dynamics=" + dynamics.Count() + " obstacles=" + m_Obstacles.Count());
		#endif

		m_Phase = dmCampPhase.CANDIDATES;
	}

	//! Диагностический дамп решения по одной сущности сканбокса (почему взята/отброшена).
	void LogScanItem(EntityAI e, bool inCone, bool isObstacle)
	{
		#ifdef DM_BOT_DEBUG_CAMP
		bool isTree = e.IsTree();
		bool isBush = e.IsBush();
		bool isBld = BuildingBase.Cast(e) != null;
		bool isItem = ItemBase.Cast(e) != null;
		dmBotLog.Debug("[Camp] scan item type=" + e.GetType() + " isTree=" + isTree + " isBush=" + isBush + " isBld=" + isBld);
		dmBotLog.Debug("[Camp] scan item isItem=" + isItem + " inCone=" + inCone + " isObstacle=" + isObstacle);
		#endif
	}

	//! Точка внутри конуса поиска: напротив двери вдоль оси «наружу», не ближе
	//! DM_CAMP_MIN_DOOR_DIST и не дальше DM_CAMP_CONE_LENGTH, в пределах полуширины,
	//! растущей линейно с дистанцией.
	bool InCone(vector pos)
	{
		float dX = pos[0] - m_DoorPos[0];
		float dZ = pos[2] - m_DoorPos[2];

		float distSq = dX * dX + dZ * dZ;
		float minSq = DM_CAMP_MIN_DOOR_DIST * DM_CAMP_MIN_DOOR_DIST;
		if (distSq < minSq)
			return false;

		float alongAxis = dX * m_OutDir[0] + dZ * m_OutDir[2];
		if (alongAxis <= 0.0 || alongAxis > DM_CAMP_CONE_LENGTH)
			return false;

		float perpDist = Math.AbsFloat(dX * m_PerpDir[0] + dZ * m_PerpDir[2]);
		float maxPerp = alongAxis * DM_CAMP_CONE_HALF_WIDTH / DM_CAMP_CONE_LENGTH;
		return perpDist <= maxPerp;
	}

	//! C. Точки-кандидаты за каждым препятствием: на дальней (от двери) стороне по
	//! углам 0°, ±30°, ±60° от awayDir, на дистанции radius+margin от центра.
	void StepCandidates()
	{
		int i;
		float radius;
		float dist;
		for (i = 0; i < m_Obstacles.Count(); i++)
		{
			EntityAI o = m_Obstacles[i];
			vector oPos = o.GetPosition();
			vector awayDir = oPos - m_DoorPos;
			awayDir[1] = 0.0;
			float lenSq = awayDir.LengthSq();
			if (lenSq < 0.001)
				continue;
			awayDir.Normalize();

			radius = o.GetCollisionRadius();
			if (radius < 0.3)
				radius = 1.0;
			dist = radius + DM_CAMP_COVER_BACK;

			AddCandidate(oPos, awayDir, 1.0, 0.0, dist, o);
			AddCandidate(oPos, awayDir, 0.866, 0.5, dist, o);
			AddCandidate(oPos, awayDir, 0.866, -0.5, dist, o);
			AddCandidate(oPos, awayDir, 0.5, 0.866, dist, o);
			AddCandidate(oPos, awayDir, 0.5, -0.866, dist, o);
		}

		#ifdef DM_BOT_DEBUG_CAMP
		dmBotLog.Debug("[Camp] candidates=" + m_Candidates.Count() + " obstacles=" + m_Obstacles.Count());
		#endif

		if (m_Bot)
			m_BotPos = m_Bot.GetPosition();
		else
			m_BotPos = m_DoorPos;

		m_Phase = dmCampPhase.VALIDATE;
		m_ValidateIdx = 0;
	}

	void AddCandidate(vector oPos, vector awayDir, float aFwd, float aSide, float dist, EntityAI cover)
	{
		vector dir = awayDir * aFwd + m_PerpDir * aSide;
		dir.Normalize();
		vector p = oPos + dir * dist;
		float groundY = GetGame().SurfaceY(p[0], p[2]);
		p[1] = groundY;

		dmCampCandidate c = new dmCampCandidate();
		c.m_Point = p;
		c.m_Cover = cover;
		m_Candidates.Insert(c);
	}

	//! D. Валидация по одной точке за тик.
	void StepValidate()
	{
		if (!m_Bot || !m_Bot.IsSpawned())
		{
			dmBotLog.Error("[Camp] job aborted: bot gone building=" + m_Building.GetType());
			m_Phase = dmCampPhase.DONE;
			return;
		}

		if (m_ValidateIdx >= m_Candidates.Count())
		{
			FinalizeCover();
			return;
		}

		dmCampCandidate cand = m_Candidates[m_ValidateIdx];
		m_ValidateIdx = m_ValidateIdx + 1;

		// 1. дистанция от бота (дёшево, ранний выход)
		float dx = cand.m_Point[0] - m_BotPos[0];
		float dz = cand.m_Point[2] - m_BotPos[2];
		float maxDistSq = DM_CAMP_MAX_DIST * DM_CAMP_MAX_DIST;
		if (dx * dx + dz * dz > maxDistSq)
		{
			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] reject dist point=" + cand.m_Point);
			#endif
			return;
		}

		// 2. LOS «дверь (глаза) → точка (присяд)»: свободен или только само укрытие
		// (reject-лог с деталями хита — внутри ValidateLOS).
		if (!ValidateLOS(cand))
			return;

		// 3. точка на навмеше.
		vector sampled;
		if (!m_Bot.SampleNavmesh(cand.m_Point, sampled))
		{
			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] reject navmesh point=" + cand.m_Point);
			#endif
			return;
		}

		// 4. достижимость.
		array<vector> path = new array<vector>();
		if (!m_Bot.FindPathTo(sampled, path))
		{
			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] reject path point=" + cand.m_Point);
			#endif
			return;
		}
		if (path.Count() == 0)
		{
			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] reject path point=" + cand.m_Point);
			#endif
			return;
		}

		m_Results.Insert(sampled);

		#ifdef DM_BOT_DEBUG_CAMP
		dmBotLog.Debug("[Camp] valid cover point=" + sampled + " results=" + m_Results.Count());
		#endif

		if (m_Results.Count() >= DM_CAMP_MAX_RESULTS)
			FinalizeCover();
	}

	//! LOS двери до точки: свободен или ближайший хит — само укрытие (не постороннее).
	bool ValidateLOS(dmCampCandidate cand)
	{
		vector doorEye = m_DoorPos;
		float groundY = GetGame().SurfaceY(cand.m_Point[0], cand.m_Point[2]);
		vector pointEye = Vector(cand.m_Point[0], groundY + DM_CAMP_CROUCH_EYE, cand.m_Point[2]);

		RaycastRVParams rp = new RaycastRVParams(doorEye, pointEye, null);
		rp.sorted = true;
		rp.type = ObjIntersectView;
		rp.flags = CollisionFlags.NEARESTCONTACT;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		DayZPhysics.RaycastRVProxy(rp, hits);
		if (hits.Count() == 0)
			return true;

		RaycastRVResult h = hits[0];
		if (h.obj == cand.m_Cover || h.parent == cand.m_Cover)
			return true;

		#ifdef DM_BOT_DEBUG_CAMP
		string hitType = "null";
		if (h.obj)
			hitType = h.obj.GetType();
		string hitParent = "null";
		if (h.parent)
			hitParent = h.parent.GetType();
		dmBotLog.Debug("[Camp] reject LOS point=" + cand.m_Point + " hitType=" + hitType + " hitParent=" + hitParent);
		#endif

		return false;
	}

	//! E. Сортировка по близости к боту + запись в кэш укрытий.
	void FinalizeCover()
	{
		SortResultsByDistance();
		m_Owner.CacheCover(m_Building, m_DoorIdx, m_Results);

		#ifdef DM_BOT_DEBUG_CAMP
		dmBotLog.Debug("[Camp] cover done door=" + m_DoorIdx + " points=" + m_Results.Count());
		#endif

		m_Phase = dmCampPhase.DONE;
	}

	void SortResultsByDistance()
	{
		int i;
		int j;
		for (i = 1; i < m_Results.Count(); i++)
		{
			vector key = m_Results[i];
			float keyDist = DistSq(key);
			j = i - 1;
			while (j >= 0 && DistSq(m_Results[j]) > keyDist)
			{
				m_Results[j + 1] = m_Results[j];
				j = j - 1;
			}
			m_Results[j + 1] = key;
		}
	}

	float DistSq(vector p)
	{
		float dx = p[0] - m_BotPos[0];
		float dz = p[2] - m_BotPos[2];
		return dx * dx + dz * dz;
	}

	//! Является ли сущность «укрытием» (за чем можно спрятаться и видеть дверь).
	//! Держим деревья, здания/заборы/стены и прочие динамические препятствия
	//! (машины); отбрасываем кусты/траву, существ и обычный лут.
	static bool IsObstacle(EntityAI e, Building self)
	{
		if (!e || e == self)
			return false;
		if (e.IsTree())
			return true;
		if (e.IsBush())
			return false;
		if (BuildingBase.Cast(e))
			return true;
		if (e.IsMan() || e.IsZombie() || e.IsAnimal())
			return false;
		if (ItemBase.Cast(e))
			return false;
		return true;
	}
};

class dmCampCoverFinder
{
	static ref dmCampCoverFinder s_Instance;

	ref map<string, ref array<int>> m_DoorCache;     // здание -> внешние двери
	ref map<string, ref array<vector>> m_CoverCache; // здание#дверь -> точки укрытия
	ref array<ref dmCampCoverJob> m_Jobs;

	static dmCampCoverFinder Get()
	{
		if (!s_Instance)
			s_Instance = new dmCampCoverFinder();
		return s_Instance;
	}

	void dmCampCoverFinder()
	{
		m_DoorCache = new map<string, ref array<int>>();
		m_CoverCache = new map<string, ref array<vector>>();
		m_Jobs = new array<ref dmCampCoverJob>();
	}

	//! Стабильный ключ здания: "type@<X>,<Z>" (X/Z округлены до 0.1 м).
	static string KeyOf(Building b)
	{
		vector p = b.GetPosition();
		int x = Math.Floor(p[0] * 10.0 + 0.5);
		int z = Math.Floor(p[2] * 10.0 + 0.5);
		return b.GetType() + "@" + x + "," + z;
	}

	//! Ключ «здание + дверь».
	static string KeyOf(Building b, int doorIdx)
	{
		return KeyOf(b) + "#" + doorIdx;
	}

	//! Внешние двери здания: true + двери, когда готово; false — пока job в очереди.
	bool QueryExternalDoors(Building building, out array<int> doors)
	{
		if (!building)
			return false;
		string key = KeyOf(building);
		if (m_DoorCache.Find(key, doors))
		{
			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] doors cache hit key=" + key);
			#endif
			return true;
		}
		EnqueueJob(new dmCampCoverJob(this, building, -1, null, true));
		return false;
	}

	//! Точки укрытия напротив конкретной двери: true + точки, когда готово; false — пока job в очереди.
	bool QueryCover(dmAISurvivor bot, Building building, int doorIdx, out array<vector> points)
	{
		if (!bot || !building || doorIdx < 0)
			return false;
		string key = KeyOf(building, doorIdx);
		if (m_CoverCache.Find(key, points))
		{
			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] cover cache hit key=" + key + " points=" + points.Count());
			#endif
			return true;
		}
		EnqueueJob(new dmCampCoverJob(this, building, doorIdx, bot, false));
		return false;
	}

	bool IsCoverCached(Building building, int doorIdx)
	{
		if (!building || doorIdx < 0)
			return false;
		return m_CoverCache.Contains(KeyOf(building, doorIdx));
	}

	//! Внешние двери здания уже в кэше (для пропуска фазы A cover-job'ом).
	bool IsDoorsCached(Building building)
	{
		return m_DoorCache.Contains(KeyOf(building));
	}

	void CacheDoors(Building building, array<int> doors)
	{
		string key = KeyOf(building);
		if (!m_DoorCache.Contains(key))
			m_DoorCache.Insert(key, doors);
	}

	void CacheCover(Building building, int doorIdx, array<vector> points)
	{
		string key = KeyOf(building, doorIdx);
		if (!m_CoverCache.Contains(key))
			m_CoverCache.Insert(key, points);
	}

	//! Один асинхронный шаг переднего job'а очереди.
	void Tick(float timeslice)
	{
		if (m_Jobs.Count() == 0)
			return;
		dmCampCoverJob job = m_Jobs[0];
		job.Step();
		if (job.IsDone())
			m_Jobs.Remove(0);
	}

	//! Поставить job в очередь; дедуп по ключу (один job на (здание, дверь)).
	void EnqueueJob(dmCampCoverJob job)
	{
		int i;
		for (i = 0; i < m_Jobs.Count(); i++)
		{
			if (m_Jobs[i].GetKey() == job.GetKey())
			{
				#ifdef DM_BOT_DEBUG_CAMP
				dmBotLog.Debug("[Camp] job dedup key=" + job.GetKey());
				#endif
				return;
			}
		}
		m_Jobs.Insert(job);

		#ifdef DM_BOT_DEBUG_CAMP
		dmBotLog.Debug("[Camp] job enqueued key=" + job.GetKey() + " queue=" + m_Jobs.Count());
		#endif
	}
}
