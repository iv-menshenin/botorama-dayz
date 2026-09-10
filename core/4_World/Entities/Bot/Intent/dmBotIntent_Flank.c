//! dmBotIntent_Flank — arc around a target the bot cannot see, to find a spot
//! with line-of-sight and resume firing.
//!
//! CRITICAL + PARALLEL, MOVE channel. Inherits the full path-following machinery
//! from dmBotIntent_MoveTo and is "continuous" (no immediate Fail on start when
//! there is no goal yet; a stuck MoveTo re-routes instead of aborting). The goal
//! is NOT fixed up front: each tick it sweeps the arc around the target, one
//! candidate at a time (±DM_FLANK_ANGLE_STEP from DM_FLANK_START_ANGLE up to
//! DM_FLANK_MAX_ANGLE), validates the candidate (navmesh path + terrain-surface
//! guard + LOS pre-check from the candidate's neck height to the target's head)
//! and only then re-paths. As soon as the target becomes visible (positive case)
//! it finishes early. A whole-attempt stall timeout (DM_FLANK_STALL_TIMEOUT) and
//! a fully exhausted sweep both Fail().
class dmBotIntent_Flank : dmBotIntent_MoveTo
{
	EntityAI m_TargetEntity;
	float m_BaseDir;       // yaw (degrees) of the target->bot direction at start
	float m_Dist;          // distance bot->target (D) at start
	float m_FlankAngle;    // current sweep angle (degrees)
	float m_NeckHeight;    // neck bone height of the bot above the ground
	float m_StallTimer;    // whole-attempt elapsed time while actively flanking

	void dmBotIntent_Flank()
	{
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "Flank";
	}

	override bool IsContinuous()
	{
		return true;
	}

	override bool KeepLookAtGoal()
	{
		return true;
	}

	override float GetMoveSpeed(dmAISurvivor bot)
	{
		return 2.0;
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);

		m_StallTimer = 0.0;
		m_FlankAngle = DM_FLANK_START_ANGLE;
		m_NeckHeight = DM_EYE_HEIGHT;

		if (!m_TargetEntity)
			return;

		vector botPos = bot.GetPosition();
		vector tPos = m_TargetEntity.GetPosition();
		vector d = botPos - tPos;
		d[1] = 0.0;
		m_BaseDir = d.VectorToAngles()[0];
		m_Dist = d.Length();

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
		{
			int neck = pawn.GetBoneIndexByName("Neck");
			if (neck >= 0)
			{
				vector neckPos = pawn.GetBonePositionWS(neck);
				m_NeckHeight = neckPos[1] - botPos[1];
			}
		}
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.Flank");
		#endif

		if (!m_TargetEntity)
		{
			Finish();
			return;
		}

		//! Positive case: the target is now visible — stop and hand back to Aim.
		if (CheckLOS(bot))
		{
			bot.SetMove(0.0, 0.0);
			Finish();
			return;
		}

