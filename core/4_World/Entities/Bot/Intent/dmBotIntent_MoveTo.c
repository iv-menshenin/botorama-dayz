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

		m_MovingVisionDt += pDt;
		if ( m_MovingVisionDt > 1.0 )
		{
			MovingVision(bot);
			m_MovingVisionDt = 0.0;
		}

		if (IsFinished())
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] IsFinished движение завершено");
			#endif
			return;
		}

		if (m_Recovering)
		{
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
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug("[PATH] RePath #002");
				#endif
				m_UseLadder = null;
				m_Laddering = false;
				m_NoProgressTime = 0.0;
				RePath(bot);   // пере-прокладка после смены этажа
			}
			return;
		}

		m_DoorCheckAccum += pDt;
		if (m_DoorCheckAccum >= DM_DOOR_CHECK_INTERVAL)
		{
			m_DoorCheckAccum = 0.0;
			bot.TryOpenDoorOnPath();
		}

		UpdateGoal(bot, pDt);

		vector subGoal = m_Goal;
		if (m_HasPath && m_Path.Count() > 0)
			subGoal = m_Path[m_PathIdx];

		float reach = m_ReachDistance;
		if (m_HasPath && m_PathIdx < m_Path.Count() - 1)
			reach = DM_PATH_WAYPOINT_REACH;

		vector pos = bot.GetPosition();
		vector dir = subGoal - pos;
		dir[1] = 0.0;
		float dist = dir.Length();

		//! Fall safety: don't step off a dangerous ledge when steering directly
		//! (no navmesh path) toward a goal below.
		if (!m_HasPath && m_Goal[1] < pos[1] && IsDangerousAltitude(bot))
		{
			bot.SetMove(0.0, 0.0);
			return;
		}
		
		bool reached = IsWaypointReachedOnce(pos, subGoal);
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
				return;
			}

			OnReachedGoal(bot, subGoal);
			return;
		}

		float subYaw = dir.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float moveAngle = dmAISurvivor.AngleDiff(subYaw, bodyYaw);

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
			if (vaultPawn)
			{
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug("[PATH] TryVaultClimb нужно попытаться забраться на препятствие");
				#endif
				if ( vaultPawn.TryVaultClimb() )
				{
					m_Vaulting = true;
					m_VaultGrace = DM_VAULT_GRACE;
					m_NoProgressTime = 0.0;
					if ( dist < 0.5 )
					{
						#ifdef DM_BOT_DEBUG_PATHFINDER
						dmBotLog.Debug("[PATH] TryVaultClimb до цели очень близко, боюсь перепрыгну");
						#endif
						if (m_HasPath && m_PathIdx < m_Path.Count() - 1)
						{
							m_PathIdx++;
							m_BestDist = -1.0;
							m_NoProgressTime = 0.0;
							return;
						}

						OnReachedGoal(bot, subGoal);
						return;
					}
					return;
				}
			}

			dmAISurvivorBase ladderPawn = dmAISurvivorBase.Cast(bot.GetPawn());
			if (ladderPawn)
			{
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug("[PATH] TryVaultClimb нужно попытаться забраться на лестницу");
				#endif
				if ( TryStartLadder(bot) )
				{
					m_Laddering = true;
					m_NoProgressTime = 0.0;
					return;
				}
			}

			if (m_RecoverCount < DM_MOVE_MAX_RECOVER)
			{
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug("[PATH] Recovering я застрял, пытаюсь выбраться");
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

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] MoveTo: stuck, recover #" + m_RecoverCount + " dir=" + m_RecoverDir);
				#endif
				return;
			}

			if (IsContinuous())
			{
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug("[PATH] RePath #001");
				#endif
				m_NoProgressTime = 0.0;
				RePath(bot);
				return;
			}

			dmBotLog.Error("MoveTo: застрял на пути к " + m_Goal + " (подцель " + subGoal + "), abort");
			bot.SetMove(0.0, 0.0);
			Fail();
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
		bot.AddFSMIntent(m_UseLadder);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] MoveTo: ladder building=" + building + " dir=" + dirSign + " index=" + best.m_Index);
		#endif
		return true;
	}

	void MovingVision(dmAISurvivor bot)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("PathFinder.MovingVision");
		#endif

		vector subGoal = m_Goal;
		if (m_HasPath && m_Path.Count() > 0)
			subGoal = m_Path[m_PathIdx];
		vector pos = bot.GetPosition();
		vector dir = subGoal - pos;
		if ( dir.LengthSq() > 1.0 ) dir.Normalize();
		if ( IsPointOnNavMesh( pos + dir ) )
		{
			// none
		}
		if ( HasObstaclesToPoint( bot, pos, pos + dir ) )
		{
			// none
		}
	}

	autoptr PGFilter m_PathFilter;

    bool IsPointOnNavMesh(vector point)
    {
		if (!m_PathFilter)
		{
			m_PathFilter = new PGFilter();
			int include = PGPolyFlags.UNREACHABLE | PGPolyFlags.DISABLED | PGPolyFlags.WALK | PGPolyFlags.DOOR | PGPolyFlags.INSIDE | PGPolyFlags.LADDER;
			int exclude = PGPolyFlags.CRAWL | PGPolyFlags.CROUCH | PGPolyFlags.SWIM_SEA | PGPolyFlags.SWIM;
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

        // Raycast вниз для проверки поверхности, нужно чтобы луч уперся в NAVMESH, иначе тут нельзя ходить
        vector rayStart = point + "0 1.8 0"; // с высоты головы
        vector rayEnd = point - "0 0.5 0";   // под землю на полметра
        vector hitNormal;
        vector hitPos;
		bool hit = g_Game.GetWorld().GetAIWorld().RaycastNavMesh(rayStart, rayEnd, m_PathFilter, hitPos, hitNormal);
		if ( hit )
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("[PATH] RaycastNavMesh HIT hitPos=" + hitPos + " hitNormal=" + hitNormal);
			#endif
			return Math.AbsFloat( point[1] - hitPos[1] ) < 4.0;
		}
		return false;
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

	bool IsWaypointReachedOnce(vector pos, vector wp)
	{
		vector A = m_LastPassedPoint;
		vector B = pos;
		vector P = wp;

		// Обнуление высоты нужно, потому что высота Path и точка на которой стоит бот никогда не сходятся!
		// Но для того, чтобы не было ошибки на разных этажах, заранее сравним высоту цели и высоту позиции.
		if ( Math.AbsFloat( B[1] - P[1] ) > 1.8 )
		{
			m_LastPassedPoint = pos;
			return false;
		}
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
			dmBotLog.Debug("[PATH] Вырожденный случай: A и B совпадают distSq=" + distSq);
			#endif
			return distSq <= m_ReachDistance * m_ReachDistance;
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
		dmBotLog.Debug("[PATH] IsWaypointReached dist=" + dist + "[" + (dist <= m_ReachDistance) + "]");
		#endif
		return dist <= m_ReachDistance;
	}

	override void OnCancel(dmAISurvivor bot)
	{
		super.OnCancel(bot);

		bot.SetMove(0.0, 0.0);
	}
}
