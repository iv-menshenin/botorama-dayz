//! dmVision — perception: server-side scan for visible threats around the bot.
//!
//! The brain ticks at a fixed rate (~30 Hz); a full spatial query + FOV/LOS scan is
//! expensive, so dmVision throttles itself: it accumulates frame time and only runs
//! a scan every DM_PERCEPTION_INTERVAL seconds. Each scan rebuilds the bot's target
//! list (m_Targets) as a snapshot of what it currently sees; remembering/forgetting
//! targets across scans is a separate concern (task T4).
//!
//! Pipeline: box query (Scene or Physics, switchable) -> classify (player/zombie/
//! animal) -> distance/FOV -> line-of-sight -> dmTarget (DESTROY).

class dmVision
{
	//! Spatial query switch: true = SceneGetEntitiesInBox, false = PhysicsGetEntitiesInBox.
	bool m_UseScene = true;

	//! Accumulated time toward the next scan.
	float m_Accum = 0.0;

	//! Cached head-bone index of the bot's pawn (-1 = not resolved yet).
	int m_HeadBone = -1;

	//! Accumulate frame time and scan once every DM_PERCEPTION_INTERVAL.
	void Update(dmAISurvivor bot, float pDt)
	{
		m_Accum += pDt;
		if (m_Accum < DM_PERCEPTION_INTERVAL)
			return;
		m_Accum = 0.0;
		Scan(bot);
	}

	//! Snapshot the visible threats: box query -> classify -> distance/FOV/LOS -> targets.
	void Scan(dmAISurvivor bot)
	{
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		vector botPos = pawn.GetPosition();

		vector half = Vector(DM_PERCEPTION_RADIUS, DM_PERCEPTION_HEIGHT, DM_PERCEPTION_RADIUS);
		vector min = botPos - half;
		vector max = botPos + half;

		array<EntityAI> candidates = new array<EntityAI>();
		if (m_UseScene)
			DayZPlayerUtils.SceneGetEntitiesInBox(min, max, candidates, QueryFlags.DYNAMIC);
		else
			DayZPlayerUtils.PhysicsGetEntitiesInBox(min, max, candidates);

		//! Look direction (body + head). Prefer the head bone forward (includes the
		//! head turn); fall back to the body direction when the bone can't resolve.
		int hb = m_HeadBone;
		if (hb < 0)
		{
			hb = pawn.GetBoneIndexByName("Head");
			m_HeadBone = hb;
		}
		vector lookDir;
		if (hb >= 0)
		{
			vector transform[4];
			pawn.GetBoneTransformWS(hb, transform);
			lookDir = transform[1];
		}
		else
		{
			lookDir = pawn.GetDirection();
		}
		lookDir[1] = 0.0;
		lookDir.Normalize();

		bot.ClearTargets();

		int i;
		float priority;
		for (i = 0; i < candidates.Count(); i++)
		{
			EntityAI e = candidates[i];
			if (e == pawn)
				continue;

			priority = 0.0;
			if (PlayerBase.Cast(e))
				priority = 2.0;
			else if (e.IsInherited(ZombieBase))
				priority = 1.5;
			else if (e.IsInherited(AnimalBase))
				priority = 1.0;
			else
				continue;

			vector targetPos = e.GetPosition();
			vector toTarget = targetPos - botPos;
			toTarget[1] = 0.0;
			float dist = toTarget.Length();
			if (dist > DM_PERCEPTION_RADIUS)
				continue;
			if (dist < 0.01)
				continue;

			float targetYaw = toTarget.VectorToAngles()[0];
			float lookYaw = lookDir.VectorToAngles()[0];
			float ang = dmAISurvivor.AngleDiff(targetYaw, lookYaw);
			if (Math.AbsFloat(ang) > DM_PERCEPTION_FOV * 0.5)
				continue;

			if (!HasLOS(pawn, e, botPos, targetPos))
				continue;

			dmTarget t = new dmTarget();
			t.m_Type = dmTargetType.DESTROY;
			t.m_Entity = e;
			t.m_LastPosition = targetPos;
			t.m_Priority = priority;
			bot.AddTarget(t);

			#ifdef DM_BOT_DEBUG_VISION
			dmBotLog.Debug("[Vision] hit: " + e + " dist=" + dist + " priority=" + priority);
			#endif
		}

		#ifdef DM_BOT_DEBUG_VISION
		dmBotLog.Debug("[Vision] scan: candidates=" + candidates.Count() + " targets=" + bot.GetTargets().Count() + " scene=" + m_UseScene);
		#endif
	}

	//! Line-of-sight from the bot's eye to the target's head (fallback: feet + eye
	//! height). Visible when the closest raycast hit is the target itself.
	bool HasLOS(PlayerBase pawn, EntityAI target, vector botPos, vector targetPos)
	{
		vector eye = botPos + Vector(0, DM_EYE_HEIGHT, 0);

		int hb = target.GetBoneIndexByName("Head");
		vector end;
		if (hb >= 0)
			end = target.GetBonePositionWS(hb);
		else
			end = targetPos + Vector(0, DM_EYE_HEIGHT, 0);

		if (end == vector.Zero)
			end = targetPos;

		RaycastRVParams rp = new RaycastRVParams(eye, end, pawn);
		rp.sorted = true;
		rp.type = ObjIntersectView;
		rp.flags = CollisionFlags.NEARESTCONTACT;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (!DayZPhysics.RaycastRVProxy(rp, hits) || hits.Count() == 0)
			return true;

		Object o = hits[0].obj;
		Object p = hits[0].parent;
		return (o == target) || (p == target);
	}

	//! Toggle between Scene and Physics spatial queries.
	void ToggleQuery()
	{
		m_UseScene = !m_UseScene;
	}

	//! Which spatial query is active (true = Scene, false = Physics).
	bool GetUseScene()
	{
		return m_UseScene;
	}
}