		//! Stall guard: bound the whole flank attempt in time.
		m_StallTimer += pDt;
		if (m_StallTimer > DM_FLANK_STALL_TIMEOUT)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Flank: stall timeout=" + m_StallTimer);
			#endif
			dmBotLog.Error("Flank: stall timeout, abort");
			bot.SetMove(0.0, 0.0);
			Fail();
			return;
		}

		//! Sweep for a candidate only while there is no active path.
		if (!m_HasPath)
		{
			Sweep(bot);
			if (IsFinished())
				return;
		}

		//! No path yet (both sides failed this tick) — hold position, try next tick.
		if (!m_HasPath)
		{
			bot.SetMove(0.0, 0.0);
			return;
		}

		super.OnUpdate(bot, pDt);
	}

	override void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		bot.SetMove(0.0, 0.0);

		//! Re-check LOS at the candidate; if visible we are done.
		if (CheckLOS(bot))
		{
			Finish();
			return;
		}

		//! Still no LOS — drop the goal and keep sweeping from the current angle.
		m_Goal = vector.Zero;
		m_HasPath = false;
		m_Path = null;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Flank: достиг точки без LOS, продолжаю свип angle=" + m_FlankAngle);
		#endif
	}

	//! True when the current target is tracked and visible (per the perception
	//! LOS refresh, throttled per target). Drives the early finish.
	bool CheckLOS(dmAISurvivor bot)
	{
		if (!m_TargetEntity)
			return false;
		dmTarget t = bot.FindTarget(m_TargetEntity);
		if (t && t.m_HasLOS)
			return true;
		return false;
	}

	//! Advance the sweep angle and try both sides for a valid candidate; on the
	//! first hit set m_Goal and re-path. Exhausting the sweep fails the intent.
	void Sweep(dmAISurvivor bot)
	{
		m_FlankAngle += DM_FLANK_ANGLE_STEP;
		if (m_FlankAngle > DM_FLANK_MAX_ANGLE)
		{
			if ( m_Dist <= DM_FLANK_MIN_DIST * 2 )
			{
				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] Flank: свип исчерпан angle=" + m_FlankAngle);
				#endif
				dmBotLog.Error("Flank: свип исчерпан, abort");
				bot.SetMove(0.0, 0.0);
				Fail();
				return;	
			}
			m_Dist = m_Dist / 1.5;
			m_FlankAngle = DM_FLANK_START_ANGLE;
		}

		vector candidate;
		if (FindCandidate(bot, 1, candidate))
		{
			m_Goal = candidate;
			RePath(bot);
			return;
		}
		if (FindCandidate(bot, -1, candidate))
		{
			m_Goal = candidate;
			RePath(bot);
			return;
		}
	}

	//! Build and validate one sweep candidate. Returns true and fills `candidate`
	//! (the final path point) when the candidate has a navmesh path, sits on the
	//! terrain surface and has a clear line to the target's head.
	bool FindCandidate(dmAISurvivor bot, int sign, out vector candidate)
	{
		vector tPos = m_TargetEntity.GetPosition();

		float flankDist = m_Dist;
		if (flankDist > DM_FLANK_MAX_DIST)
			flankDist = DM_FLANK_MAX_DIST;

		float yaw = m_BaseDir + (sign * m_FlankAngle);
		vector angles = Vector(yaw, 0.0, 0.0);
		vector dir = angles.AnglesToVector();
		vector cand = tPos + dir * flankDist;

		ref array<vector> path = new array<vector>();
		if (!bot.FindPathTo(cand, path))
			return false;
		if (path.Count() == 0)
			return false;

		//! The last path point is already on the navmesh — use it, not the raw
		//! candidate (the pathfinder snaps and corrects the height).
		vector p = path[path.Count() - 1];

		//! Height guard: navmesh path points can hover far above/below the actual
		//! terrain surface (pathfinder height is unreliable) — reject those.
		float groundY = GetGame().SurfaceY(p[0], p[2]);
		if (Math.AbsFloat(p[1] - groundY) > DM_FLANK_MAX_SURFACE_DELTA)
			return false;

		//! Ray origin: the candidate point lifted to the bot's neck height above
		//! the surface (NOT the navmesh Y).
		vector rayFrom = Vector(p[0], groundY + m_NeckHeight, p[2]);
		vector rayTo = GetHeadPosition(tPos);

		if (!HasClearLine(rayFrom, rayTo))
			return false;

		candidate = p;
		return true;
	}

	//! Target head world position (fallback: feet + eye height).
	vector GetHeadPosition(vector fallbackPos)
	{
		int hb = -1;
		Human human = Human.Cast(m_TargetEntity);
		if (human)
			hb = human.GetBoneIndexByName("Head");
		else
		{
			DayZCreature creature = DayZCreature.Cast(m_TargetEntity);
			if (creature)
				hb = creature.GetBoneIndexByName("Head");
		}

		vector end;
		if (hb >= 0)
			end = m_TargetEntity.GetBonePositionWS(hb);
		else
			end = fallbackPos + Vector(0.0, DM_EYE_HEIGHT, 0.0);
		return end;
	}

	//! Raycast from `from` to the target head; true when nothing blocks (or the
	//! closest hit is the target itself), like dmVision.HasLOS.
	bool HasClearLine(vector from, vector to)
	{
		RaycastRVParams rp = new RaycastRVParams(from, to, null);
		rp.sorted = true;
		rp.type = ObjIntersectView;
		rp.flags = CollisionFlags.NEARESTCONTACT;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (!DayZPhysics.RaycastRVProxy(rp, hits) || hits.Count() == 0)
			return true;

		Object o = hits[0].obj;
		Object parent = hits[0].parent;
		if (o == m_TargetEntity || parent == m_TargetEntity)
			return true;
		return false;
	}
}
