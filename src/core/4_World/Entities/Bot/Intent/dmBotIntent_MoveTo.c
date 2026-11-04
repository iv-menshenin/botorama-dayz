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
	//! Троттлинг always-on error-логов «нет пути»: уникальный гол (тип сообщения +
	//! m_Goal) логируется не чаще раза в DM_MOVE_NOPATH_LOG_INTERVAL секунд. Иначе
	//! settlement-боты, зацикленные на патруль к недостижимой точке (вне navmesh),
	//! пересоздают MoveTo каждую итерацию и спамят один и тот же error каждый ретрай.
	static const float DM_MOVE_NOPATH_LOG_INTERVAL = 30.0;
	static ref map<string, float> s_NoPathLog;

#ifdef DM_BOT_DEBUG_MOTION
	//! Cooldown (static) для событий [MOTION] pathfail: ключ "reason:botName",
	//! чтобы одно событие (nopath/fallfear) не спамилось чаще раза в
	//! DM_MOTION_PATHFAIL_COOLDOWN. Статик — переживает пересоздание интента
	//! (MoveTo пересоздаётся на каждый ретрай, см. ShouldLogNoPath).
	static ref map<string, float> s_MotionPathfailCooldown;
#endif

	vector m_Goal;
	float m_ReachDistance = DM_PATH_WAYPOINT_REACH;
	float m_ReachDeadline = 0.0;   // seconds to reach the target; 0 = no deadline

	ref array<vector> m_Path;
	int m_PathIdx = 0;
	bool m_HasPath = false;
	ref array<ref dmBotRouteSegment> m_Route;   // полный маршрут (сегменты)
	int m_RouteIdx = 0;                          // индекс текущего navmesh-сегмента
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

	//! Door-press recovery: an open door pinned the bot — closed it, then a
	//! forward sprint burst to clear the doorway, then re-route.
	bool m_DoorRecovering = false;
	float m_DoorRecoverTimer = 0.0;

	bool m_SteppingBack = false;    // отходим назад от края (fall-safe)
	float m_StepBackTimer = 0.0;
	bool m_StepBackDone = false;    // уже отходили один раз (без зацикливания)

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

	//! Accumulator for the fall-safety check (throttled by DM_FALL_CHECK_INTERVAL).
	float m_FallCheckAccum = 0.0;

	//! Collision oracle (test): throttle for the "slide off a round trunk" error log —
	//! one trunk should not spam a dozen errors over half a second.
	float m_TreeCollisionCooldown = 0.0;

	//! Proactive vision (ProbeAhead): climb/door candidates with a cooldown/timeout,
	//! and whether walkable ground is ahead (fall safety).
	bool m_ClimbCandidate = false;
	float m_ClimbCandidateUntil = 0.0;
	float m_VaultFallbackUntil = 0.0;
	bool m_DoorCandidate = false;
	float m_DoorCandidateUntil = 0.0;
	bool m_NoGroundAhead = false;

#ifdef DM_BOT_DEBUG_MOTION
	//! Motion stall oracle (событийный, DM_BOT_DEBUG_MOTION): якорь-позиция +
	//! накопленное время стояния на месте (смещение < DM_MOTION_STALL_EPS по
	//! горизонтали). При > DM_MOTION_STALL_TIME — событие [MOTION] stall.
	vector m_MotionStallAnchor = vector.Zero;
	bool m_MotionStallAnchorValid = false;
	float m_MotionStallTime = 0.0;
	float m_MotionStallCooldownUntil = 0.0;

	//! Motion fallfear oracle (событийный, DM_BOT_DEBUG_MOTION): накопитель
	//! удержания m_NoGroundAhead=true подряд; при > DM_MOTION_FALLFEAR_TIME —
	//! событие [MOTION] pathfail reason=fallfear.
	float m_MotionFallFearTime = 0.0;
