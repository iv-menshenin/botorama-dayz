//! dmBotIntent_MoveTo — walk to a world point along a navmesh path; finishes when
//! reached. Base class for path-following intents: dmBotIntent_FollowTo inherits
//! the full movement machinery (steering, stuck detection, vault/climb, ladders,
//! recovery) and only overrides the goal (UpdateGoal), speed (GetMoveSpeed) and
//! continuity (IsContinuous).
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
	vector m_Goal;
	float m_ReachDistance = DM_PATH_WAYPOINT_REACH;
	float m_ReachDeadline = 0.0;   // seconds to reach the target; 0 = no deadline

	ref array<vector> m_Path;
	int m_PathIdx = 0;
	bool m_HasPath = false;
	float m_PathYaw;
	float m_Distance;

	float m_BestDist = -1.0;
	float m_NoProgressTime = 0.0;
	float m_AllProgressTime = 0.0;

	//! Stuck-recovery: step back/sideways for a short time before re-routing,
	//! up to DM_MOVE_MAX_RECOVER attempts (see OnUpdate).
	bool m_Recovering = false;
	float m_RecoverTimer = 0.0;
	int m_RecoverCount = 0;
	float m_RecoverDir = 180.0;

	bool m_Detouring = false;
	float m_DetourTimer = 0.0;
	int m_DetourCount = 0;
	float m_DetourDir = 90.0;

	//! Vault/climb in progress: while the climb command is active MoveTo neither
	//! steers nor monitors progress (see OnUpdate). Once IsClimbing() clears the
	//! following resumes.
	bool m_Vaulting = false;
	float m_VaultGrace = 0.0;

	//! Accumulator for the periodic movement debug log (DM_BOT_DEBUG_FSM).
	float m_DebugAccum = 0.0;

	//! Accumulator for the proactive door check (throttled by DM_DOOR_CHECK_INTERVAL).
	float m_DoorCheckAccum = 0.0;

	//! Accumulator for the proactive campfire scan (throttled by DM_DANGER_CHECK_INTERVAL).
	float m_DangerAccum = 0.0;

	//! Accumulator for the periodic re-path (throttled by DM_MOVE_REPATH_INTERVAL).
	float m_RepathAccum = 0.0;

	//! Proactive vision (ProbeAhead): climb/door candidates with a cooldown/timeout,
	//! and whether walkable ground is ahead (fall safety).
	bool m_ClimbCandidate = false;
	float m_ClimbCandidateUntil = 0.0;
	float m_VaultFallbackUntil = 0.0;
	bool m_DoorCandidate = false;
	float m_DoorCandidateUntil = 0.0;
	bool m_NoGroundAhead = false;

	//! Ladder climb/descend in progress: while the UseLadder intent (EXCLUSIVE)
	//! owns the body, MoveTo is dormant; once it finishes MoveTo re-routes (see
	//! OnUpdate).
	bool m_Laddering = false;
	ref dmBotIntent_UseLadder m_UseLadder;

	void dmBotIntent_MoveTo()
	{
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "MoveTo";
	}

	//! True for intents that keep moving forever (FollowTo) — no finish, no path
	//! requirement on start, and a stuck bot re-routes instead of failing.
	bool IsContinuous()
	{
		return false;
	}

	//! Whether the head should track the current sub-goal while moving. FollowTo
	//! disables it (the LookAround intent owns the head).
	bool KeepLookAtGoal()
	{
		return true;
	}

	//! Movement speed (0..3) toward m_Goal. FollowTo overrides it to match the
	//! target speed and the distance to the escort anchor.
	float GetMoveSpeed(dmAISurvivor bot)
	{
		return bot.CalcSpeed(m_Goal, m_ReachDeadline);
	}

	//! Called every tick after the door check, before steering. FollowTo uses it
	//! to re-derive its dynamic escort anchor (m_Goal) and re-path.
	void UpdateGoal(dmAISurvivor bot, float pDt)
	{
	}

	//! Called when the final goal is reached. Default: stop and (unless continuous)
	//! finish. Subclasses (e.g. dmBotIntent_PickUp) override to act on the goal.
	void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		bot.SetMove(0.0, 0.0);
		Finish();
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);

		m_AllProgressTime = 0.0;
		m_BestDist = -1.0;
		m_NoProgressTime = 0.0;
		m_PathIdx = 0;
		m_DoorCheckAccum = 0.0;
		m_DangerAccum = 0.0;
		m_RepathAccum = 0.0;

		m_Recovering = false;
		m_RecoverTimer = 0.0;
		m_RecoverCount = 0;

		m_Detouring = false;
		m_DetourTimer = 0.0;
		m_DetourCount = 0;

		m_Vaulting = false;
		m_VaultGrace = 0.0;

		m_Laddering = false;
		m_UseLadder = null;

		m_Path = new array<vector>();
		m_HasPath = false;

		m_LastPassedPoint = bot.GetPosition();

		if (!IsContinuous())
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] RePath #005");
			#endif
			RePath(bot);
			if (!m_HasPath)
			{
				dmBotLog.Error("MoveTo: нет пути к " + m_Goal + " (вне navmesh или недостижимо), abort");
				Fail();
			}
		}
	}

	float m_MovingVisionDt = 0;

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intent.MoveTo");
		#endif

		m_AllProgressTime += pDt;
		TickVision(bot, pDt);

		if (IsFinished()) return;
		if (m_Recovering) { TickRecover(bot, pDt); return; }
		if (m_Detouring)  { TickDetour(bot, pDt);  return; }
		if (m_Vaulting)   { TickVault(bot, pDt);   return; }
		if (m_Laddering)  { TickLadder(bot, pDt);  return; }

		TickMove(bot, pDt);
	}

	//! Stuck recovery step-back/sideways: moves for DM_MOVE_RECOVER_TIME, then
	//! re-routes. Runs while m_Recovering is set (see ResolveStuck).
	void TickRecover(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] TickRecover m_RecoverDir=" + m_RecoverDir);
		#endif
		m_RecoverTimer -= pDt;
		bot.SetMove(m_RecoverDir, 1.0);

		if (m_RecoverTimer > 0.0)
			return;

		m_Recovering = false;
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] RePath #003");
		#endif
		RePath(bot);
		if (!m_HasPath && !IsContinuous())
		{
			dmBotLog.Error("MoveTo: восстановление не помогло, путь к " + m_Goal + " недоступен, abort");
			bot.SetMove(0.0, 0.0);
			Fail();
			return;
		}

		m_NoProgressTime = 0.0;
	}

	//! Lateral detour: sidestep perpendicular to the facing (left/right) for a longer
	//! distance than the short recovery step, to walk AROUND an obstacle the bot
	//! cannot vault/climb. Runs while m_Detouring is set (see ResolveStuck).
	void TickDetour(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] TickDetour m_DetourDir=" + m_DetourDir);
		#endif
		m_DetourTimer -= pDt;
		bot.SetMove(m_DetourDir, 1.0);

		if (m_DetourTimer > 0.0)
			return;

		m_Detouring = false;
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] RePath #006");
		#endif
		RePath(bot);
		if (!m_HasPath && !IsContinuous())
		{
			dmBotLog.Error("MoveTo: detour не помог, путь к " + m_Goal + " недоступен, abort");
			bot.SetMove(0.0, 0.0);
			Fail();
			return;
		}

		m_NoProgressTime = 0.0;
	}

	//! Vault/climb in progress: wait for the grace period, then stop vaulting once
	//! the climb command is no longer active (IsClimbing clears).
	void TickVault(dmAISurvivor bot, float pDt)
	{
		m_VaultGrace -= pDt;
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] TickVault m_VaultGrace=" + m_VaultGrace);
		#endif
		if (m_VaultGrace <= 0.0)
		{
			dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
			if (!pawn || !pawn.IsClimbing())
			{
				m_Vaulting = false;
				m_NoProgressTime = 0.0;
			}
		}
	}

	//! Ladder climb/descend in progress: while the UseLadder intent runs, MoveTo is
	//! dormant; once it finishes/expires, re-route (floor changed).
	void TickLadder(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_DEBUG_PATHFINDER
		if ( m_UseLadder )
			dmBotLog.Debug("[PATH] TickLadder m_Phase=" + m_UseLadder.m_Phase);
		#endif
		if (m_UseLadder && (m_UseLadder.IsFinished() || m_UseLadder.IsExpired()))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] RePath #002");
			#endif
			m_UseLadder = null;
			m_Laddering = false;
			m_NoProgressTime = 0.0;
			RePath(bot);   // пере-прокладка после смены этажа
		}
	}

	void CalcDistance2D(dmAISurvivor bot)
	{
		vector pos = bot.GetPosition();
		pos[1] = 0.0;

		vector subGoal = m_Goal;
		if (m_HasPath && m_Path.Count() > 0)
			subGoal = m_Path[m_PathIdx];
		subGoal[1] = 0.0;

		vector dir = subGoal - pos;
		m_Distance = dir.Length();
	}

	float m_TooCloseTime = 0.0;

	//! Normal steering: proactive door check, goal re-derive, steer toward the
	//! current waypoint, per-waypoint progress monitor and stuck resolution.
	void TickMove(dmAISurvivor bot, float pDt)
	{
		m_DoorCheckAccum += pDt;
		if (m_DoorCheckAccum >= DM_DOOR_CHECK_INTERVAL)
		{
			m_DoorCheckAccum = 0.0;
			bot.TryOpenDoorOnPath();
		}

		UpdateGoal(bot, pDt);

		//! Периодический ре-патинг: маршрут устаревает (двери, костры, препятствия).
		//! Только для НЕ-непрерывных интентов — FollowTo ре-патит сам по дрейфу якоря.
		m_RepathAccum += pDt;
		if (!IsContinuous() && m_RepathAccum >= DM_MOVE_REPATH_INTERVAL)
		{
			m_RepathAccum = 0.0;
			RePath(bot);
			return;
		}

		vector subGoal = m_Goal;
		if (m_HasPath && m_Path.Count() > 0)
			subGoal = m_Path[m_PathIdx];

		float reach = m_ReachDistance;
		if (m_HasPath && m_PathIdx < m_Path.Count() - 1)
			reach = DM_PATH_WAYPOINT_REACH;

		vector pos = bot.GetPosition();

		//! Проактивное избегание костра: детектим горящий костёр у подцели,
		//! запоминаем в красную зону и сдвигаем подцель вбок от костра.
		m_DangerAccum += pDt;
		if (m_DangerAccum >= DM_DANGER_CHECK_INTERVAL)
		{
			m_DangerAccum = 0.0;
			FireplaceBase fire = dmRedZone.ScanFireplace(subGoal, DM_BOT_DANGER_AVOID_RADIUS * 2.0);
			if (fire)
				dmRedZone.Add(fire.GetPosition(), DM_BOT_DANGER_AVOID_RADIUS, DM_BOT_DANGER_TIMEOUT);
		}

		vector avoidCenter;
		float avoidRadius;
		if (dmRedZone.FindNearest(subGoal, avoidCenter, avoidRadius))
		{
			vector toCenter = subGoal - avoidCenter;
			toCenter[1] = 0.0;
			float dc = toCenter.Length();
			if (dc < avoidRadius + DM_BOT_DANGER_MARGIN)
			{
				vector away = toCenter;
				if (away.Length() < 0.01)
					away = pos - avoidCenter;   // дегенеративный случай: от костра относительно бота
				away[1] = 0.0;
				away.Normalize();
				subGoal = avoidCenter + away * (avoidRadius + DM_BOT_DANGER_MARGIN);
				subGoal[1] = pos[1];
			}
		}

		vector dir = subGoal - pos;
		dir[1] = 0.0;
		float dist = dir.Length();
		CalcDistance2D( bot );

		//! Fall safety: the proactive vision probe found no walkable ground ahead —
		//! stop instead of stepping off a ledge.
		if (m_NoGroundAhead)
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] TickMove STOP m_NoGroundAhead=" + m_NoGroundAhead);
			#endif
			bot.SetMove(0.0, 0.0);
			return;
		}

		if ( m_Distance < 1.0 )
		{
			m_TooCloseTime += pDt;
			if ( m_TooCloseTime > DM_MOVE_TOO_CLOSE_MAX )
				m_TooCloseTime = DM_MOVE_TOO_CLOSE_MAX;
		} else {
			m_TooCloseTime = 0.0;
		}
		bool reached = IsWaypointReachedOnce(pos, subGoal, reach * (1.0 + m_TooCloseTime / 2.0));
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] Иду к точке: subGoal=" + subGoal + " pos=" + pos + " reached=" + reached);
		#endif

		if ( reached )
		{
			if (m_HasPath && m_PathIdx < m_Path.Count() - 1)
			{
				m_PathIdx++;
				m_BestDist = -1.0;
				m_NoProgressTime = 0.0;
				m_TooCloseTime = 0.0;
				m_FoolVaulted = false;
				return;
			}

			OnReachedGoal(bot, subGoal);
			return;
		}

		float subYaw = dir.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float moveAngle = dmAISurvivor.AngleDiff(subYaw, bodyYaw);
		m_PathYaw = subYaw;

		//! Body faces the movement direction (comfort policy); the head looks at the
		//! waypoint. If a higher-priority look intent holds the body (FULL), moveAngle
		//! becomes the strafe/backpedal direction instead.
		bot.SetMoveYaw(subYaw);
		if (KeepLookAtGoal())
			bot.LookAtPoint(subGoal + Vector(0, DM_EYE_HEIGHT, 0), dmBotLookTurn.NONE);
		float speed = GetMoveSpeed(bot);
		bot.SetMove(moveAngle, speed);

		#ifdef DM_BOT_DEBUG_FSM
		m_DebugAccum += pDt;
		if (m_DebugAccum >= 1.0)
		{
			m_DebugAccum = 0.0;
			int pathPoints = 0;
			if (m_Path)
				pathPoints = m_Path.Count();
			dmBotLog.Debug("[FSM] MoveTo: subGoal=" + subGoal + " pos=" + pos + " dist=" + dist + " reach=" + reach + " pathIdx=" + m_PathIdx + " pathPoints=" + pathPoints);
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
			m_FoolVaulted = false;
		}
		else
		{
			if ( m_AllProgressTime > 1.0 ) m_NoProgressTime += pDt;
		}

		if (m_NoProgressTime >= DM_MOVE_STUCK_TIME && dist > DM_MOVE_STUCK_MIN_DIST)
		{
			ResolveStuck(bot);
			return;
		}
	}

	//! Decision cascade when the bot is stuck: door first, then vault/climb, then
	//! ladder, then a step-back recovery, and finally abort/re-path.
	void ResolveStuck(dmAISurvivor bot)
	{
		if (TryOpenDoorAhead(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] TryOpenDoorAhead pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryVaultOrClimb(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] TryVaultOrClimb pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryVaultFallback(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] TryVaultFallback pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryStartLadder(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] TryStartLadder pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryRecover(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] TryRecover pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryDetour(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] TryDetour pos=" + bot.GetPosition());
			#endif
			return;
		}
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] TryAbortOrRepath pos=" + bot.GetPosition());
		#endif
		TryAbortOrRepath(bot);
	}

	//! Open a closed door flagged by ProbeAhead (within the timeout).
	bool TryOpenDoorAhead(dmAISurvivor bot)
	{
		float now = GetGame().GetTickTime();
		if (!m_DoorCandidate || now > m_DoorCandidateUntil)
			return false;
		return bot.TryOpenDoorOnPath();
	}

	bool m_FoolVaulted = false;
	float m_FoolVaultedTime = 0.6;

	//! Vault/climb the obstacle flagged by ProbeAhead (within the cooldown).
	bool TryVaultOrClimb(dmAISurvivor bot)
	{
		float now = GetGame().GetTickTime();
		if (!m_ClimbCandidate || now > m_ClimbCandidateUntil)
		{
			if ( m_NoProgressTime < m_FoolVaultedTime || m_FoolVaulted )
				return false;
			m_FoolVaulted = true;
		}
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] Vault по флагу зрения t=" + m_NoProgressTime);
		if ( bot.m_DebugPlayer )
			GetGame().ChatMP(bot.m_DebugPlayer, "Перепрыгнуть t=" + m_NoProgressTime, "colorAction");
		#endif
		
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn || !pawn.TryVaultClimb(m_PathYaw))
			return false;
		m_Vaulting = true;
		m_VaultGrace = DM_VAULT_GRACE;
		m_NoProgressTime = 0.0;
		return true;
	}

	//! Direct vault/climb fallback: the vision probe can miss a low obstacle, so when
	//! stuck we also run the engine climb test directly (DoClimbTest), throttled to
	//! avoid spamming the native test every stuck tick.
	bool TryVaultFallback(dmAISurvivor bot)
	{
		float now = GetGame().GetTickTime();
		if (now < m_VaultFallbackUntil)
			return false;
		m_VaultFallbackUntil = now + DM_CLIMB_FLAG_COOLDOWN;

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] Fallback на Vault t=" + m_NoProgressTime);
		if ( bot.m_DebugPlayer )
			GetGame().ChatMP(bot.m_DebugPlayer, "Перепрыгнуть? t=" + m_NoProgressTime, "colorAction");
		#endif

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn || !pawn.TryVaultClimb(m_PathYaw))
			return false;

		m_Vaulting = true;
		m_VaultGrace = DM_VAULT_GRACE;
		m_NoProgressTime = 0.0;
		return true;
	}

	//! Start a step-back/sideways recovery (if attempts remain). Returns true when
	//! recovery started.
	bool TryRecover(dmAISurvivor bot)
	{
		if (m_RecoverCount < DM_MOVE_MAX_RECOVER)
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] Recovering я застрял, пытаюсь выбраться");
			if ( bot.m_DebugPlayer )
				GetGame().ChatMP(bot.m_DebugPlayer, "Я застрял t=" + m_NoProgressTime, "colorAction");
			#endif
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
			return true;
		}
		return false;
	}

	//! Start a lateral detour (if attempts remain): sidestep perpendicular to the
	//! facing for DM_MOVE_DETOUR_TIME, then re-route. Returns true when started.
	bool TryDetour(dmAISurvivor bot)
	{
		if (m_DetourCount < DM_MOVE_MAX_DETOUR)
		{
			m_DetourCount++;
			m_Detouring = true;
			m_DetourTimer = DM_MOVE_DETOUR_TIME;
			m_NoProgressTime = 0.0;

			m_DetourDir = 90.0;
			if (m_DetourCount % 2 == 0)
				m_DetourDir = -90.0;

			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] MoveTo: detour #" + m_DetourCount + " dir=" + m_DetourDir);
			if ( bot.m_DebugPlayer )
				GetGame().ChatMP(bot.m_DebugPlayer, "Обход сбоку t=" + m_NoProgressTime, "colorAction");
			#endif
			return true;
		}
		return false;
	}

	//! Last resort: continuous intents re-path and keep going; one-shot intents
	//! abort. Returns true if the bot keeps going (re-pathed), false if aborted.
	bool TryAbortOrRepath(dmAISurvivor bot)
	{
		if (IsContinuous())
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] RePath #001 t=" + m_NoProgressTime);
			if ( bot.m_DebugPlayer )
				GetGame().ChatMP(bot.m_DebugPlayer, "Изменение пути t=" + m_NoProgressTime, "colorAction");
			#endif
			m_NoProgressTime = 0.0;
			RePath(bot);
			return true;
		}

		dmBotLog.Error("MoveTo: застрял на пути к " + m_Goal + ", abort");
		bot.SetMove(0.0, 0.0);
		Fail();
		return false;
	}

	//! Proactive vision throttle: probe ahead (navmesh ground + climb/door rays) at
	//! a fixed interval.
	void TickVision(dmAISurvivor bot, float pDt)
	{
		m_MovingVisionDt += pDt;
		if (m_MovingVisionDt < DM_MOVE_VISION_INTERVAL)
			return;
		pDt = m_MovingVisionDt;
		m_MovingVisionDt = 0.0;
		ProbeAhead(bot, pDt);
	}

	float m_NoNavMeshBug = 0.0;
	float m_DoNotCheckNavMesh = 0.0;

	//! Proactive look ahead: probe the point just ahead of the bot for walkable
	//! ground (fall safety), a climbable obstacle (low upward ray) and a closed door
	//! (eye ray). Sets m_NoGroundAhead and the m_ClimbCandidate/m_DoorCandidate flags.
	void ProbeAhead(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("MoveTo.Vision");
		#endif
		vector subGoal = m_Goal;
		if (m_HasPath && m_Path.Count() > 0)
			subGoal = m_Path[m_PathIdx];
		vector pos = bot.GetPosition();
		vector dir = subGoal - pos;
		
		float distTo = dir.Length();
		if (distTo < 0.01)
			return;

		if (distTo > 3.0)
			dir[1] = 0.0;

		dir.Normalize();
		float probeDist = Math.Min(1.0, distTo + 0.1);
		vector probe = pos + dir * probeDist;

		if ( m_DoNotCheckNavMesh > 0.0 )
		{
			m_NoGroundAhead = false;
			m_DoNotCheckNavMesh -= pDt;
		} else {
			m_NoGroundAhead = !IsPointOnNavMesh(probe);
		}

		if ( m_NoGroundAhead )
		{
			m_NoNavMeshBug += pDt;
			if ( m_NoNavMeshBug > 5.0 )
			{
				m_DoNotCheckNavMesh = 15.0;
			}
		} else {
			m_NoNavMeshBug = 0.0;
		}

		float now = GetGame().GetTickTime();

		//! Низкий луч (под углом вверх 0.3 -> 1.0): кандидат на карабканье.
		RaycastRVParams low = new RaycastRVParams(pos + Vector(0.0, 0.3, 0.0), probe + Vector(0.0, 1.0, 0.0), bot.GetPawn());
		low.flags = CollisionFlags.ALLOBJECTS;
		ref array<ref RaycastRVResult> lowHits = new array<ref RaycastRVResult>;
		if (DayZPhysics.RaycastRVProxy(low, lowHits) && lowHits.Count() > 0 && lowHits[0].obj)
		{
			m_ClimbCandidate = true;
			m_ClimbCandidateUntil = now + DM_CLIMB_FLAG_COOLDOWN;
		}

		//! Toe-луч у земли: ловит низкие перегородки, которые верхние лучи пропускают.
		// RaycastRVParams toe = new RaycastRVParams(pos + Vector(0.0, DM_MOVE_PROBE_TOE_Y, 0.0), probe + Vector(0.0, DM_MOVE_PROBE_TOE_Y, 0.0), bot.GetPawn());
		// toe.flags = CollisionFlags.ALLOBJECTS;
		// ref array<ref RaycastRVResult> toeHits = new array<ref RaycastRVResult>;
		// if (DayZPhysics.RaycastRVProxy(toe, toeHits) && toeHits.Count() > 0 && toeHits[0].obj)
		// {
		// 	m_ClimbCandidate = true;
		// 	m_ClimbCandidateUntil = now + DM_CLIMB_FLAG_COOLDOWN;
		// }

		//! Глазной луч (1.5м): дверь.
		RaycastRVParams eye = new RaycastRVParams(pos + Vector(0.0, 1.5, 0.0), probe + Vector(0.0, 1.5, 0.0), bot.GetPawn());
		eye.flags = CollisionFlags.ALLOBJECTS;
		ref array<ref RaycastRVResult> eyeHits = new array<ref RaycastRVResult>;
		if (DayZPhysics.RaycastRVProxy(eye, eyeHits) && eyeHits.Count() > 0)
		{
			Building building = Building.Cast(eyeHits[0].obj);
			if (building)
			{
				int doorIdx = building.GetDoorIndex(eyeHits[0].component);
				if (doorIdx >= 0 && !building.IsDoorOpen(doorIdx))
				{
					m_DoorCandidate = true;
					m_DoorCandidateUntil = now + DM_DOOR_FLAG_TIMEOUT;
				}
			}
		}
	}

	//! Re-aim the navmesh path at m_Goal. On failure m_HasPath becomes false and
	//! m_Path null — the steering then moves directly toward m_Goal.
	void RePath(dmAISurvivor bot)
	{
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] RePath invoked");
		#endif
		ref array<vector> newPath = new array<vector>();
		if (bot.FindPathTo(m_Goal, newPath) && newPath.Count() > 0)
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] FindPathTo выполнено успешно, обнаружено " + newPath.Count() + " сегментов в пути");
			#endif
			m_Path = newPath;
			m_PathIdx = 0;
			m_HasPath = true;
			RoundPath();
			return;
		}

		//! No navmesh path: allow a deliberate "leap of faith" drop only if the
		//! goal is below and the fall is safe. Otherwise the target is unreachable
		//! (m_HasPath stays false but we don't attempt to step off a drop).
		if (TryLeapOfFaith(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] TryLeapOfFaith выполнено - высота падения не опасна");
			#endif
			m_HasPath = false;
			m_Path = null;
			return;
		}

		m_HasPath = false;
		m_Path = null;
	}

	//! Post-process a freshly computed path: round sharp corners (>= DM_PATH_ROUND_ANGLE_LOW)
	//! by inserting intermediate waypoints (overshoot + 90° stair-steps) so the bot
	//! doesn't jam into fence corners or vault them. Only the first
	//! DM_PATH_ROUND_LOOKAHEAD meters are rounded; the rest is copied as-is.
	void RoundPath()
	{
		if (!m_Path || m_Path.Count() < 3)
			return;

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] RoundPath: count=" + m_Path.Count());
		#endif

		ref array<vector> rounded = new array<vector>();
		vector prev = m_Path[0];
		rounded.Insert(prev);

		float accum = 0.0;
		int i;
		int count = m_Path.Count();
		for (i = 1; i < count - 1; i++)
		{
			vector B = m_Path[i];
			vector C = m_Path[i + 1];

			vector seg = B - prev;
			seg[1] = 0.0;
			accum += seg.Length();
			if (accum > DM_PATH_ROUND_LOOKAHEAD)
			{
				int j;
				for (j = i; j < count; j++)
					rounded.Insert(m_Path[j]);
				m_Path = rounded;
				return;
			}

			vector dirIn = B - prev;
			dirIn[1] = 0.0;
			vector dirOut = C - B;
			dirOut[1] = 0.0;
			if (dirIn.Length() < 0.001 || dirOut.Length() < 0.001)
			{
				rounded.Insert(B);
				prev = B;
				continue;
			}
			dirIn.Normalize();
			dirOut.Normalize();

			float yawIn = dirIn.VectorToAngles()[0];
			float yawOut = dirOut.VectorToAngles()[0];
			float turn = Math.AbsFloat(dmAISurvivor.AngleDiff(yawOut, yawIn));

			if (turn < DM_PATH_ROUND_ANGLE_LOW)
			{
				rounded.Insert(B);
				prev = B;
				continue;
			}

			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] RoundPath: turn=" + turn);
			#endif

			vector P1 = B + dirIn * DM_PATH_ROUND_STEP;
			if (turn <= DM_PATH_ROUND_ANGLE_HIGH)
			{
				InsertRounded(rounded, P1, B);
				prev = rounded[rounded.Count() - 1];
				continue;
			}

			vector dir90 = Rotate90Toward(dirIn, dirOut);
			vector P2 = P1 + dir90 * DM_PATH_ROUND_STEP;
			InsertRounded(rounded, P1, B);
			InsertRounded(rounded, P2, B);
			prev = rounded[rounded.Count() - 1];
		}

		rounded.Insert(m_Path[count - 1]);
		m_Path = rounded;
	}

	//! Rotate a horizontal unit vector ±90° in the XZ plane, picking the side that
	//! points toward dirOut (the perpendicular with a non-negative dot product).
	vector Rotate90Toward(vector dirIn, vector dirOut)
	{
		vector rPlus = Vector(-dirIn[2], 0.0, dirIn[0]);
		float dotPlus = rPlus[0] * dirOut[0] + rPlus[2] * dirOut[2];
		if (dotPlus >= 0.0)
			return rPlus;
		return Vector(dirIn[2], 0.0, -dirIn[0]);
	}

	//! Insert a rounded point, falling back to the original corner when the rounded
	//! point is not walkable (off-navmesh).
	void InsertRounded(inout array<vector> dest, vector point, vector fallback)
	{
		if (IsPointOnNavMesh(point))
			dest.Insert(point);
		else
			dest.Insert(fallback);
	}

	//! Try to step off a ledge ("leap of faith") when there is no navmesh path
	//! down. Only allows the drop when the goal is clearly below the bot and the
	//! fall height (to the surface directly underneath) is safe.
	bool TryLeapOfFaith(dmAISurvivor bot)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("PathFinder.LeapOfFaith");
		#endif

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return false;

		vector botPos = pawn.GetPosition();

		//! Goal must be below the bot (a real vertical gap).
		if (m_Goal[1] >= botPos[1] - 0.5)
			return false;

		//! Find the surface directly below (floor/terrain) and check the fall height.
		vector beg = botPos + Vector(0.0, 0.5, 0.0);
		vector end = botPos + Vector(0.0, -50.0, 0.0);
		vector contactPos;
		vector contactDir;
		int contactComponent;
		if (!DayZPhysics.RaycastRV(beg, end, contactPos, contactDir, contactComponent, null, null, pawn, false, false, ObjIntersectGeom))
			return false;

		float fallHeight = botPos[1] - contactPos[1];
		return fallHeight < DM_BOT_FALL_HEIGHT_LOW;
	}

	//! True when the surface directly below the bot is far enough down that a fall
	//! would be dangerous (>= DM_BOT_FALL_HEIGHT_LOW). Used by the fall-safety guard.
	bool IsDangerousAltitude(dmAISurvivor bot)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return false;

		vector botPos = pawn.GetPosition();
		vector beg = botPos + Vector(0.0, 0.5, 0.0);
		vector end = botPos + Vector(0.0, -50.0, 0.0);
		vector contactPos;
		vector contactDir;
		int contactComponent;
		if (!DayZPhysics.RaycastRV(beg, end, contactPos, contactDir, contactComponent, null, null, pawn, false, false, ObjIntersectGeom))
			return false;

		float fallHeight = botPos[1] - contactPos[1];
		return fallHeight >= DM_BOT_FALL_HEIGHT_LOW;
	}

	//! Try to start a ladder climb/descend (the stuck detector calls this when the
	//! path goes across floors). Resolves the building from the floor entity under
	//! the bot, falling back to a forward raycast when approaching from outside,
	//! then picks the ladder entry point with the lowest 2D-distance x height
	//! difference on the correct side (up = bottom entry, down = top entry) and
	//! spawns a dmBotIntent_UseLadder (EXCLUSIVE) to own the climb. Returns true if
	//! started.
	bool TryStartLadder(dmAISurvivor bot)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("PathFinder.StartLadder");
		#endif

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return false;

		vector botPos = pawn.GetPosition();

		Building building;
		IEntity floor = pawn.PhysicsGetFloorEntity();
		if (floor)
			building = Building.Cast(floor);

		ref array<ref dmBotLadder> ladders;
		if (building)
			ladders = dmBotLadderCache.GetInstance().GetLadders(building);

		if (!building || !ladders || ladders.Count() == 0)
		{
			vector dir = pawn.GetDirection();
			dir[1] = 0.0;
			dir.Normalize();
			vector beg = botPos + Vector(0.0, DM_EYE_HEIGHT, 0.0);
			vector end = beg + dir * DM_DOOR_OPEN_DIST;

			RaycastRVParams rp = new RaycastRVParams(beg, end, pawn);
			rp.sorted = true;
			rp.type = ObjIntersectView;
			rp.flags = CollisionFlags.NEARESTCONTACT;
			ref array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
			if (DayZPhysics.RaycastRVProxy(rp, hits) && hits.Count() > 0)
			{
				building = Building.Cast(hits[0].obj);
				if (building)
					ladders = dmBotLadderCache.GetInstance().GetLadders(building);
			}
		}

		if (!building || !ladders || ladders.Count() == 0)
			return false;

		int dirSign = 1;
		if (m_Goal[1] < botPos[1])
			dirSign = -1;

		dmBotLadder best = null;
		float bestWeight = 0.0;
		int i;
		for (i = 0; i < ladders.Count(); i++)
		{
			dmBotLadder ladder = ladders[i];
			vector modelEntry = ladder.m_Bottom;
			if (dirSign < 0)
				modelEntry = ladder.m_Top;
			vector entry = building.ModelToWorld(modelEntry);
			float dx = entry[0] - botPos[0];
			float dz = entry[2] - botPos[2];
			float dy = Math.AbsFloat(entry[1] - botPos[1]);
			float weight = (dx * dx + dz * dz) * dy;
			if (!best || weight < bestWeight)
			{
				best = ladder;
				bestWeight = weight;
			}
		}

		if (!best)
			return false;

		m_UseLadder = new dmBotIntent_UseLadder();
		m_UseLadder.m_Building = building;
		m_UseLadder.m_Ladder = best;
		m_UseLadder.m_Direction = dirSign;
		//! Personality (не FSM): переживает ClearFSMIntents() при переходе FSM, иначе
		//! бот отменяется посреди подъёма и зависает на лестнице. Эталон — OpenDoor.
		bot.AddPersonalityIntent(m_UseLadder);
		m_Laddering = true;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] MoveTo: ladder building=" + building + " dir=" + dirSign + " index=" + best.m_Index);
		#endif
		return true;
	}

	ref PGFilter m_PathFilter;

    bool IsPointOnNavMesh(vector point)
    {
		if (!m_PathFilter)
		{
			m_PathFilter = new PGFilter();
			int include = PGPolyFlags.UNREACHABLE | PGPolyFlags.WALK | PGPolyFlags.DOOR | PGPolyFlags.INSIDE | PGPolyFlags.DISABLED | PGPolyFlags.LADDER;
			int exclude = PGPolyFlags.SWIM | PGPolyFlags.SWIM_SEA | PGPolyFlags.CRAWL | PGPolyFlags.CROUCH;
			int exclusive = PGPolyFlags.NONE;

			m_PathFilter.SetCost(PGAreaType.LADDER, 1.0);
			m_PathFilter.SetCost(PGAreaType.CRAWL, 10.0);
			m_PathFilter.SetCost(PGAreaType.CROUCH, 10.0);
			m_PathFilter.SetCost(PGAreaType.FENCE_WALL, 5.0);  //! Vault
			m_PathFilter.SetCost(PGAreaType.JUMP, 10.0);  //! Climb
			m_PathFilter.SetCost(PGAreaType.WATER, 5.0);
			m_PathFilter.SetCost(PGAreaType.WATER_DEEP, 10.0);
			m_PathFilter.SetCost(PGAreaType.WATER_SEA, 5.0);
			m_PathFilter.SetCost(PGAreaType.WATER_SEA_DEEP, 10.0);

			m_PathFilter.SetCost(PGAreaType.DOOR_CLOSED, 4.0);
			m_PathFilter.SetCost(PGAreaType.DOOR_OPENED, 10000.0);

			m_PathFilter.SetCost(PGAreaType.ROADWAY, 4.0);
			m_PathFilter.SetCost(PGAreaType.TREE, 1.0);

			m_PathFilter.SetCost(PGAreaType.OBJECTS_NOFFCON, 5.0);
			m_PathFilter.SetCost(PGAreaType.OBJECTS, 5.0);
			m_PathFilter.SetCost(PGAreaType.TERRAIN, 4.0);
			m_PathFilter.SetCost(PGAreaType.BUILDING, 4.0);
			m_PathFilter.SetCost(PGAreaType.ROADWAY_BUILDING, 1.0);

			m_PathFilter.SetFlags(include, exclude, exclusive);
		}

		//! Ground probe: find the closest navmesh point within a small radius.
		//! RaycastNavMesh detects navmesh EDGES (polygon boundaries), NOT the flat
		//! surface, so a vertical ray NOHITs on flat ground. SampleNavmeshPosition
		//! returns the closest point on the navmesh — the correct "is there a
		//! walkable surface here" check.
		vector sampled;
		if (!g_Game.GetWorld().GetAIWorld().SampleNavmeshPosition(point, DM_MOVE_GROUND_PROBE_RADIUS, m_PathFilter, sampled))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] SampleNavmeshPosition NOHIT point=" + point);
			#endif
			return false;
		}
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] SampleNavmeshPosition HIT sampled=" + sampled + " point=" + point + " diff=" + (Math.AbsFloat(point[1] - sampled[1])));
		#endif
		return Math.AbsFloat(point[1] - sampled[1]) < DM_MOVE_GROUND_PROBE_Y;
    }

	bool HasObstaclesToPoint(dmAISurvivor bot, vector from, vector to)
    {
		float h = from[1] + 0.5;
		from[1] = h;
		to[1] = h;
        
        RaycastRVParams params = new RaycastRVParams(from, to, bot.GetPawn(), 0.3);
        params.flags = CollisionFlags.ALLOBJECTS;
        
        array<ref RaycastRVResult> results = {};
        if (DayZPhysics.RaycastRVProxy(params, results))
        {
            foreach (RaycastRVResult result : results)
            {
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug("[PATH] HasObstaclesToPoint HIT Pos=" + result.pos + " hitNormal=" + result.obj.GetType() + " comp=" + result.component);
				#endif
            }
        }
        
        return false;
    }

	vector m_LastPassedPoint;

	bool IsWaypointReachedOnce(vector pos, vector wp, float reachDistance)
	{
		vector A = m_LastPassedPoint;
		vector B = pos;
		vector P = wp;

		// Обнуление высоты нужно, потому что высота Path и точка на которой стоит бот никогда не сходятся!
		// Но для того, чтобы не было ошибки на разных этажах, заранее сравним высоту цели и высоту позиции.
		// if ( Math.AbsFloat( B[1] - P[1] ) > 1.8 )
		// {
		// 	#ifdef DM_BOT_DEBUG_PATHFINDER
		// 	dmBotLog.Debug("[PATH] Разность высоты dH=" + Math.AbsFloat( B[1] - P[1] ));
		// 	#endif
		// 	m_LastPassedPoint = pos;
		// 	return false;
		// } - Все бы ничего, но иногда путь строится по воздуху. Нужно каждый вейпоинт проверять по навмеш
		A[1] = 0;
		B[1] = 0;
		P[1] = 0;

		m_LastPassedPoint = pos;

		// Направляющий вектор отрезка: d = B - A
		float dx = B[0] - A[0];
		float dy = B[1] - A[1];
		float dz = B[2] - A[2];

		// Квадрат длины направляющего вектора
		float dd = dx * dx + dy * dy + dz * dz;

		// Вектор от A к P: v = P - A
		float vx = P[0] - A[0];
		float vy = P[1] - A[1];
		float vz = P[2] - A[2];

		// Вырожденный случай: A и B совпадают
		if (dd < 0.0001 ) {
			float distSq = vector.DistanceSq(B, P);
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] Вырожденный случай: A и B совпадают distSq=" + distSq + " reachDistanceSq=" + (reachDistance * reachDistance));
			#endif
			return distSq <= reachDistance * reachDistance;
		}

		// Параметр проекции t = (v · d) / (d · d)
		float t = (vx * dx + vy * dy + vz * dz) / dd;

		// Если проекция не попадает на отрезок — точка не на отрезке
		if (t < 0.0 || t > 1.0) return false;

		// Перпендикулярное расстояние: |d × v| / |d|
		float cross_x = dy * vz - dz * vy;
		float cross_y = dz * vx - dx * vz;
		float cross_z = dx * vy - dy * vx;

		float cross_len = Math.Sqrt(cross_x * cross_x + cross_y * cross_y + cross_z * cross_z);
		float seg_len = Math.Sqrt(dd);
		float dist = cross_len / seg_len;

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("[PATH] IsWaypointReached dist=" + dist + "[" + (dist <= reachDistance) + "]");
		#endif
		return dist <= reachDistance;
	}

	override void OnCancel(dmAISurvivor bot)
	{
		super.OnCancel(bot);

		bot.SetMove(0.0, 0.0);
	}
}
