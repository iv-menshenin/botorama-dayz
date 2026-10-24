//! dmCoverFinder — статический реюзабельный поиск укрытия. Геометрия вынесена из
//! dmBotIntent_EvadeAim (FindCoverCandidate/IsBlocked/GetHeadPosition) + две новые
//! проверки: безопасность от целей и «коричневые зоны».
//!
//! Валидация кандидата = пять условий:
//!   1. путь достижим (bot.FindPathTo);
//!   2. на поверхности (guard по GetGame().SurfaceY, DM_COVER_MAX_SURFACE_DELTA);
//!   3. укрытие от прицела — рейкаст «голова угрозы → кандидат (шея бота)» заблокирован;
//!   4. безопасность от целей — в safeRadius нет живой угрозы (safeRadius <= 0 — пропуск);
//!   5. НЕ внутри коричневой зоны (bot.GetBrownZone().IsPointInside).

class dmCoverFinder
{
	//! Полный поиск: свип по полукругу (центр threatPoint, радиус minDist..maxDist)
	//! — первый валидный кандидат. safeRadius <= 0 — НЕ проверять цели (режим EvadeAim).
	static bool FindSafeCover(dmAISurvivor bot, vector threatPoint, float minDist, float maxDist, float safeRadius, out vector result)
	{
		float neckHeight = GetBotNeckHeight(bot);
		vector botPos = bot.GetPosition();
		vector base = botPos - threatPoint;
		base[1] = 0.0;
		float baseDir = base.VectorToAngles()[0];

		float dist = minDist;
		while (dist <= maxDist)
		{
			float angle = DM_COVER_START_ANGLE;
			while (angle <= DM_COVER_MAX_ANGLE)
			{
				if (TrySweepCandidate(bot, threatPoint, baseDir, angle, dist, neckHeight, safeRadius, result))
					return true;
				if (TrySweepCandidate(bot, threatPoint, baseDir, -angle, dist, neckHeight, safeRadius, result))
					return true;
				angle = angle + DM_COVER_ANGLE_STEP;
			}
			dist = dist + DM_COVER_DIST_STEP;
		}
		return false;
	}

	//! Безопасна ли точка от известных целей: в safeRadius нет живой угрозы
	//! (threat > DM_COVER_SAFE_THREAT) и нет ZombieBase. Пропускает m_Friendly,
	//! мёртвых и цели без сущности (m_Entity == null); позиция — m_Entity.GetPosition().
	static bool IsSafeFromTargets(dmAISurvivor bot, vector pos, float safeRadius)
	{
		ref array<ref dmTarget> targets = bot.GetTargets();
		int i;
		for (i = 0; i < targets.Count(); i++)
		{
			dmTarget t = targets[i];
			if (t.m_Friendly)
				continue;

			EntityAI e = t.m_Entity;
			if (!e || !e.IsAlive())
				continue;

			vector tPos = e.GetPosition();
			tPos[1] = 0.0;
			vector p = pos;
			p[1] = 0.0;

			if (vector.Distance(p, tPos) > safeRadius)
				continue;

			if (e && ZombieBase.Cast(e))
				return false;
			if (t.m_Threat > DM_COVER_SAFE_THREAT)
				return false;
		}
		return true;
	}

	//! Укрытие от одной точки: рейкаст «голова угрозы -> кандидат (шея)» заблокирован.
	static bool HasCoverFromPoint(EntityAI threat, vector candidate, float neckHeight)
	{
		if (!threat)
			return false;
		return HasCoverFromThreat(threat, threat.GetPosition(), candidate, neckHeight);
	}