#endif

	//! Tree/bush candidate: the trunk probe hit a tree/bush ahead; consumed by the
	//! veer (side-strafe) to walk around it instead of vaulting/climbing.
	bool m_TreeCandidate = false;
	float m_TreeCandidateUntil = 0.0;

	//! Veer in progress: short side-strafe around a tree (body keeps facing forward).
	bool m_Veering = false;
	float m_VeerTimer = 0.0;
	float m_VeerDir = 90.0;
	//! Veer pace (0..3) — captured in the trigger so the veer uses the bot's current
	//! movement speed, not a fixed one.
	float m_VeerSpeed = 2.0;
	//! Tree collision oracle grace: until this time (GetTickTime) the oracle ignores
	//! the lateral velocity component — the diagonal impulse of a just-finished veer
	//! decays here instead of being read as a trunk collision.
	float m_VeerGraceUntil = 0.0;

	//! Ladder climb/descend in progress: while the UseLadder intent (EXCLUSIVE)
	//! owns the body, MoveTo is dormant; once it finishes MoveTo re-routes (see
	//! OnUpdate).
	bool m_Laddering = false;
	ref dmBotIntent_UseLadder m_UseLadder;

	//! Leap-of-faith descent in progress: bot steps off a detached navmesh island
	//! (table/rooftop) in crouch and falls a safe distance, then re-routes on
	//! landing. Separate OnUpdate branch (like vault/ladder) so stuck detection
	//! never runs on the descent.
	bool m_Descending = false;
	vector m_DescendTarget;      // точка за краем на земле — куда ведём бота
	float m_DescendGroundY = 0.0; // уровень земли (Y) под точкой спуска — детект приземления
	float m_DescendTimer = 0.0;   // таймаут спуска (защита от бесконечного зависания)
	bool m_DescendWasFalling = false; // бот уже падал (для детекта «падал → приземлился»)
	ref dmBotIntent_Stance m_DescendStance; // присед на время спуска (STANCE, CRITICAL)

	void dmBotIntent_MoveTo()
	{
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	//! Разрешено ли логировать гол «нет пути» по ключу (тип сообщения + m_Goal).
	//! Лениво создаёт статик-кэш меток времени; каждый уникальный ключ логируется
	//! не чаще раза в DM_MOVE_NOPATH_LOG_INTERVAL секунд.
	static bool ShouldLogNoPath(string key)
	{
		float now = GetGame().GetTickTime();
		if (!s_NoPathLog)
			s_NoPathLog = new map<string, float>();
		float last;
		if (s_NoPathLog.Find(key, last) && now - last < DM_MOVE_NOPATH_LOG_INTERVAL)
			return false;
		s_NoPathLog.Set(key, now);
		return true;
	}

#ifdef DM_BOT_DEBUG_MOTION
	//! Разрешено ли логировать событие [MOTION] pathfail по ключу "reason:botName"
	//! (cooldown DM_MOTION_PATHFAIL_COOLDOWN). Статик-кэш переживает пересоздание
	//! интента — глушит спам одного события на одного бота.
	static bool ShouldLogMotionPathfail(string key)
	{
		float now = GetGame().GetTickTime();
		if (!s_MotionPathfailCooldown)
			s_MotionPathfailCooldown = new map<string, float>();
		float last;
		if (s_MotionPathfailCooldown.Find(key, last) && now - last < DM_MOTION_PATHFAIL_COOLDOWN)
			return false;
		s_MotionPathfailCooldown.Set(key, now);
		return true;
	}
#endif

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

	//! Доворачивать ли корпус (слайдом) к текущему sub-goal во время движения.
	//! FollowTo может переопределить.
	bool KeepBodyTurnAtGoal()
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

		m_SteppingBack = false;
		m_StepBackTimer = 0.0;
		m_StepBackDone = false;

		m_Vaulting = false;
		m_VaultGrace = 0.0;

		m_TreeCandidate = false;
		m_TreeCandidateUntil = 0.0;
		m_Veering = false;
		m_VeerTimer = 0.0;
		m_VeerDir = 90.0;
		m_VeerSpeed = 2.0;
		m_VeerGraceUntil = 0.0;
		m_TreeCollisionCooldown = 0.0;

		m_Laddering = false;
		m_UseLadder = null;

		m_Descending = false;
		m_DescendTarget = vector.Zero;
		m_DescendGroundY = 0.0;
		m_DescendTimer = 0.0;
		m_DescendWasFalling = false;
		m_DescendStance = null;

		m_Path = new array<vector>();
		m_HasPath = false;
		m_Route = new array<ref dmBotRouteSegment>();
		m_RouteIdx = 0;

		#ifdef DM_BOT_DEBUG_MOTION
		m_MotionStallAnchorValid = false;
		m_MotionStallTime = 0.0;
		m_MotionStallCooldownUntil = 0.0;
		m_MotionFallFearTime = 0.0;
		#endif

		m_LastPassedPoint = bot.GetPosition();

		if (!IsContinuous())
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] RePath #005");
			#endif
			RePath(bot);
			if (!m_HasPath && !m_Descending)
			{
				if (ShouldLogNoPath("nopath:" + m_Goal.ToString()))
					dmBotLog.Error(bot.GetName(), "MoveTo: нет пути к " + m_Goal + " (вне navmesh или недостижимо), abort");
				#ifdef DM_BOT_DEBUG_MOTION
				if (ShouldLogMotionPathfail("nopath:" + bot.GetName()))
					LogMotionPathFail(bot, "nopath", bot.GetPosition());
				#endif
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
		UpdateFallSafety(bot, pDt);

		if (IsFinished()) return;

		if (m_Veering) { TickVeer(bot, pDt); return; }
		float nowTree = GetGame().GetTickTime();
		if (m_TreeCandidate && nowTree < m_TreeCandidateUntil)
		{
			m_TreeCandidate = false;
			m_Veering = true;
			float veerSpeed = GetMoveSpeed(bot);
			m_VeerSpeed = veerSpeed;
			float veerTime = DM_TREE_VEER_TIME_WALK;
			if (veerSpeed >= 3.0)
				veerTime = DM_TREE_VEER_TIME_SPRINT;
			else if (veerSpeed >= 2.0)
				veerTime = DM_TREE_VEER_TIME_JOG;
			m_VeerTimer = veerTime;
			m_VeerDir = DM_TREE_VEER_DIR;
			if (Math.RandomFloat(0.0, 1.0) < 0.5)
				m_VeerDir = -DM_TREE_VEER_DIR;
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] Veer start dir=" + m_VeerDir + " speed=" + veerSpeed + " time=" + veerTime);
			#endif
			TickVeer(bot, pDt);
			return;
		}

		if (m_Recovering) { TickRecover(bot, pDt); return; }
		if (m_Detouring)  { TickDetour(bot, pDt);  return; }
		if (m_DoorRecovering) { TickDoorRecover(bot, pDt); return; }
		if (m_Vaulting)   { TickVault(bot, pDt);   return; }
		if (m_Laddering)  { TickLadder(bot, pDt);  return; }
		if (m_SteppingBack) { TickStepBack(bot, pDt); return; }
		if (m_Descending) { TickDescend(bot, pDt); return; }

		TickMove(bot, pDt);
	}

	//! Stuck recovery step-back/sideways: moves for DM_MOVE_RECOVER_TIME, then
	//! re-routes. Runs while m_Recovering is set (see ResolveStuck).
	void TickRecover(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] TickRecover m_RecoverDir=" + m_RecoverDir);
		#endif
		m_RecoverTimer -= pDt;
		bot.IntentMove(m_RecoverDir, 1.0);

		if (m_RecoverTimer > 0.0)
			return;

		m_Recovering = false;
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] RePath #003");
		#endif
		RePath(bot);
		if (!m_HasPath && !IsContinuous())
		{
			if (ShouldLogNoPath("recover:" + m_Goal.ToString()))
				dmBotLog.Error(bot.GetName(), "MoveTo: восстановление не помогло, путь к " + m_Goal + " недоступен, abort");
			#ifdef DM_BOT_DEBUG_MOTION
			if (ShouldLogMotionPathfail("nopath:" + bot.GetName()))
				LogMotionPathFail(bot, "nopath", bot.GetPosition());
			#endif
			Fail();
			return;
		}

		m_NoProgressTime = 0.0;
	}

	//! Door-press recovery: sprint forward for DM_DOOR_RECOVER_TIME, then re-route.
	//! Runs while m_DoorRecovering is set (see ResolveStuck/TryCloseDoorAhead).
	void TickDoorRecover(dmAISurvivor bot, float pDt)
	{
		m_DoorRecoverTimer -= pDt;
		bot.IntentMove(0.0, DM_DOOR_RECOVER_SPEED);

		if (m_DoorRecoverTimer > 0.0)
			return;

		m_DoorRecovering = false;
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] RePath #008");
		#endif
		RePath(bot);
		if (!m_HasPath && !IsContinuous())
		{
			if (ShouldLogNoPath("doorrecover:" + m_Goal.ToString()))
				dmBotLog.Error(bot.GetName(), "MoveTo: закрыл дверь, но путь к " + m_Goal + " недоступен, abort");
			#ifdef DM_BOT_DEBUG_MOTION
			if (ShouldLogMotionPathfail("nopath:" + bot.GetName()))
				LogMotionPathFail(bot, "nopath", bot.GetPosition());
			#endif
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
		dmBotLog.Debug(bot.GetName(), "[PATH] TickDetour m_DetourDir=" + m_DetourDir);
		#endif
		m_DetourTimer -= pDt;
		bot.IntentMove(m_DetourDir, 1.0);

		if (m_DetourTimer > 0.0)
			return;

		m_Detouring = false;
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] RePath #006");
		#endif
		RePath(bot);
		if (!m_HasPath && !IsContinuous())
		{
			if (ShouldLogNoPath("detour:" + m_Goal.ToString()))
				dmBotLog.Error(bot.GetName(), "MoveTo: detour не помог, путь к " + m_Goal + " недоступен, abort");
			#ifdef DM_BOT_DEBUG_MOTION
			if (ShouldLogMotionPathfail("nopath:" + bot.GetName()))
				LogMotionPathFail(bot, "nopath", bot.GetPosition());
			#endif
			Fail();
			return;
		}

		m_NoProgressTime = 0.0;
	}

	//! Короткий стрейф вбок (без доворота корпуса), чтобы обойти дерево. Без
	//! SetMoveYaw/LookAtPoint — корпус не крутится, только смещение вбок. m_NoProgressTime
	//! во время veer не обновляется (мы в отдельном бранче OnUpdate).
	void TickVeer(dmAISurvivor bot, float pDt)
	{
		m_VeerTimer -= pDt;
		bot.IntentMove(m_VeerDir, m_VeerSpeed);
		if (m_VeerTimer <= 0.0)
		{
			m_Veering = false;
			m_VeerGraceUntil = GetGame().GetTickTime() + DM_TREE_ORACLE_GRACE;
		}
	}

	//! Fall-safe: есть ли обрыв на 0.5 м впереди (по направлению к подцели).
	bool HasDropAhead(dmAISurvivor bot)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return false;

		vector pos = bot.GetPosition();

		vector moveDir = m_Goal - pos;
		moveDir[1] = 0.0;
		if (moveDir.Length() < 0.01)
			return false;
		moveDir.Normalize();

		return !GroundRay(pawn, pos + moveDir * DM_FALL_CHECK_AHEAD);
	}

	//! Начать отход назад на DM_STEP_BACK_TIME.
	void StartStepBack(dmAISurvivor bot)
	{
		m_SteppingBack = true;
		m_StepBackTimer = DM_STEP_BACK_TIME;
	}

	//! Пока отходим: движемся назад, по истечении — перестроить маршрут.
	void TickStepBack(dmAISurvivor bot, float pDt)
	{
		m_StepBackTimer -= pDt;
		bot.IntentMove(180.0, 1.0);

		if (m_StepBackTimer <= 0.0)
		{
			m_SteppingBack = false;
			RePath(bot);
		}
	}

	//! Vault/climb in progress: wait for the grace period, then stop vaulting once
	//! the climb command is no longer active (IsClimbing clears).
	void TickVault(dmAISurvivor bot, float pDt)
	{
		m_VaultGrace -= pDt;
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] TickVault m_VaultGrace=" + m_VaultGrace);
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
			dmBotLog.Debug(bot.GetName(), "[PATH] TickLadder m_Phase=" + m_UseLadder.m_Phase);
		#endif
		if (m_UseLadder && (m_UseLadder.IsFinished() || m_UseLadder.IsExpired()))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] RePath #002");
			#endif
			m_UseLadder = null;
			m_Laddering = false;
			m_NoProgressTime = 0.0;
			m_RouteIdx = m_RouteIdx + 2;   // перепрыгнуть лестничный сегмент
			if (m_Route && m_RouteIdx < m_Route.Count() && !m_Route[m_RouteIdx].m_IsLadder)
			{
				m_Path = m_Route[m_RouteIdx].m_Waypoints;
				m_PathIdx = 0;
				m_HasPath = true;
				RoundPath();
			}
			else
			{
				RePath(bot);
			}
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

