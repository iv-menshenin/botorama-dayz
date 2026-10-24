//! dmBrownZone — per-bot реестр «коричневых зон» (точек пулевых ранений).
//!
//! В отличие от dmRedZone (глобальный статик, влияет на pathfinding) — это
//! экземпляр на каждого бота. НЕ влияет на pathfinding: только отсекает кандидатов
//! при поиске укрытия (чтобы бот не прятался в месте, где в него уже попали).

//! Одна коричневая зона: центр (m_Position), радиус и срок действия.
class dmBrownZoneEntry
{
	vector m_Position;
	float m_Radius;
	float m_Until;   // GetGame().GetTickTime() + timeout; -1.0 = бессрочно
}

class dmBrownZone
{
	ref array<ref dmBrownZoneEntry> m_Zones;

	void dmBrownZone()
	{
		m_Zones = new array<ref dmBrownZoneEntry>();
	}

	//! Добавить зону пулевого ранения. timeout <= 0 — бессрочно.
	void Add(vector pos, float radius, float timeout = -1.0)
	{
		dmBrownZoneEntry e = new dmBrownZoneEntry;
		e.m_Position = pos;
		e.m_Radius = radius;
		e.m_Until = -1.0;
		if (timeout > 0.0)
			e.m_Until = GetGame().GetTickTime() + timeout;
		m_Zones.Insert(e);

		#ifdef DM_BOT_DEBUG_COVER
		dmBotLog.Debug("[BrownZone] add pos=" + pos + " radius=" + radius + " until=" + e.m_Until);
		#endif
	}

	//! Точка (горизонтально, 2D) внутри любой зоны.
	bool IsPointInside(vector pos)
	{
		Prune();
		vector p = pos;
		p[1] = 0.0;
		int i;
		for (i = 0; i < m_Zones.Count(); i++)
		{
			vector c = m_Zones[i].m_Position;
			c[1] = 0.0;
			if (vector.Distance(p, c) <= m_Zones[i].m_Radius)
				return true;
		}
		return false;
	}

	//! Удалить истёкшие зоны.
	void Prune()
	{
		float now = GetGame().GetTickTime();
		int i;
		for (i = m_Zones.Count() - 1; i >= 0; i--)
		{
			if (m_Zones[i].m_Until >= 0.0 && now > m_Zones[i].m_Until)
				m_Zones.Remove(i);
		}
	}
}