	//! Один кандидат: все пять условий. threat может быть null — тогда укрытие от
	//! прицела считается от threatPos (ноги угрозы + высота глаз).
	static bool ValidateCandidate(dmAISurvivor bot, EntityAI threat, vector threatPos, vector candidate, float neckHeight, float safeRadius, out vector result)
	{
		ref array<vector> path = new array<vector>();
		if (!bot.FindPathTo(candidate, path))
		{
			#ifdef DM_BOT_DEBUG_COVER
			dmBotLog.Debug("[Cover] reject path: " + candidate);
			#endif
			return false;
		}
		if (path.Count() == 0)
		{
			#ifdef DM_BOT_DEBUG_COVER
			dmBotLog.Debug("[Cover] reject path-empty: " + candidate);
			#endif
			return false;
		}

		vector p = path[path.Count() - 1];

		float groundY = GetGame().SurfaceY(p[0], p[2]);
		if (Math.AbsFloat(p[1] - groundY) > DM_COVER_MAX_SURFACE_DELTA)
		{
			#ifdef DM_BOT_DEBUG_COVER
			dmBotLog.Debug("[Cover] reject surface: " + p);
			#endif
			return false;
		}

		if (!HasCoverFromThreat(threat, threatPos, p, neckHeight))
		{
			#ifdef DM_BOT_DEBUG_COVER
			dmBotLog.Debug("[Cover] reject no-cover: " + p);
			#endif
			return false;
		}

		if (safeRadius > 0.0 && !IsSafeFromTargets(bot, p, safeRadius))
		{
			#ifdef DM_BOT_DEBUG_COVER
			dmBotLog.Debug("[Cover] reject unsafe: " + p);
			#endif
			return false;
		}

		if (bot.GetBrownZone().IsPointInside(p))
		{
			#ifdef DM_BOT_DEBUG_COVER
			dmBotLog.Debug("[Cover] reject brown-zone: " + p);
			#endif
			return false;
		}

		#ifdef DM_BOT_DEBUG_COVER
		dmBotLog.Debug("[Cover] accept: " + p);
		#endif
		result = p;
		return true;
	}

	//! Точка на полукруге (baseDir + angle, dist) → валидация одним кандидатом.
	private static bool TrySweepCandidate(dmAISurvivor bot, vector threatPoint, float baseDir, float angle, float dist, float neckHeight, float safeRadius, out vector result)
	{
		vector angles = Vector(baseDir + angle, 0.0, 0.0);
		vector dir = angles.AnglesToVector();
		vector cand = threatPoint + dir * dist;
		return ValidateCandidate(bot, null, threatPoint, cand, neckHeight, safeRadius, result);
	}

	//! Укрытие от прицела угрозы: рейкаст «голова угрозы → шея бота на кандидате».
	private static bool HasCoverFromThreat(EntityAI threat, vector threatPos, vector candidate, float neckHeight)
	{
		vector from;
		if (threat)
			from = GetHeadPosition(threat, threatPos);
		else
			from = threatPos + Vector(0.0, DM_EYE_HEIGHT, 0.0);
		float groundY = GetGame().SurfaceY(candidate[0], candidate[2]);
		vector to = Vector(candidate[0], groundY + neckHeight, candidate[2]);
		return RayBlocked(from, to, threat);
	}

	//! Голова угрозы (фолбэк: ноги + высота глаз).
	private static vector GetHeadPosition(EntityAI threat, vector fallbackPos)
	{
		int hb = -1;
		Human human = Human.Cast(threat);
		if (human)
			hb = human.GetBoneIndexByName("Head");
		vector end;
		if (hb >= 0)
			end = threat.GetBonePositionWS(hb);
		else
			end = fallbackPos + Vector(0.0, DM_EYE_HEIGHT, 0.0);
		return end;
	}

	//! True, когда луч от угрозы до кандидата перекрыт (укрытие). Угроза игнорируется.
	private static bool RayBlocked(vector from, vector to, EntityAI ignore)
	{
		RaycastRVParams rp = new RaycastRVParams(from, to, ignore);
		rp.sorted = true;
		rp.type = ObjIntersectView;
		rp.flags = CollisionFlags.NEARESTCONTACT;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (!DayZPhysics.RaycastRVProxy(rp, hits) || hits.Count() == 0)
			return false;
		return true;
	}

	//! Высота шеи бота над землёй (фолбэк DM_EYE_HEIGHT).
	private static float GetBotNeckHeight(dmAISurvivor bot)
	{
		float h = DM_EYE_HEIGHT;
		dmAISurvivorBase pawn = bot.GetPawn();
		if (pawn)
		{
			int neck = pawn.GetBoneIndexByName("Neck");
			if (neck >= 0)
			{
				vector botPos = bot.GetPosition();
				vector neckPos = pawn.GetBonePositionWS(neck);
				h = neckPos[1] - botPos[1];
			}
		}
		return h;
	}
}