#ifdef DM_BOT_DEBUG_MOTION
	//! Motion stall oracle (событийный): стояние на месте > DM_MOTION_STALL_TIME.
	//! Якорь сбрасывается при любом горизонтальном смещении >= DM_MOTION_STALL_EPS;
	//! при удержании в пределах EPS копит pDt и пишет [MOTION] stall (cooldown).
	void TickMotionStallOracle(dmAISurvivor bot, float pDt)
	{
		float now = GetGame().GetTickTime();
		vector pos = bot.GetPosition();

		if (!m_MotionStallAnchorValid)
		{
			m_MotionStallAnchor = pos;
			m_MotionStallAnchorValid = true;
			m_MotionStallTime = 0.0;
			return;
		}

		float dx = pos[0] - m_MotionStallAnchor[0];
		float dz = pos[2] - m_MotionStallAnchor[2];
		float epsSq = DM_MOTION_STALL_EPS * DM_MOTION_STALL_EPS;
		if (dx * dx + dz * dz >= epsSq)
		{
			m_MotionStallAnchor = pos;
			m_MotionStallTime = 0.0;
			return;
		}

		m_MotionStallTime = m_MotionStallTime + pDt;
		if (m_MotionStallTime > DM_MOTION_STALL_TIME && now > m_MotionStallCooldownUntil)
		{
			m_MotionStallCooldownUntil = now + DM_MOTION_STALL_COOLDOWN;
			float elapsed = m_MotionStallTime;
			m_MotionStallTime = 0.0;
			LogMotionStall(bot, pos, elapsed);
		}
	}

	//! Motion fallfear oracle (событийный): event-изация per-tick STOP по
	//! m_NoGroundAhead. Копит время удержания m_NoGroundAhead=true; при >
	//! DM_MOTION_FALLFEAR_TIME пишет [MOTION] pathfail reason=fallfear (cooldown).
	void TickMotionFallFearOracle(dmAISurvivor bot, float pDt)
	{
		if (!m_NoGroundAhead)
		{
			m_MotionFallFearTime = 0.0;
			return;
		}

		m_MotionFallFearTime = m_MotionFallFearTime + pDt;
		if (m_MotionFallFearTime > DM_MOTION_FALLFEAR_TIME)
		{
			m_MotionFallFearTime = 0.0;
			if (ShouldLogMotionPathfail("fallfear:" + bot.GetName()))
				LogMotionPathFail(bot, "fallfear", bot.GetPosition());
		}
	}

	//! Одна структурированная строка [MOTION] stall (для Python-парсера).
	void LogMotionStall(dmAISurvivor bot, vector pos, float elapsed)
	{
		string msg = "[MOTION] stall bot=" + bot.GetName() + " pos=" + pos;
		msg = msg + " goal=" + m_Goal;
		if (m_HasPath && m_Path && m_PathIdx < m_Path.Count())
			msg = msg + " wp=" + m_Path[m_PathIdx];
		else
			msg = msg + " wp=none";
		msg = msg + " elapsed=" + elapsed;
		msg = msg + " intent=" + GetIntentName();
		dmBotLog.Debug("", msg);
	}

	//! Одна структурированная строка [MOTION] pathfail (для Python-парсера).
	void LogMotionPathFail(dmAISurvivor bot, string reason, vector pos)
	{
		string msg = "[MOTION] pathfail bot=" + bot.GetName() + " pos=" + pos;
		msg = msg + " goal=" + m_Goal;
		msg = msg + " reason=" + reason;
		dmBotLog.Debug("", msg);
	}
