//! dmBotIntent_MoveTo — walk to a world point along a navmesh path; finishes when
//! reached.
//!
//! On start it requests a path (bot.FindPathTo) and then follows the waypoints one
//! by one: each tick it steers toward the current waypoint using the body-relative
//! movement command (forward/back/strafe), without snapping the body. It requests
//! the body to face the waypoint (FULL); if a higher-priority look/turn intent owns
//! the body, the movement direction becomes a strafe/backpedal. A per-waypoint
//! progress monitor detects a stuck bot; instead of failing right away it steps
//! back/sideways and re-routes, up to DM_MOVE_MAX_RECOVER times, then gives up.
class dmBotIntent_MoveTo : dmBotIntent
{
	vector m_Target;
	float m_ReachDistance = 0.5;
	float m_ReachDeadline = 0.0;   // seconds to reach the target; 0 = no deadline

	ref array<vector> m_Path;
	int m_PathIdx = 0;

	float m_BestDist = -1.0;
	float m_NoProgressTime = 0.0;

	//! Stuck-recovery: step back/sideways for a short time before re-routing,
	//! up to DM_MOVE_MAX_RECOVER attempts (see OnUpdate).
	bool m_Recovering = false;
	float m_RecoverTimer = 0.0;
	int m_RecoverCount = 0;
	float m_RecoverDir = 180.0;

	//! Vault/climb in progress: while the climb command is active MoveTo neither
	//! steers nor monitors progress (see OnUpdate). Once IsClimbing() clears the
	//! following resumes.
	bool m_Vaulting = false;
	float m_VaultGrace = 0.0;

	//! Accumulator for the periodic movement debug log (DM_BOT_DEBUG_FSM).
	float m_DebugAccum = 0.0;

	//! Accumulator for the proactive door check (throttled by DM_DOOR_CHECK_INTERVAL).
	float m_DoorCheckAccum = 0.0;

	//! Ladder climb/descend in progress: while the UseLadder intent (EXCLUSIVE)
	//! owns the body, MoveTo is dormant; once it finishes MoveTo re-routes (see
	//! OnUpdate).
	bool m_Laddering = false;
	ref dmBotIntent_UseLadder m_UseLadder;

	override void OnStart(dmAISurvivor bot)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.MoveTo.Start");
		#endif

		m_BestDist = -1.0;
		m_NoProgressTime = 0.0;
		m_PathIdx = 0;
		m_DoorCheckAccum = 0.0;

		m_Recovering = false;
		m_RecoverTimer = 0.0;
		m_RecoverCount = 0;

		m_Vaulting = false;
		m_VaultGrace = 0.0;

		m_Laddering = false;
		m_UseLadder = null;

		m_Path = new array<vector>();
		bool hasPath = bot.FindPathTo(m_Target, m_Path);

		#ifdef DM_BOT_DEBUG_FSM
		if (!hasPath)
			dmBotLog.Debug("[FSM] MoveTo.OnStart: FindPathTo=false target=" + m_Target);
		else
			dmBotLog.Debug("[FSM] MoveTo.OnStart: target=" + m_Target + " pathPoints=" + m_Path.Count());
		#endif

		if (!hasPath || m_Path.Count() == 0)
		{
			dmBotLog.Error("MoveTo: нет пути к " + m_Target + " (вне navmesh или недостижимо), abort");
			Fail();
		}
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.MoveTo");
		#endif

		if (IsFinished())
			return;

		if (m_Recovering)
		{
			m_RecoverTimer -= pDt;
			bot.SetMove(m_RecoverDir, 1.0);

			if (m_RecoverTimer > 0.0)
				return;

			m_Recovering = false;
			if (!Recalc(bot))
			{
				dmBotLog.Error("MoveTo: восстановление не помогло, путь к " + m_Target + " недоступен, abort");
				bot.SetMove(0.0, 0.0);
				Fail();
				return;
			}

			m_NoProgressTime = 0.0;
			return;
		}

		if (m_Vaulting)
		{
			m_VaultGrace -= pDt;
			if (m_VaultGrace <= 0.0)
			{
				dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
				if (!pawn || !pawn.IsClimbing())
				{
					m_Vaulting = false;
					m_NoProgressTime = 0.0;
				}
			}
			return;
		}

		if (m_Laddering)
		{
			if (m_UseLadder && (m_UseLadder.IsFinished() || m_UseLadder.IsExpired()))
			{
				m_UseLadder = null;
				m_Laddering = false;
				m_NoProgressTime = 0.0;
				Recalc(bot);   // пере-прокладка после смены этажа
			}
			return;
		}

		m_DoorCheckAccum += pDt;
		if (m_DoorCheckAccum >= DM_DOOR_CHECK_INTERVAL)
		{
			m_DoorCheckAccum = 0.0;
			bot.TryOpenDoorOnPath();
		}

		vector subGoal = m_Path[m_PathIdx];
		float reach = m_ReachDistance;
		if (m_PathIdx < m_Path.Count() - 1)
			reach = DM_PATH_WAYPOINT_REACH;

		vector pos = bot.GetPosition();
		vector dir = subGoal - pos;
		dir[1] = 0.0;
		float dist = dir.Length();

		if (dist <= reach)
		{
			if (m_PathIdx >= m_Path.Count() - 1)
			{
				bot.SetMove(0.0, 0.0);
				Finish();
				return;
			}

			m_PathIdx++;
			m_BestDist = -1.0;
			m_NoProgressTime = 0.0;
			return;
		}

		float subYaw = dir.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float moveAngle = dmAISurvivor.AngleDiff(subYaw, bodyYaw);

