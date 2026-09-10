//! dmRedZone — общий (на всех ботов) реестр «зон избегания» (опасных мест, напр.
//! горящих костров) + хелпер детекта костра.
//!
//! Зоны копятся глобально и проверяются без привязки к конкретному боту: точка/отрезок
//! считаются «внутри опасной зоны» по 2D-горизонтали. Вызывающих сторон пока нет —
//! накопление зон и проверки подключаются в следующих фазах (MoveTo/EEHitBy).

//! Одна зона избегания: центр (m_Position), радиус и срок действия.
class dmRedZoneEntry
{
	vector m_Position;
	float m_Radius;
	float m_Until;   // GetGame().GetTickTime() + timeout; -1.0 = бессрочно
}

class dmRedZone
{
	static ref array<ref dmRedZoneEntry> s_Zones = new array<ref dmRedZoneEntry>;

	//! Добавить зону избегания. timeout <= 0 — бессрочно.
	static void Add(vector pos, float radius, float timeout = -1.0)
	{
		dmRedZoneEntry e = new dmRedZoneEntry;
		e.m_Position = pos;
		e.m_Radius = radius;
		e.m_Until = -1.0;
		if (timeout > 0.0)
			e.m_Until = GetGame().GetTickTime() + timeout;
		s_Zones.Insert(e);

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[RedZone] add pos=" + pos + " radius=" + radius + " until=" + e.m_Until);
		#endif
	}

	//! Точка (горизонтально, 2D) внутри любой зоны.
	static bool IsPointInside(vector pos)
	{
		Prune();
		vector p = pos;
		p[1] = 0.0;
		int i;
		for (i = 0; i < s_Zones.Count(); i++)
		{
			vector c = s_Zones[i].m_Position;
			c[1] = 0.0;
			if (vector.Distance(p, c) <= s_Zones[i].m_Radius)
				return true;
		}
		return false;
	}

	//! Отрезок a->b (2D) пересекает любую зону.
	static bool IsSegmentCrossing(vector a, vector b)
	{
		Prune();
		int i;
		for (i = 0; i < s_Zones.Count(); i++)
		{
			dmRedZoneEntry zone = s_Zones[i];
			if (PointToSegmentDist2D(zone.m_Position, a, b) <= zone.m_Radius)
				return true;
		}
		return false;
	}

	//! Ближайшая зона к pos. Возвращает true и заполняет center/radius, или false.
	static bool FindNearest(vector pos, out vector center, out float radius)
	{
		Prune();
		vector p = pos;
		p[1] = 0.0;
		dmRedZoneEntry nearest = null;
		float best = -1.0;
		int i;
		for (i = 0; i < s_Zones.Count(); i++)
		{
			vector c = s_Zones[i].m_Position;
			c[1] = 0.0;
			float d = vector.Distance(p, c);
			if (!nearest || d < best)
			{
				nearest = s_Zones[i];
				best = d;
			}
		}
		if (!nearest)
			return false;
		center = nearest.m_Position;
		radius = nearest.m_Radius;
		return true;
	}

	//! Ближайший горящий костёр в радиусе radius вокруг pos, или null.
	static FireplaceBase ScanFireplace(vector pos, float radius)
	{
		vector minPos = pos - Vector(radius, radius, radius);
		vector maxPos = pos + Vector(radius, radius, radius);
		array<EntityAI> entities = new array<EntityAI>();
		DayZPlayerUtils.SceneGetEntitiesInBox(minPos, maxPos, entities, QueryFlags.DYNAMIC);

		int i;
		for (i = 0; i < entities.Count(); i++)
		{
			FireplaceBase fire = FireplaceBase.Cast(entities[i]);
			if (fire && fire.IsBurning())
				return fire;
		}
		return null;
	}

	//! Удалить истёкшие зоны.
	static void Prune()
	{
		float now = GetGame().GetTickTime();
		int i;
		for (i = s_Zones.Count() - 1; i >= 0; i--)
		{
			if (s_Zones[i].m_Until >= 0.0 && now > s_Zones[i].m_Until)
				s_Zones.Remove(i);
		}
	}

	//! 2D-расстояние от точки p до отрезка a->b (проекция, зажатая на отрезок).
	private static float PointToSegmentDist2D(vector p, vector a, vector b)
	{
		vector P = p;
		P[1] = 0.0;
		vector A = a;
		A[1] = 0.0;
		vector B = b;
		B[1] = 0.0;

		float dx = B[0] - A[0];
		float dz = B[2] - A[2];
		float dd = dx * dx + dz * dz;

		float vx = P[0] - A[0];
		float vz = P[2] - A[2];

		if (dd < 0.0001)
			return Math.Sqrt(vx * vx + vz * vz);

		float t = (vx * dx + vz * dz) / dd;
		if (t < 0.0)
			t = 0.0;
		if (t > 1.0)
			t = 1.0;

		float cx = A[0] + dx * t;
		float cz = A[2] + dz * t;
		float rx = P[0] - cx;
		float rz = P[2] - cz;
		return Math.Sqrt(rx * rx + rz * rz);
	}
}