#endif

	//! Normal steering: proactive door check, goal re-derive, steer toward the
	//! current waypoint, per-waypoint progress monitor and stuck resolution.
	void TickMove(dmAISurvivor bot, float pDt)
	{
		#ifdef DM_BOT_DEBUG_MOTION
		TickMotionStallOracle(bot, pDt);
		TickMotionFallFearOracle(bot, pDt);
		#endif

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

		//! Проактивное избегание костра: детектим горящий костёр у бота и у подцели,
		//! запоминаем в красную зону и сдвигаем подцель ВБОК от костра.
		m_DangerAccum += pDt;
		if (m_DangerAccum >= DM_DANGER_CHECK_INTERVAL)
		{
			m_DangerAccum = 0.0;
			FireplaceBase fire = dmRedZone.ScanFireplace(subGoal, DM_BOT_DANGER_AVOID_RADIUS * 2.0);
			if (!fire)
				fire = dmRedZone.ScanFireplace(pos, DM_BOT_DANGER_AVOID_RADIUS * 2.0);
			if (fire)
				dmRedZone.Add(fire.GetPosition(), DM_BOT_DANGER_AVOID_RADIUS, DM_BOT_DANGER_TIMEOUT);
		}

		//! Если подцель внутри зоны ИЛИ отрезок pos->subGoal пересекает зону — сдвинуть
		//! подцель ВБОК от ближайшего костра (перпендикулярно бот->костёр), а не «за» него.
		if (dmRedZone.IsPointInside(subGoal) || dmRedZone.IsSegmentCrossing(pos, subGoal))
		{
			vector avoidCenter;
			float avoidRadius;
			if (dmRedZone.FindNearest(pos, avoidCenter, avoidRadius))
			{
				vector toFire = avoidCenter - pos;
				toFire[1] = 0.0;
				if (toFire.Length() < 0.01)
					toFire = Vector(1.0, 0.0, 0.0);
				toFire.Normalize();

				vector side = Vector(-toFire[2], 0.0, toFire[0]);   // перпендикуляр +90
				vector toSub = subGoal - avoidCenter;
				toSub[1] = 0.0;
				float dot = side[0] * toSub[0] + side[2] * toSub[2];
				if (dot < 0.0)
					side = Vector(toFire[2], 0.0, -toFire[0]);      // -90 (ближе к исходной подцели)

				vector dangerSubGoal = subGoal;
				subGoal = avoidCenter + side * (avoidRadius + DM_BOT_DANGER_MARGIN);
				subGoal[1] = pos[1];

				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug(bot.GetName(), "[PATH] Danger shift " + dangerSubGoal + " -> " + subGoal + " fire=" + avoidCenter);
				#endif
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
			dmBotLog.Debug(bot.GetName(), "[PATH] TickMove STOP m_NoGroundAhead=" + m_NoGroundAhead);
			#endif
			return;
		}

		if ( m_Distance < 1.0 )
		{
			dmAISurvivorBase pawnChk = dmAISurvivorBase.Cast(bot.GetPawn());
			bool atHeight = false;
			if (pawnChk)
				atHeight = pawnChk.IsAtHeight();
			if (!atHeight)
			{
				m_TooCloseTime += pDt;
				if ( m_TooCloseTime > DM_MOVE_TOO_CLOSE_MAX )
					m_TooCloseTime = DM_MOVE_TOO_CLOSE_MAX;
			}
		} else {
			m_TooCloseTime = 0.0;
		}
		bool reached = IsWaypointReachedOnce(pos, subGoal, reach * (1.0 + m_TooCloseTime / 2.0));
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] Иду к точке: subGoal=" + subGoal + " pos=" + pos + " reached=" + reached);
		#endif

		//! Снисходительный переход к лестнице: бот упирается в перила входа (0.245 м
		//! при допуске 0.15) и не дотягивается до `reached`. Если следующий сегмент —
		//! лестница и бот уже у конца текущего навмеш-сегмента (последняя точка m_Path),
		//! спавним подъём/спуск по близости, не дожидаясь `reached`.
		if (m_Route && m_RouteIdx + 1 < m_Route.Count() && m_Route[m_RouteIdx + 1].m_IsLadder && m_HasPath && m_Path && m_PathIdx == m_Path.Count() - 1)
		{
			float ladderDist = vector.Distance(pos, m_Path[m_Path.Count() - 1]);
			if (ladderDist <= DM_LADDER_ATTACH_DIST)
			{
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug(bot.GetName(), "[PATH] Лестница по близости: dist=" + ladderDist + " -> UseLadder (не по reached)");
				#endif
				SpawnLadder(bot);
				return;
			}
		}

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

			//! Конец navmesh-сегмента: если дальше по маршруту лестница — спавним подъём.
			if (m_Route && m_RouteIdx + 1 < m_Route.Count() && m_Route[m_RouteIdx + 1].m_IsLadder)
			{
				SpawnLadder(bot);
				return;
			}

			//! Бот дошёл до конца пути, но всё ещё выше цели (оторванный navmesh-остров —
			//! стол/платформа с перепадом < DM_NAV_GAP): попробуй «прыжок веры» вниз.
			//! TryLeapOfFaith сам вернёт false для обычного достижения цели (цель не ниже),
			//! поэтому нормальный ground-reach не задет.
			if (TryLeapOfFaith(bot))
				return;

			OnReachedGoal(bot, subGoal);
			return;
		}

		float subYaw = dir.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float moveAngle = dmAISurvivor.AngleDiff(subYaw, bodyYaw);
		m_PathYaw = subYaw;

		#ifdef DM_BOT_DEBUG_LADDER
		//! Временный per-frame диагностический лог (баг «бесконечное кручение после
		//! лестницы»): почему вращается subYaw — бот реально крутится, dir≈0 (мусор
		//! VectorToAngles от нулевого вектора) или дрейфует якорь FollowTo.
		int pc = 0;
		if (m_Path)
			pc = m_Path.Count();
		dmBotLog.Debug(bot.GetName(), "[LADDER] MV: subGoal=" + subGoal + " pos=" + pos + " dir=" + dir + " dist=" + dist);
		dmBotLog.Debug(bot.GetName(), "[LADDER] MV: subYaw=" + subYaw + " bodyYaw=" + bodyYaw + " moveAngle=" + moveAngle);
		dmBotLog.Debug(bot.GetName(), "[LADDER] MV: goal=" + m_Goal + " hasPath=" + m_HasPath + " pathIdx=" + m_PathIdx + " pathCnt=" + pc + " reached=" + reached);
		dmAISurvivorBase ladderPawn = bot.GetPawn();
		vector ladderVel = vector.Zero;
		if (ladderPawn)
			ladderVel = GetVelocity(ladderPawn);
		dmBotLog.Debug(bot.GetName(), "[LADDER] MV: laddering=" + m_Laddering + " vel=" + ladderVel);
		#endif

		//! Оракул: командуем вперёд, а фактическая скорость имеет боковую составляющую —
		//! значит бот зацепил круглое дерево и его снесло по касательной.
		if (Math.AbsFloat(moveAngle) < DM_TREE_COLLISION_MOVE_ANGLE && GetGame().GetTickTime() > m_TreeCollisionCooldown && GetGame().GetTickTime() > m_VeerGraceUntil)
		{
			dmAISurvivorBase pawnV = dmAISurvivorBase.Cast(bot.GetPawn());
			if (pawnV)
			{
				vector vel = GetVelocity(pawnV);
				vel[1] = 0.0;
				vector dnorm = dir;
				dnorm[1] = 0.0;
				if (dnorm.Length() > 0.001)
				{
					dnorm.Normalize();
					float fwd = vel[0] * dnorm[0] + vel[2] * dnorm[2];
					if (fwd > DM_TREE_COLLISION_FORWARD)
					{
						float lat = Math.AbsFloat(vel[0] * dnorm[2] - vel[2] * dnorm[0]);
						if (lat > DM_TREE_COLLISION_LATERAL)
						{
							dmBotLog.Error(bot.GetName(), "Tree collision: pos=" + pos + " lat=" + lat);
							m_TreeCollisionCooldown = GetGame().GetTickTime() + 1.0;
						}
					}
				}
			}
		}

		//! Body faces the movement direction (comfort policy); the head looks at the
		//! waypoint. If a higher-priority look intent holds the body (FULL), moveAngle
		//! becomes the strafe/backpedal direction instead.
		bot.SetMoveYaw(subYaw);
		if (KeepBodyTurnAtGoal())
			bot.TurnBodyToward(subYaw, DM_MOVE_BODY_TURN_RATE, pDt);
		if (KeepLookAtGoal())
			bot.LookAtPoint(subGoal + Vector(0, DM_EYE_HEIGHT, 0), dmBotLookTurn.FULL);
		float speed = GetMoveSpeed(bot);
		bot.IntentMove(moveAngle, speed);

		#ifdef DM_BOT_DEBUG_FSM
		m_DebugAccum += pDt;
		if (m_DebugAccum >= 1.0)
		{
			m_DebugAccum = 0.0;
			int pathPoints = 0;
			if (m_Path)
				pathPoints = m_Path.Count();
			dmBotLog.Debug(bot.GetName(), "[FSM] MoveTo: subGoal=" + subGoal + " pos=" + pos + " dist=" + dist + " reach=" + reach + " pathIdx=" + m_PathIdx + " pathPoints=" + pathPoints);
			dmBotLog.Debug(bot.GetName(), "[FSM] MoveTo: moveAngle=" + moveAngle + " speed=" + speed + " deadline=" + m_ReachDeadline);
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

	//! Спавн подъёма/спуска по лестнице: создать dmBotIntent_UseLadder из
	//! следующего (лестничного) сегмента маршрута и пометить m_Laddering, пока
	//! EXCLUSIVE-интент владеет телом. Вызывается как при `reached` (конец
	//! навмеш-сегмента), так и при близости к последней точке сегмента.
	void SpawnLadder(dmAISurvivor bot)
	{
		dmBotRouteSegment ladderSeg = m_Route[m_RouteIdx + 1];
		m_UseLadder = new dmBotIntent_UseLadder();
		m_UseLadder.m_Building = ladderSeg.m_Building;
		m_UseLadder.m_Ladder = ladderSeg.m_Ladder;
		m_UseLadder.m_Direction = ladderSeg.m_Direction;
		bot.AddPersonalityIntent(m_UseLadder);
		m_Laddering = true;
	}

	//! Decision cascade when the bot is stuck: door first, then vault/climb, then
	//! a step-back recovery, and finally abort/re-path.
	void ResolveStuck(dmAISurvivor bot)
	{
		if (TryOpenDoorAhead(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] TryOpenDoorAhead pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryCloseDoorAhead(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] TryCloseDoorAhead pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryVaultOrClimb(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] TryVaultOrClimb pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryVaultFallback(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] TryVaultFallback pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryRecover(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] TryRecover pos=" + bot.GetPosition());
			#endif
			return;
		}
		if (TryDetour(bot))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] TryDetour pos=" + bot.GetPosition());
			#endif
			return;
		}
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] TryAbortOrRepath pos=" + bot.GetPosition());
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

	//! Close an OPEN door that pinned the bot and start a forward sprint recovery.
	bool TryCloseDoorAhead(dmAISurvivor bot)
	{
		if (!bot.TryCloseDoorOnPath())
			return false;

		m_DoorRecovering = true;
		m_DoorRecoverTimer = DM_DOOR_RECOVER_TIME;
		m_NoProgressTime = 0.0;
		return true;
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
		dmBotLog.Debug(bot.GetName(), "[PATH] Vault по флагу зрения t=" + m_NoProgressTime);
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
		dmBotLog.Debug(bot.GetName(), "[PATH] Fallback на Vault t=" + m_NoProgressTime);
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
			dmBotLog.Debug(bot.GetName(), "[PATH] Recovering я застрял, пытаюсь выбраться");
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
			dmBotLog.Debug(bot.GetName(), "[PATH] MoveTo: detour #" + m_DetourCount + " dir=" + m_DetourDir);
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
			dmBotLog.Debug(bot.GetName(), "[PATH] RePath #001 t=" + m_NoProgressTime);
			if ( bot.m_DebugPlayer )
				GetGame().ChatMP(bot.m_DebugPlayer, "Изменение пути t=" + m_NoProgressTime, "colorAction");
			#endif
			m_NoProgressTime = 0.0;
			RePath(bot);
			return true;
		}

		if (ShouldLogNoPath("stuck:" + m_Goal.ToString()))
			dmBotLog.Error(bot.GetName(), "MoveTo: застрял на пути к " + m_Goal + ", abort");
		#ifdef DM_BOT_DEBUG_MOTION
		if (ShouldLogMotionPathfail("nopath:" + bot.GetName()))
			LogMotionPathFail(bot, "nopath", bot.GetPosition());
		#endif
		Fail();
		return false;
	}

	//! Proactive vision throttle: probe ahead (climb/door rays) at a fixed interval.
	void TickVision(dmAISurvivor bot, float pDt)
	{
		m_MovingVisionDt += pDt;
		if (m_MovingVisionDt < DM_MOVE_VISION_INTERVAL)
			return;
		pDt = m_MovingVisionDt;
		m_MovingVisionDt = 0.0;
		ProbeAhead(bot, pDt);
	}

	//! Proactive look ahead: probe the point just ahead of the bot for a climbable
	//! obstacle (low upward ray) and a closed door (eye ray). Sets the
	//! m_ClimbCandidate/m_DoorCandidate flags. (Fall safety moved to UpdateFallSafety.)
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

		float now = GetGame().GetTickTime();

		//! Низкий луч (под углом вверх 0.3 -> 1.0): кандидат на карабканье.
		RaycastRVParams low = new RaycastRVParams(pos + Vector(0.0, 0.3, 0.0), probe + Vector(0.0, 1.0, 0.0), bot.GetPawn());
		low.flags = CollisionFlags.ALLOBJECTS;
		ref array<ref RaycastRVResult> lowHits = new array<ref RaycastRVResult>;
		if (DayZPhysics.RaycastRVProxy(low, lowHits) && lowHits.Count() > 0 && lowHits[0].obj)
		{
			//! Дерево/куст НЕ карабкабельно — не ставить climb-candidate.
			if (!lowHits[0].obj.IsTree() && !lowHits[0].obj.IsBush())
			{
				m_ClimbCandidate = true;
				m_ClimbCandidateUntil = now + DM_CLIMB_FLAG_COOLDOWN;
			}
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

		//! Луч ствола (низкий толстый, физическая геометрия): дерево/куст впереди —
		//! не карабкаемся, а обходим (veer). Луч длиннее probe — нужен запас дистанции.
		vector treeProbe = pos + dir * DM_TREE_LOOKAHEAD;
		RaycastRVParams tree = new RaycastRVParams(pos + Vector(0.0, DM_TREE_RAY_HEIGHT, 0.0), treeProbe + Vector(0.0, DM_TREE_RAY_HEIGHT, 0.0), bot.GetPawn(), DM_TREE_RAY_RADIUS);
		tree.type = ObjIntersectGeom;
		tree.flags = CollisionFlags.ALLOBJECTS;
		ref array<ref RaycastRVResult> treeHits = new array<ref RaycastRVResult>;
		if (DayZPhysics.RaycastRVProxy(tree, treeHits) && treeHits.Count() > 0)
		{
			Object to = treeHits[0].obj;
			if (to && (to.IsTree() || to.IsBush()))
			{
				m_TreeCandidate = true;
				m_TreeCandidateUntil = now + DM_TREE_FLAG_TIMEOUT;
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug(bot.GetName(), "[PATH] Tree candidate ahead type=" + to.GetType());
				#endif
			}
		}
	}

	//! Fall-safety (троттлинг DM_FALL_CHECK_INTERVAL): физическая проверка GroundRay
	//! в точке ВПЕРЕДИ ПОЗИЦИИ БОТА (pos + moveDir*DM_FALL_CHECK_AHEAD) — есть ли
	//! поверхность в пределах DM_FALL_DANGER_DROP. Луч бьёт в точку, где бот физически
	//! стоит (а не в вейпоинт пути), поэтому висячий навмеш-only дек (крыша Land_House_1M2)
	//! ложного «обрыва» не даёт, а настоящий физический обрыв (>2.75 м) ловится.
	void UpdateFallSafety(dmAISurvivor bot, float pDt)
	{
		//! Обход fall-safety на спуске: m_Descending намеренно ведёт бота за край —
		//! m_NoGroundAhead не должен накапливаться и стопить бота на спуске.
		if (m_Descending)
		{
			m_NoGroundAhead = false;
			return;
		}

		m_FallCheckAccum += pDt;
		if (m_FallCheckAccum < DM_FALL_CHECK_INTERVAL)
			return;
		m_FallCheckAccum = 0.0;

		vector pos = bot.GetPosition();
		vector subGoal = m_Goal;
		if (m_HasPath && m_Path.Count() > 0)
			subGoal = m_Path[m_PathIdx];

		vector moveDir = subGoal - pos;
		moveDir[1] = 0.0;
		if (moveDir.Length() < 0.01)
		{
			m_NoGroundAhead = false;
			return;
		}
		moveDir.Normalize();

		vector ahead = pos + moveDir * DM_FALL_CHECK_AHEAD;

		bool physGround = false;
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
			physGround = GroundRay(pawn, ahead);
		m_NoGroundAhead = !physGround;

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] FallSafety ahead=" + ahead + " ground=" + physGround);
		#endif
	}

	//! Луч вниз из точки: есть ли поверхность в пределах DM_FALL_DANGER_DROP.
	bool GroundRay(dmAISurvivorBase pawn, vector p)
	{
		vector beg = p + Vector(0.0, 1.0, 0.0);
		vector end = p + Vector(0.0, -DM_FALL_DANGER_DROP, 0.0);
		vector contactPos;
		vector contactDir;
		int contactComponent;
		return DayZPhysics.RaycastRV(beg, end, contactPos, contactDir, contactComponent, null, null, pawn, false, false, ObjIntersectGeom);
	}

	//! Re-aim the navmesh path at m_Goal. On failure m_HasPath becomes false and
	//! m_Path null — the steering then moves directly toward m_Goal.
	void RePath(dmAISurvivor bot)
	{
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] RePath invoked goal=" + m_Goal + " goalY=" + m_Goal[1]);
		DebugElevatedGoal(bot);
		#endif
		ref array<ref dmBotRouteSegment> route = new array<ref dmBotRouteSegment>();
		if (bot.FindRouteTo(m_Goal, route) && route.Count() > 0)
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] FindRouteTo выполнено успешно, сегментов " + route.Count());
			int si;
			for (si = 0; si < route.Count(); si++)
			{
				if (route[si].m_IsLadder)
					dmBotLog.Debug(bot.GetName(), "[PATH]   seg[" + si + "] LADDER dir=" + route[si].m_Direction);
				else
					dmBotLog.Debug(bot.GetName(), "[PATH]   seg[" + si + "] navmesh n=" + route[si].m_Waypoints.Count());
			}
			#endif
			m_Route = route;
			m_RouteIdx = 0;
			m_Path = m_Route[0].m_Waypoints;
			m_PathIdx = 0;
			m_HasPath = true;
			RoundPath();
			return;
		}

		//! No navmesh path: allow a deliberate "leap of faith" drop only if the
		//! goal is below and the fall is safe. Builds a real descent (reverse-path)
		//! — m_Descending is set (лог «спуск начат» внутри StartDescend), so the
		//! bot steps off the edge instead of aborting.
		if (TryLeapOfFaith(bot))
		{
			return;
		}

		//! Fall-safe: перед ботом обрыв и маршрута нет — один раз отойти назад и перестроить.
		if (HasDropAhead(bot) && !m_StepBackDone)
		{
			m_StepBackDone = true;
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] RePath: drop ahead — step back");
			#endif
			StartStepBack(bot);
			return;
		}

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] RePath: NO PATH goal=" + m_Goal + " unreachable");
		#endif
		m_HasPath = false;
		m_Path = null;
	}