		//! Body faces the movement direction (comfort policy); the head looks at the
		//! waypoint. If a higher-priority look intent holds the body (FULL), moveAngle
		//! becomes the strafe/backpedal direction instead.
		bot.SetMoveYaw(subYaw);
		bot.LookAtPoint(subGoal + Vector(0, DM_EYE_HEIGHT, 0), dmBotLookTurn.NONE);
		float speed = bot.CalcSpeed(m_Target, m_ReachDeadline);
		bot.SetMove(moveAngle, speed);

		#ifdef DM_BOT_DEBUG_FSM
		m_DebugAccum += pDt;
		if (m_DebugAccum >= 1.0)
		{
			m_DebugAccum = 0.0;
			dmBotLog.Debug("[FSM] MoveTo: subGoal=" + subGoal + " pos=" + pos + " dist=" + dist + " reach=" + reach + " pathIdx=" + m_PathIdx + " pathPoints=" + m_Path.Count());
			dmBotLog.Debug("[FSM] MoveTo: moveAngle=" + moveAngle + " speed=" + speed + " deadline=" + m_ReachDeadline);
		}
		#endif

		if (m_BestDist < 0.0)
		{
			m_BestDist = dist;
		}
		else if (dist < m_BestDist - DM_MOVE_PROGRESS_EPS)
		{
			m_BestDist = dist;
			m_NoProgressTime = 0.0;
		}
		else
		{
			m_NoProgressTime += pDt;
		}

		if (m_NoProgressTime >= DM_MOVE_STUCK_TIME)
		{
			if (m_Recovering || m_Vaulting || m_Laddering)
				return;

			dmAISurvivorBase vaultPawn = dmAISurvivorBase.Cast(bot.GetPawn());
			if (vaultPawn && vaultPawn.TryVaultClimb())
			{
				m_Vaulting = true;
				m_VaultGrace = DM_VAULT_GRACE;
				m_NoProgressTime = 0.0;
				return;
			}

			dmAISurvivorBase ladderPawn = dmAISurvivorBase.Cast(bot.GetPawn());
			if (ladderPawn && TryStartLadder(bot))
			{
				m_Laddering = true;
				m_NoProgressTime = 0.0;
				return;
			}

			if (m_RecoverCount < DM_MOVE_MAX_RECOVER)
			{
				m_RecoverCount++;
				m_Recovering = true;
				m_RecoverTimer = DM_MOVE_RECOVER_TIME;
				m_NoProgressTime = 0.0;

				m_RecoverDir = 180.0;
				if (m_RecoverCount % 2 == 0)
				{
					m_RecoverDir = 90.0;
					if (m_RecoverCount % 4 == 0)
						m_RecoverDir = -90.0;
				}

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] MoveTo: stuck, recover #" + m_RecoverCount + " dir=" + m_RecoverDir);
				#endif
				return;
			}

			dmBotLog.Error("MoveTo: застрял на пути к " + m_Target + " (подцель " + subGoal + "), abort");
			bot.SetMove(0.0, 0.0);
			Fail();
		}
	}

	bool Recalc(dmAISurvivor bot)
	{
		ref array<vector> newPath = new array<vector>();
		if (!bot.FindPathTo(m_Target, newPath) || newPath.Count() == 0)
			return false;

		m_Path = newPath;
		m_PathIdx = 0;
		m_BestDist = -1.0;
		m_NoProgressTime = 0.0;
		return true;
	}

	//! Try to start a ladder climb/descend through the building directly ahead
	//! (the stuck detector calls this when the path goes across floors). Raycasts
	//! forward, resolves the building, picks the nearest ladder entry point on the
	//! correct side (up = bottom entry, down = top entry) and spawns a
	//! dmBotIntent_UseLadder (EXCLUSIVE) to own the climb. Returns true if started.
	bool TryStartLadder(dmAISurvivor bot)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return false;

		vector pos = pawn.GetPosition();
		vector dir = pawn.GetDirection();
		dir[1] = 0.0;
		dir.Normalize();
		vector beg = pos + Vector(0.0, DM_EYE_HEIGHT, 0.0);
		vector end = beg + dir * DM_DOOR_OPEN_DIST;

		RaycastRVParams rp = new RaycastRVParams(beg, end, pawn);
		rp.sorted = true;
		rp.type = ObjIntersectView;
		rp.flags = CollisionFlags.NEARESTCONTACT;
		ref array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (!DayZPhysics.RaycastRVProxy(rp, hits) || hits.Count() == 0)
			return false;

		Building building = Building.Cast(hits[0].obj);
		if (!building)
			return false;

		ref array<ref dmBotLadder> ladders = dmBotLadderCache.GetInstance().GetLadders(building);
		if (!ladders || ladders.Count() == 0)
			return false;

		int dirSign = 1;
		if (m_Target[1] < pos[1])
			dirSign = -1;

		dmBotLadder best = null;
		float bestDist = 0.0;
		int i;
		for (i = 0; i < ladders.Count(); i++)
		{
			dmBotLadder ladder = ladders[i];
			vector modelEntry = ladder.m_Bottom;
			if (dirSign < 0)
				modelEntry = ladder.m_Top;
			vector entry = building.ModelToWorld(modelEntry);
			vector delta = entry - pos;
			delta[1] = 0.0;
			float dist = delta.Length();
			if (!best || dist < bestDist)
			{
				best = ladder;
				bestDist = dist;
			}
		}

		if (!best)
			return false;

		m_UseLadder = new dmBotIntent_UseLadder();
		m_UseLadder.m_Building = building;
		m_UseLadder.m_Ladder = best;
		m_UseLadder.m_Direction = dirSign;
		bot.AddFSMIntent(m_UseLadder);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] MoveTo: ladder building=" + building + " dir=" + dirSign + " index=" + best.m_Index);
		#endif
		return true;
	}

	override void OnCancel(dmAISurvivor bot)
	{
		bot.SetMove(0.0, 0.0);
	}
}