#ifdef DM_BOT_DEBUG_PATHFINDER
	//! Отладочная диагностика: почему цель на этаже выше идёт внутрь, а не по лестнице.
	void DebugElevatedGoal(dmAISurvivor bot)
	{
		vector botPos = bot.GetPosition();
		float dY = Math.AbsFloat(m_Goal[1] - botPos[1]);
		if (dY <= DM_LADDER_FLOOR_GAP)
			return;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		string botInside = "n/a";
		if (pawn)
			botInside = pawn.IsSoundInsideBuilding().ToString();

		dmBotLog.Debug(bot.GetName(), "[PATH] ElevatedGoal dY=" + dY + " goal=" + m_Goal + " botInside=" + botInside);

		RoofProbe(bot, m_Goal, "goal");
		RoofProbe(bot, botPos, "bot");
		DebugPathProbes(bot);
	}

	//! Raycast вверх на 25 м: ловит «под крышей здания» (hit = House/Building) vs «открытое небо».
	void RoofProbe(dmAISurvivor bot, vector pt, string label)
	{
		vector from = pt + Vector(0.0, 0.5, 0.0);
		vector to = pt + Vector(0.0, 25.0, 0.0);
		RaycastRVParams rp = new RaycastRVParams(from, to, bot.GetPawn());
		rp.flags = CollisionFlags.ALLOBJECTS;
		ref array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		string hitType = "none";
		if (DayZPhysics.RaycastRVProxy(rp, hits) && hits.Count() > 0 && hits[0].obj)
			hitType = hits[0].obj.GetType();
		dmBotLog.Debug(bot.GetName(), "[PATH] RoofProbe " + label + " pt=" + pt + " hit=" + hitType);
	}

	//! Комплексная отладочная диагностика «цель выше, чем достижимо» (крыша/этаж).
	//! Вся отладка изолирована здесь: чтобы выпилить — удали ОДИН вызов DebugPathProbes(bot).
	void DebugPathProbes(dmAISurvivor bot)
	{
		vector botPos = bot.GetPosition();

		// Лениво строит m_PathFilter (как IsPointOnNavMesh) и логирует HIT/NOHIT по цели.
		IsPointOnNavMesh(m_Goal);

		// 1) Точный сэмпл: есть ли navmesh в 0.5 м от цели (платформа игрока).
		vector exact;
		bool exactNav = g_Game.GetWorld().GetAIWorld().SampleNavmeshPosition(m_Goal, 0.5, m_PathFilter, exact);
		if (exactNav)
			dmBotLog.Debug(bot.GetName(), "[PATH] ProbePlatform navmesh@goal sampled=" + exact + " dY=" + Math.AbsFloat(m_Goal[1] - exact[1]));
		else
			dmBotLog.Debug(bot.GetName(), "[PATH] ProbePlatform NO navmesh within 0.5m");

		// 2) Обратный путь: может ли «крыша» дотянуться до земли?
		ref array<vector> rev = new array<vector>();
		if (g_Game.GetWorld().GetAIWorld().FindPath(m_Goal, botPos, m_PathFilter, rev) && rev.Count() > 0)
			dmBotLog.Debug(bot.GetName(), "[PATH] ProbeReverse found n=" + rev.Count() + " last=" + rev[rev.Count() - 1]);
		else
			dmBotLog.Debug(bot.GetName(), "[PATH] ProbeReverse NO PATH");

		// 3) Серия высот: FindPath(bot -> цель + k метров вверх).
		ProbeHeight(bot, 0.0);
		ProbeHeight(bot, 0.5);
		ProbeHeight(bot, 1.0);
		ProbeHeight(bot, 2.0);
		ProbeHeight(bot, 3.0);

		// 4) Лестница: путь до нижней/верхней точки входа.
		ProbeLadders(bot, botPos);
	}

	//! FindPath(bot -> m_Goal + up) с маркером высоты.
	void ProbeHeight(dmAISurvivor bot, float up)
	{
		vector probeGoal = m_Goal + Vector(0.0, up, 0.0);
		ref array<vector> p = new array<vector>();
		if (bot.FindPathTo(probeGoal, p) && p.Count() > 0)
			dmBotLog.Debug(bot.GetName(), "[PATH] ProbeHeight +" + up + " found n=" + p.Count() + " last=" + p[p.Count() - 1]);
		else
			dmBotLog.Debug(bot.GetName(), "[PATH] ProbeHeight +" + up + " NO PATH");
	}

	//! Путь до нижней/верхней точек входа лестницы здания под ботом/перед ботом.
	void ProbeLadders(dmAISurvivor bot, vector botPos)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return;

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
		{
			dmBotLog.Debug(bot.GetName(), "[PATH] ProbeLadders no ladder building");
			return;
		}

		int i;
		for (i = 0; i < ladders.Count(); i++)
		{
			dmBotLadder ladder = ladders[i];
			vector bottomWorld = building.ModelToWorld(ladder.m_Bottom);
			vector topWorld = building.ModelToWorld(ladder.m_Top);
			ref array<vector> pB = new array<vector>();
			bool bOk = bot.FindPathTo(bottomWorld, pB);
			if (bOk && pB.Count() > 0)
				dmBotLog.Debug(bot.GetName(), "[PATH] ProbeLadder i=" + ladder.m_Index + " bottom ok last=" + pB[pB.Count() - 1]);
			else
				dmBotLog.Debug(bot.GetName(), "[PATH] ProbeLadder i=" + ladder.m_Index + " bottom NO PATH");
			ref array<vector> pT = new array<vector>();
			bool tOk = bot.FindPathTo(topWorld, pT);
			if (tOk && pT.Count() > 0)
				dmBotLog.Debug(bot.GetName(), "[PATH] ProbeLadder i=" + ladder.m_Index + " top ok last=" + pT[pT.Count() - 1]);
			else
				dmBotLog.Debug(bot.GetName(), "[PATH] ProbeLadder i=" + ladder.m_Index + " top NO PATH");
		}
	}
#endif

	//! Post-process a freshly computed path: round sharp corners (>= DM_PATH_ROUND_ANGLE_LOW)
	//! by inserting intermediate waypoints (overshoot + 90° stair-steps) so the bot
	//! doesn't jam into fence corners or vault them. Only the first
	//! DM_PATH_ROUND_LOOKAHEAD meters are rounded; the rest is copied as-is.
	void RoundPath()
	{
		if (!m_Path || m_Path.Count() < 3)
			return;

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("", "[PATH] RoundPath: count=" + m_Path.Count());
		#endif

		ref array<vector> rounded = new array<vector>();
		vector prev = m_Path[0];
		rounded.Insert(prev);

		float accum = 0.0;
		int i;
		int count = m_Path.Count();
		for (i = 1; i < count - 1; i++)
		{
			vector wp = m_Path[i];
			vector nextWp = m_Path[i + 1];

			vector seg = wp - prev;
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

			vector dirIn = wp - prev;
			dirIn[1] = 0.0;
			vector dirOut = nextWp - wp;
			dirOut[1] = 0.0;
			if (dirIn.Length() < 0.001 || dirOut.Length() < 0.001)
			{
				rounded.Insert(wp);
				prev = wp;
				continue;
			}
			dirIn.Normalize();
			dirOut.Normalize();

			float yawIn = dirIn.VectorToAngles()[0];
			float yawOut = dirOut.VectorToAngles()[0];
			float turn = Math.AbsFloat(dmAISurvivor.AngleDiff(yawOut, yawIn));

			if (turn < DM_PATH_ROUND_ANGLE_LOW)
			{
				rounded.Insert(wp);
				prev = wp;
				continue;
			}

			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("", "[PATH] RoundPath corner=" + wp + " turn=" + turn);
			#endif

			vector extPoint = wp + dirIn * DM_PATH_ROUND_STEP;
			if (turn <= DM_PATH_ROUND_ANGLE_HIGH)
			{
				InsertRounded(rounded, extPoint, wp);
				#ifdef DM_BOT_DEBUG_PATHFINDER
				dmBotLog.Debug("", "[PATH] RoundPath insert ext=" + extPoint + " (from " + wp + ")");
				#endif
				prev = rounded[rounded.Count() - 1];
				continue;
			}

			vector dir90 = Rotate90Toward(dirIn, dirOut);
			vector sidePoint = extPoint + dir90 * DM_PATH_ROUND_STEP;
			InsertRounded(rounded, extPoint, wp);
			InsertRounded(rounded, sidePoint, wp);
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("", "[PATH] RoundPath insert ext=" + extPoint + " side=" + sidePoint + " (from " + wp + ")");
			#endif
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
	//! down. First checks the drop is safe (goal below, fall height < LOW, not a
	//! rooftop), then builds a real descent via a reverse FindPath(goal -> bot):
	//! the end of the reversed path is the nearest reachable ground point past the
	//! edge. On success m_Descending is set (and m_HasPath stays true) so the bot
	//! walks off the edge in crouch instead of aborting.
	bool TryLeapOfFaith(dmAISurvivor bot)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("PathFinder.LeapOfFaith");
		#endif

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return false;

		vector botPos = pawn.GetPosition();

		//! Не прыгать, если бот высоко над землёй (на крыше) — спуск только по лестнице.
		float groundY = GetGame().SurfaceY(botPos[0], botPos[2]);
		if (botPos[1] - groundY > DM_FALL_JUMP_BLOCK_HEIGHT)
			return false;

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
		if (fallHeight >= DM_BOT_FALL_HEIGHT_LOW)
			return false;

		//! Обратный путь: ближайшая к боту точка земли, достижимая navmesh (конец
		//! обратного пути tempEnd). От неё бот «сходит с края». FindPath сам снипает
		//! концы к навмеш и при оторванном острове возвращает частичный путь до
		//! ближайшей достижимой точки — поэтому полагаемся только на Count() > 0.
		ref dmBotPathfinder pf = new dmBotPathfinder();
		ref array<vector> tempPath = new array<vector>();
		pf.FindPath(m_Goal, botPos, tempPath);
		if (tempPath.Count() == 0)
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] LeapOfFaith: обратный путь пуст, спуска нет");
			#endif
			return false;
		}

		vector tempEnd = tempPath[tempPath.Count() - 1];

		//! tempEnd должна быть рядом с ботом по горизонтали (иначе «остров» далёк
		//! от земли) и НИЖЕ бота (это земля под краем, а не та же высота).
		vector toEnd = tempEnd - botPos;
		toEnd[1] = 0.0;
		float horizDist = toEnd.Length();
		if (horizDist > DM_DESCEND_LEAP_RADIUS)
			return false;
		if (tempEnd[1] >= botPos[1] - 0.1)
			return false;

		StartDescend(bot, tempEnd, toEnd, horizDist);
		return true;
	}

	//! Начать спуск: вычислить точку за краем, держать присед (sub-intent Stance)
	//! и пометить m_Descending. m_HasPath/m_Path не трогаем: при запуске из OnStart
	//! они уже false/пустой не-null массив (OnStart не Fail по `!m_Descending`), при
	//! запуске из TickMove они содержат валидный путь — во время спуска не читаются
	//! (ветка TickDescend до TickMove).
	void StartDescend(dmAISurvivor bot, vector tempEnd, vector toEnd, float horizDist)
	{
		//! Направление спуска — к tempEnd; если tempEnd горизонтально под ботом
		//! (случай «под центром острова»), берём направление к цели.
		vector descendDir = toEnd;
		if (horizDist < 0.3)
		{
			vector botPos = bot.GetPosition();
			descendDir = m_Goal - botPos;
			descendDir[1] = 0.0;
			if (descendDir.Length() < 0.01)
				descendDir = Vector(1.0, 0.0, 0.0);
		}
		descendDir.Normalize();

		//! Точка чуть дальше края (на земле) — ведём бота туда, чтобы он сошёл.
		m_DescendTarget = tempEnd + descendDir * DM_DESCEND_EDGE_OVERSHOOT;
		m_DescendTarget[1] = tempEnd[1];

		m_DescendGroundY = tempEnd[1];
		m_DescendTimer = DM_DESCEND_TIMEOUT;
		m_DescendWasFalling = false;
		m_Descending = true;

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug(bot.GetName(), "[PATH] LeapOfFaith: спуск начат target=" + m_DescendTarget + " groundY=" + m_DescendGroundY);
		#endif

		//! Присед на время спуска. MoveTo — канал MOVE, а стойку арбитраж сбрасывает
		//! в ERECT каждый тик, поэтому держим присед отдельным STANCE-интентом
		//! (CRITICAL) до приземления/очистки.
		m_DescendStance = new dmBotIntent_Stance();
		m_DescendStance.m_Stance = DayZPlayerConstants.STANCEIDX_CROUCH;
		m_DescendStance.m_Priority = dmBotIntentPriority.CRITICAL;
		m_DescendStance.m_Deadline = -1.0;
		bot.AddFSMIntent(m_DescendStance);
	}

	//! Leap-of-faith descent: ведём бота к точке за краем, присед держится
	//! stance-интентом, fall-safety обойдён (UpdateFallSafety не трогает
	//! m_NoGroundAhead). Детектим приземление (Y ≈ земля или «падал → не падаю»)
	//! и перестраиваем маршрут. Stuck-детект сюда не заходит (отдельная ветка OnUpdate).
	void TickDescend(dmAISurvivor bot, float pDt)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			EndDescend();
			Fail();
			return;
		}

		vector pos = bot.GetPosition();

		if (pawn.IsFalling())
			m_DescendWasFalling = true;

		bool landed = pos[1] <= m_DescendGroundY + DM_DESCEND_LAND_EPS;
		if (m_DescendWasFalling && !pawn.IsFalling())
			landed = true;

		if (landed)
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] LeapOfFaith: приземлился y=" + pos[1] + " ground=" + m_DescendGroundY);
			#endif
			EndDescend();
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] RePath #007");
			#endif
			RePath(bot);
			return;
		}

		m_DescendTimer -= pDt;
		if (m_DescendTimer <= 0.0)
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug(bot.GetName(), "[PATH] LeapOfFaith: timeout, не приземлился y=" + pos[1] + " ground=" + m_DescendGroundY);
			#endif
			EndDescend();
			Fail();
			return;
		}

		vector dir = m_DescendTarget - pos;
		dir[1] = 0.0;
		float dist = dir.Length();
		if (dist < 0.01)
		{
			//! Уже у точки спуска — командуем вперёд, пока бот не сойдёт с края.
			bot.IntentMove(0.0, 1.0);
			return;
		}

		float subYaw = dir.VectorToAngles()[0];
		float bodyYaw = bot.GetOrientation()[0];
		float moveAngle = dmAISurvivor.AngleDiff(subYaw, bodyYaw);
		bot.SetMoveYaw(subYaw);
		if (KeepBodyTurnAtGoal())
			bot.TurnBodyToward(subYaw, DM_MOVE_BODY_TURN_RATE, pDt);
		bot.IntentMove(moveAngle, 1.0);
	}

	//! Снять присед-интент и сбросить флаги спуска (приземление / abort / OnCancel).
	void EndDescend()
	{
		m_Descending = false;
		if (m_DescendStance)
		{
			m_DescendStance.Finish();
			m_DescendStance = null;
		}
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

	ref PGFilter m_PathFilter;

	//! Ленивая сборка m_PathFilter (общий фильтр ходьбы): создаёт PGFilter один раз
	//! и настраивает флаги/стоимости. Используется IsPointOnNavMesh.
	void EnsurePathFilter()
	{
		if (m_PathFilter)
			return;

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

    bool IsPointOnNavMesh(vector point)
    {
		EnsurePathFilter();

		//! Ground probe: find the closest navmesh point within a small radius.
		//! RaycastNavMesh detects navmesh EDGES (polygon boundaries), NOT the flat
		//! surface, so a vertical ray NOHITs on flat ground. SampleNavmeshPosition
		//! returns the closest point on the navmesh — the correct "is there a
		//! walkable surface here" check.
		vector sampled;
		if (!g_Game.GetWorld().GetAIWorld().SampleNavmeshPosition(point, DM_MOVE_GROUND_PROBE_RADIUS, m_PathFilter, sampled))
		{
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("", "[PATH] SampleNavmeshPosition NOHIT point=" + point);
			#endif
			return false;
		}
		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("", "[PATH] SampleNavmeshPosition HIT sampled=" + sampled + " point=" + point + " diff=" + (Math.AbsFloat(point[1] - sampled[1])));
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
				dmBotLog.Debug(bot.GetName(), "[PATH] HasObstaclesToPoint HIT Pos=" + result.pos + " hitNormal=" + result.obj.GetType() + " comp=" + result.component);
				#endif
            }
        }
        
        return false;
    }

	vector m_LastPassedPoint;

	bool IsWaypointReachedOnce(vector pos, vector waypoint, float reachDistance)
	{
		vector lastPos = m_LastPassedPoint;
		vector curPos = pos;

		// Обнуление высоты нужно, потому что высота Path и точка на которой стоит бот никогда не сходятся!
		// Но для того, чтобы не было ошибки на разных этажах, заранее сравним высоту цели и высоту позиции.
		// if ( Math.AbsFloat( curPos[1] - waypoint[1] ) > 1.8 )
		// {
		// 	#ifdef DM_BOT_DEBUG_PATHFINDER
		// 	dmBotLog.Debug("", "[PATH] Разность высоты dH=" + Math.AbsFloat( curPos[1] - waypoint[1] ));
		// 	#endif
		// 	m_LastPassedPoint = pos;
		// 	return false;
		// } - Все бы ничего, но иногда путь строится по воздуху. Нужно каждый вейпоинт проверять по навмеш
		lastPos[1] = 0;
		curPos[1] = 0;
		waypoint[1] = 0;

		m_LastPassedPoint = pos;

		// Направляющий вектор отрезка: d = curPos - lastPos
		float dx = curPos[0] - lastPos[0];
		float dy = curPos[1] - lastPos[1];
		float dz = curPos[2] - lastPos[2];

		// Квадрат длины направляющего вектора
		float dd = dx * dx + dy * dy + dz * dz;

		// Вектор от lastPos к waypoint: v = waypoint - lastPos
		float vx = waypoint[0] - lastPos[0];
		float vy = waypoint[1] - lastPos[1];
		float vz = waypoint[2] - lastPos[2];

		// Вырожденный случай: lastPos и curPos совпадают
		if (dd < 0.0001 ) {
			float distSq = vector.DistanceSq(curPos, waypoint);
			#ifdef DM_BOT_DEBUG_PATHFINDER
			dmBotLog.Debug("", "[PATH] Вырожденный случай: lastPos и curPos совпадают distSq=" + distSq + " reachDistanceSq=" + (reachDistance * reachDistance));
			#endif
			return distSq <= reachDistance * reachDistance;
		}

		// Параметр проекции proj = (v · d) / (d · d)
		float proj = (vx * dx + vy * dy + vz * dz) / dd;

		// Если проекция не попадает на отрезок — точка не на отрезке
		if (proj < 0.0 || proj > 1.0) return false;

		// Перпендикулярное расстояние: |d × v| / |d|
		float cross_x = dy * vz - dz * vy;
		float cross_y = dz * vx - dx * vz;
		float cross_z = dx * vy - dy * vx;

		float cross_len = Math.Sqrt(cross_x * cross_x + cross_y * cross_y + cross_z * cross_z);
		float seg_len = Math.Sqrt(dd);
		float dist = cross_len / seg_len;

		#ifdef DM_BOT_DEBUG_PATHFINDER
		dmBotLog.Debug("", "[PATH] IsWaypointReached dist=" + dist + "[" + (dist <= reachDistance) + "]");
		#endif
		return dist <= reachDistance;
	}

	override void OnCancel(dmAISurvivor bot)
	{
		//! Очистка спуска обязательна: снять присед-интент и сбросить флаги, чтобы
		//! бот не остался в приседе/с обходом fall-safety после Fail/отмены.
		EndDescend();
		super.OnCancel(bot);
	}
}
