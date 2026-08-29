//! dmAISurvivor — server-side bot controller (Layer 0 + head look).
//!
//! The "brain" object is separate from the visual model (pawn). The pawn is a
//! dmAISurvivorBase entity (client-server mod) created via GetGame().CreateObject
//! (CE spawn, so it has an EconomyProfile and the vanilla corpse decay works).
//!
//! Head look: the controller computes a look offset (relative to the body) and
//! pushes it to the pawn, whose HeadingModel override turns the head while the
//! body stays still.

class dmAISurvivor
{
	//! Global list of all spawned bots (keeps controllers alive and enumerable).
	static ref array<ref dmAISurvivor> s_All = new array<ref dmAISurvivor>;

	//! Reverse lookup: pawn -> controller.
	static ref map<PlayerBase, ref dmAISurvivor> s_ByPawn = new map<PlayerBase, ref dmAISurvivor>;

	//! Fixed brain tick interval (seconds); see DM_BOT_TICK_INTERVAL.
	static float s_TickInterval = DM_BOT_TICK_INTERVAL;

	//! Accumulated time toward the next brain tick (driven by TickAll).
	static float s_TickAccum = 0.0;

	//! Survivor model class (e.g. "dmAI_SurvivorM_Denis").
	private string m_Model = DM_DEFAULT_MODEL;

	//! The pawn (visual/physical body) in the world. null until Spawn().
	private PlayerBase m_Pawn;

	//! Desired look direction.
	//! m_TargetLookYawAbs - horizontal look target in WORLD space (degrees).
	//! m_TargetLookPitch  - vertical angle (degrees).
	private float m_TargetLookYawAbs = 0.0;
	private float m_TargetLookPitch = 0.0;

	//! Current (smoothed) horizontal head offset relative to the body, degrees.
	private float m_CurLookYaw = 0.0;

	//! Behaviour state machine (null until a preset is loaded).
	private ref dmBotFSM m_FSM;

	//! Pathfinder (navmesh wrapper), lazily created on first use.
	private ref dmBotPathfinder m_Pathfinder;

	//! Look turn mode for UpdateLook (set by the last LookAt* call).
	private dmBotLookTurn m_LookTurnMode = dmBotLookTurn.AUTO;

	//! True while walking (movement controls body facing).
	private bool m_IsMoving = false;

	//! Desired movement direction (world yaw, degrees). Set by movement intents;
	//! ComputeBodyYaw uses it to face the movement direction when the body isn't
	//! held by a priority look intent.
	private float m_MoveYaw = 0.0;

	//! Preferred movement speed (1=walk, 2=jog, 3=sprint). Base for CalcSpeed.
	private float m_PreferredSpeed = 2.0;

	//! Intent pools: FSM (automatic), personality (interrupts), command (orders).
	private ref dmBotIntentPool m_FSMIntents;
	private ref dmBotIntentPool m_PersonalityIntents;
	private ref dmBotIntentPool m_CommandIntents;

	//! Patrol points (world positions visited in order).
	private ref array<vector> m_PatrolPoints;

	//! Goal targets (memory, survives FSM transitions).
	private ref array<ref dmTarget> m_Targets;

	//! Entity the bot escorts (follows alongside). null when not escorting.
	private EntityAI m_FollowTarget;

	void dmAISurvivor()
	{
		m_FSMIntents = new dmBotIntentPool();
		m_PersonalityIntents = new dmBotIntentPool();
		m_CommandIntents = new dmBotIntentPool();
		m_PatrolPoints = new array<vector>();
		m_Targets = new array<ref dmTarget>();
	}

	//! Model class to use. Must be set before Spawn().
	void SetModel(string model)
	{
		m_Model = model;
	}

	string GetModel()
	{
		return m_Model;
	}

	//! Materialize the bot in the world.
	//! @param position    body position in world space.
	//! @param orientation body orientation (Euler angles in degrees, yaw = [0]).
	//! @return the pawn, or null on failure.
	PlayerBase Spawn(vector position, vector orientation)
	{
		#ifdef DM_BOT_DEBUG_SPAWN
		dmBotLog.Debug("Spawn() model=" + m_Model + " position=" + position + " orientation=" + orientation);
		#endif

		if (m_Model.Length() == 0)
		{
			#ifdef DM_BOT_DEBUG_SPAWN
			dmBotLog.Debug("Spawn() FAILED: model is empty");
			#endif
			return null;
		}

		//! CE spawn (like Expansion AI): gives the pawn a Central Economy profile,
		//! so the vanilla corpse decay (lifetime/TTL) works. CreatePlayer(null, ...)
		//! would leave the pawn without a profile.
		Object entity = GetGame().CreateObject(m_Model, position);
		#ifdef DM_BOT_DEBUG_SPAWN
		dmBotLog.Debug("Spawn() CreateObject returned entity=" + entity);
		#endif

		PlayerBase pawn = PlayerBase.Cast(entity);
		if (!pawn)
		{
			#ifdef DM_BOT_DEBUG_SPAWN
			dmBotLog.Debug("Spawn() FAILED: PlayerBase.Cast(entity) returned null");
			#endif
			return null;
		}

		#ifdef DM_BOT_DEBUG_BODY
		dmBotLog.Debug("Spawn() hasCEProfile=" + (pawn.GetEconomyProfile() != null) + " hasIdentity=" + (pawn.GetIdentity() != null));
		#endif

		m_Pawn = pawn;
		m_Pawn.SetPosition(position);
		m_Pawn.SetOrientation(orientation);

		s_All.Insert(this);
		s_ByPawn.Set(m_Pawn, this);

		#ifdef DM_BOT_DEBUG_SPAWN
		dmBotLog.Debug("Spawn() OK pawn=" + pawn + " position=" + m_Pawn.GetPosition() + " orientation=" + m_Pawn.GetOrientation() + " total=" + s_All.Count());
		#endif
		return m_Pawn;
	}

	//! Remove the bot's body from the world and unregister it (manual cleanup,
	//! e.g. /test cancel). For a natural death use OnDeath() — it leaves the corpse.
	void Despawn()
	{
		#ifdef DM_BOT_DEBUG_SPAWN
		dmBotLog.Debug("Despawn() pawn=" + m_Pawn);
		#endif

		if (!m_Pawn)
			return;

		s_All.RemoveItem(this);
		s_ByPawn.Remove(m_Pawn);
		GetGame().ObjectDelete(m_Pawn);
		m_Pawn = null;
	}

	//! Death: release the brain (stop its heartbeat, drop references) but leave the
	//! corpse in the world — the engine owns its decay/TTL/removal from here.
	void OnDeath()
	{
		#ifdef DM_BOT_DEBUG_SPAWN
		dmBotLog.Debug("OnDeath() pawn=" + m_Pawn + " — мозг снимается, труп остаётся");
		#endif

		if (m_Pawn)
			s_ByPawn.Remove(m_Pawn);

		s_All.RemoveItem(this);
		m_Pawn = null;
	}

	bool IsSpawned()
	{
		return m_Pawn != null;
	}

	PlayerBase GetPawn()
	{
		return m_Pawn;
	}

	vector GetPosition()
	{
		if (m_Pawn)
			return m_Pawn.GetPosition();
		return vector.Zero;
	}

	//! Set body orientation from Euler angles (degrees). yaw = orientation[0].
	void SetOrientation(vector orientation)
	{
		#ifdef DM_BOT_DEBUG_BRAIN
		dmBotLog.Debug("SetOrientation() orientation=" + orientation + " pawn=" + m_Pawn);
		#endif
		if (m_Pawn)
			m_Pawn.SetOrientation(orientation);
	}

	//! Set body facing from a world-space direction vector.
	//! Only the horizontal (yaw) component is used; pitch/roll are zeroed.
	void SetDirection(vector direction)
	{
		#ifdef DM_BOT_DEBUG_BRAIN
		dmBotLog.Debug("SetDirection() direction=" + direction + " pawn=" + m_Pawn);
		#endif
		if (m_Pawn)
		{
			vector orientation = direction.VectorToAngles();
			orientation[1] = 0.0; // pitch
			orientation[2] = 0.0; // roll
			m_Pawn.SetOrientation(orientation);

			dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Pawn);
			if (pawn)
				pawn.SetTargetBodyYaw(orientation[0]);

			#ifdef DM_BOT_DEBUG_BRAIN
			dmBotLog.Debug("SetDirection() applied orientation=" + orientation + " actual=" + m_Pawn.GetOrientation());
			#endif
		}
	}

	vector GetOrientation()
	{
		if (m_Pawn)
			return m_Pawn.GetOrientation();
		return vector.Zero;
	}

	//------------------------------------------------------------------
	// Look control
	//------------------------------------------------------------------

	//! Direct the bot's sight at a world-space point.
	void LookAtPoint(vector pt, dmBotLookTurn turn = dmBotLookTurn.AUTO)
	{
		if (!m_Pawn)
		{
			#ifdef DM_BOT_DEBUG_BRAIN
			dmBotLog.Debug("LookAtPoint() FAILED: no pawn");
			#endif
			return;
		}

		vector pawnPos = m_Pawn.GetPosition();
		vector origin = pawnPos + Vector(0, DM_EYE_HEIGHT, 0);
		vector dir = pt - origin;
		vector angles = dir.VectorToAngles();

		float bodyYaw = m_Pawn.GetOrientation()[0];
		m_TargetLookYawAbs = angles[0]; // absolute world yaw to the target
		m_LookTurnMode = turn;

		//! VectorToAngles returns pitch in [0, 360); normalize to [-180, 180]
		//! so "slightly below level" (e.g. 359.5) doesn't clamp to +85 (look up).
		float pitch = angles[1];
		if (pitch > 180.0)
			pitch -= 360.0;
		m_TargetLookPitch = pitch;

		#ifdef DM_BOT_TRACE_LOOK
		dmBotLog.Trace("LookAtPoint() dir=" + dir + " angles=" + angles + " bodyYaw=" + bodyYaw + " targetYawAbs=" + m_TargetLookYawAbs + " lookPitch=" + m_TargetLookPitch);
		#endif
	}

	//! Direct the bot's sight by offsets from the body direction.
	//! @param v horizontal head turn in degrees (0 = body forward).
	//! @param h vertical head turn in degrees (0 = level).
	void LookAtDirection(float v, float h, dmBotLookTurn turn = dmBotLookTurn.AUTO)
	{
		float bodyYaw = 0.0;
		if (m_Pawn)
			bodyYaw = m_Pawn.GetOrientation()[0];

		m_TargetLookYawAbs = bodyYaw + v;
		m_TargetLookPitch = h;
		m_LookTurnMode = turn;
	}

	//! Reset look to forward (head aligned with body).
	void LookForward()
	{
		LookAtDirection(0.0, 0.0, dmBotLookTurn.NONE);
	}

	//! Set the look target to an absolute world yaw.
	void LookAtYaw(float worldYaw, dmBotLookTurn turn)
	{
		m_TargetLookYawAbs = worldYaw;
		m_TargetLookPitch = 0.0;
		m_LookTurnMode = turn;
	}

	//! Signed angle difference (degrees) from the body yaw to the given world yaw.
	float GetYawTo(float worldYaw)
	{
		float bodyYaw = 0.0;
		if (m_Pawn)
			bodyYaw = m_Pawn.GetOrientation()[0];
		return AngleDiff(worldYaw, bodyYaw);
	}

	//! The bot's heartbeat. Called every frame by the server driver.
	void OnUpdate(float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Bot.Update");
		#endif

		if (!m_Pawn)
			return;

		//! Death -> release the brain; the corpse stays in the world (engine decay).
		if (!m_Pawn.IsAlive())
		{
			OnDeath();
			return;
		}

		//! Incapacitated (unconscious/restrained) -> skip the motor; the pawn's
		//! CommandHandler gate already stops actuation, so don't fight the body.
		if (m_Pawn.IsUnconscious() || m_Pawn.IsRestrained())
			return;

		if (m_FSM)
			m_FSM.Update(pDt);

		UpdateIntents(pDt);
		UpdateLook(pDt);
	}

	//------------------------------------------------------------------
	// FSM
	//------------------------------------------------------------------

	//! Replace the bot's behaviour state machine (built by a preset).
	void SetFSM(dmBotFSM fsm)
	{
		m_FSM = fsm;
	}

	dmBotFSM GetFSM()
	{
		return m_FSM;
	}

	//------------------------------------------------------------------
	// Brain attributes (queried by FSM conditions/states)
	//------------------------------------------------------------------

	//! Health below a threshold (delegates to the pawn).
	bool IsLowHealth()
	{
		if (!m_Pawn)
			return false;
		return m_Pawn.GetHealth01() < 0.35;
	}

	//! (Phase 4) No ammo in the equipped weapon.
	bool HasNoAmmo()
	{
		// TODO Phase 4: inspect the weapon's magazine.
		return false;
	}

	//! (Phase 4) Perceives player signs nearby (killed zombie, campfire, items).
	bool HasPlayerSigns()
	{
		// TODO Phase 4: scan the surroundings.
		return false;
	}

	//------------------------------------------------------------------
	// Movement
	//------------------------------------------------------------------

	//! Set movement direction (relative to the body, degrees) and speed (0..3).
	//! @param angle -180..180: 0 = forward, ±90 = strafe, ±180 = backward.
	//! @param speed 0 = idle, 1 = walk, 2 = run, 3 = sprint.
	void SetMove(float angle, float speed)
	{
		if (!m_Pawn)
			return;

		m_IsMoving = speed > 0.0;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Pawn);
		if (pawn)
			pawn.SetMove(angle, speed);
	}

	//! Set the desired movement direction as a WORLD yaw (degrees). Used by
	//! ComputeBodyYaw to face the movement direction when the body isn't held by a
	//! priority look intent.
	void SetMoveYaw(float worldYaw)
	{
		m_MoveYaw = worldYaw;
	}

	//! Enable/disable forward walking (convenience wrapper over SetMove).
	void SetWalk(bool walk)
	{
		if (walk)
			SetMove(0.0, 1.0);
		else
			SetMove(0.0, 0.0);
	}

	//! Snap the body to face a world point (ignore vertical).
	void FacePoint(vector point)
	{
		if (!m_Pawn)
			return;

		vector pawnPos = m_Pawn.GetPosition();
		vector dir = point - pawnPos;
		dir[1] = 0.0;
		if (dir.Length() < 0.001)
			return;
		SetDirection(dir);
	}

	//! Navmesh path from the bot's current position to a target point.
	//! Snaps the target onto the navmesh first; returns false if the target is
	//! off-navmesh or no path exists. Fills `path` (waypoints incl. start/end).
	bool FindPathTo(vector target, inout array<vector> path)
	{
		if (!m_Pathfinder)
			m_Pathfinder = new dmBotPathfinder();

		vector sampled;
		if (!m_Pathfinder.SamplePosition(target, DM_PATH_SAMPLE_RADIUS, sampled))
			return false;

		return m_Pathfinder.FindPath(GetPosition(), sampled, path);
	}

	//! Set the desired stance (STANCEIDX_ERECT/CROUCH/PRONE). Called by stance
	//! intents during arbitration; applied by the pawn's ApplyStance.
	void SetStance(int stanceIdx)
	{
		if (!m_Pawn)
			return;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Pawn);
		if (pawn)
			pawn.SetStance(stanceIdx);
	}

	//! Set the bot's preferred movement speed (1=walk, 2=jog, 3=sprint).
	void SetPreferredSpeed(float speed)
	{
		m_PreferredSpeed = speed;
	}

	//! Current preferred movement speed (1=walk, 2=jog, 3=sprint).
	float GetPreferredSpeed()
	{
		return m_PreferredSpeed;
	}

	//! Compute the movement speed for reaching toPoint, isolated here so the
	//! movement machinery doesn't hardcode it and character traits can be added.
	//! @param deadline seconds; 0 = no deadline (use preferred speed). If > 0,
	//!        the speed is raised to the minimum needed to reach toPoint in time.
	float CalcSpeed(vector toPoint, float deadline)
	{
		float speed = m_PreferredSpeed;
		if (deadline > 0.0)
		{
			vector pos = GetPosition();
			vector dir = toPoint - pos;
			dir[1] = 0.0;
			float required = dir.Length() / deadline;
			float reqIdx = 1.0;
			if (required > DM_SPEED_JOG)
				reqIdx = 3.0;
			else if (required > DM_SPEED_WALK)
				reqIdx = 2.0;
			if (reqIdx > speed)
				speed = reqIdx;
		}
		return speed;
	}

	//------------------------------------------------------------------
	// Intent pools
	//------------------------------------------------------------------

	void AddFSMIntent(dmBotIntent intent)
	{
		AddIntent(m_FSMIntents, intent);
	}

	void AddPersonalityIntent(dmBotIntent intent)
	{
		AddIntent(m_PersonalityIntents, intent);
	}

	void AddCommandIntent(dmBotIntent intent)
	{
		AddIntent(m_CommandIntents, intent);
	}

	void AddIntent(dmBotIntentPool pool, dmBotIntent intent)
	{
		pool.Insert(intent);
		intent.OnStart(this);
	}

	void RemoveIntent(dmBotIntentPool pool, dmBotIntent intent)
	{
		if (!pool.Has(intent))
			return;
		intent.OnCancel(this);
		pool.Remove(intent);
	}

	//! Drop all FSM intents (called on FSM state transition).
	void ClearFSMIntents()
	{
		m_FSMIntents.Clear(this);
	}

	//! Drop all personality intents.
	void ClearPersonalityIntents()
	{
		m_PersonalityIntents.Clear(this);
	}

	//! Drop all command intents.
	void ClearCommandIntents()
	{
		m_CommandIntents.Clear(this);
	}

	dmBotIntentPool GetFSMIntents()
	{
		return m_FSMIntents;
	}

	//------------------------------------------------------------------
	// Patrol points
	//------------------------------------------------------------------

	ref array<vector> GetPatrolPoints()
	{
		return m_PatrolPoints;
	}

	void AddPatrolPoint(vector point)
	{
		m_PatrolPoints.Insert(point);
	}

	void ClearPatrolPoints()
	{
		m_PatrolPoints.Clear();
	}

	//------------------------------------------------------------------
	// Targets (goals)
	//------------------------------------------------------------------

	void AddTarget(dmTarget target)
	{
		m_Targets.Insert(target);
	}

	void ClearTargets()
	{
		m_Targets.Clear();
	}

	ref array<ref dmTarget> GetTargets()
	{
		return m_Targets;
	}

	//------------------------------------------------------------------
	// Follow (escort)
	//------------------------------------------------------------------

	//! Set the entity the bot escorts (follows alongside). null stops following.
	void SetFollowTarget(EntityAI target)
	{
		m_FollowTarget = target;
	}

	EntityAI GetFollowTarget()
	{
		return m_FollowTarget;
	}

	//! Resolve and execute intents each tick (arbitration, recomputed every tick).
	void UpdateIntents(float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intents");
		#endif

		LookForward();   // взгляд — канал: сброс вперёд; победитель переустанавливает
		SetWalk(false);  // движение — канал: сброс; победитель переустанавливает
		SetStance(DayZPlayerConstants.STANCEIDX_ERECT); // стойка — фоновое «стоять»

		m_FSMIntents.Tick(this, pDt);
		m_CommandIntents.Tick(this, pDt);
		m_PersonalityIntents.Tick(this, pDt);

		//! Без активных намерений арбитровать нечего: сбросы выше уже задали
		//! «покой» (смотреть вперёд, стоять, не двигаться).
		if (m_FSMIntents.Count() == 0 && m_CommandIntents.Count() == 0 && m_PersonalityIntents.Count() == 0)
			return;

		dmBotIntent exclusive = HighestExclusive();
		if (exclusive)
		{
			exclusive.OnUpdate(this, pDt);
			return;
		}

		ExecuteParallel(m_FSMIntents, dmBotIntentPriority.IDLE, pDt);
		ExecuteParallel(m_CommandIntents, dmBotIntentPriority.IDLE, pDt);
		ExecuteParallel(m_PersonalityIntents, dmBotIntentPriority.IDLE, pDt);
		ExecuteParallel(m_FSMIntents, dmBotIntentPriority.DESIRABLE, pDt);
		ExecuteParallel(m_CommandIntents, dmBotIntentPriority.DESIRABLE, pDt);
		ExecuteParallel(m_PersonalityIntents, dmBotIntentPriority.DESIRABLE, pDt);
		ExecuteParallel(m_FSMIntents, dmBotIntentPriority.CRITICAL, pDt);
		ExecuteParallel(m_CommandIntents, dmBotIntentPriority.CRITICAL, pDt);
		ExecuteParallel(m_PersonalityIntents, dmBotIntentPriority.CRITICAL, pDt);
	}

	private void ExecuteParallel(dmBotIntentPool pool, dmBotIntentPriority priority, float pDt)
	{
		ref array<ref dmBotIntent> intents = pool.GetIntents();
		int i;
		for (i = 0; i < intents.Count(); i++)
		{
			dmBotIntent intent = intents[i];
			if (intent.m_Concurrency != dmBotIntentConcurrency.PARALLEL)
				continue;
			if (intent.m_Priority != priority)
				continue;
			intent.OnUpdate(this, pDt);
		}
	}

	private dmBotIntent HighestExclusive()
	{
		dmBotIntent best = null;
		dmBotIntent intent = null;
		int bestOrder = -1;
		int i;

		ref array<ref dmBotIntent> intents = m_FSMIntents.GetIntents();
		for (i = 0; i < intents.Count(); i++)
		{
			intent = intents[i];
			if (intent.m_Concurrency == dmBotIntentConcurrency.EXCLUSIVE && IntentHigher(intent, 0, best, bestOrder))
			{
				best = intent;
				bestOrder = 0;
			}
		}
		intents = m_CommandIntents.GetIntents();
		for (i = 0; i < intents.Count(); i++)
		{
			intent = intents[i];
			if (intent.m_Concurrency == dmBotIntentConcurrency.EXCLUSIVE && IntentHigher(intent, 1, best, bestOrder))
			{
				best = intent;
				bestOrder = 1;
			}
		}
		intents = m_PersonalityIntents.GetIntents();
		for (i = 0; i < intents.Count(); i++)
		{
			intent = intents[i];
			if (intent.m_Concurrency == dmBotIntentConcurrency.EXCLUSIVE && IntentHigher(intent, 2, best, bestOrder))
			{
				best = intent;
				bestOrder = 2;
			}
		}

		return best;
	}

	private bool IntentHigher(dmBotIntent a, int aPoolOrder, dmBotIntent b, int bPoolOrder)
	{
		if (!b)
			return true;
		if (a.m_Priority != b.m_Priority)
			return a.m_Priority > b.m_Priority;
		return aPoolOrder > bPoolOrder;
	}

	//! Smoothly steer the head toward the desired look target. If the target is
	//! beyond the head's turn range, rotate the body so the bot keeps facing it
	//! (looking back over the shoulder instead of getting stuck).
	void UpdateLook(float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Look");
		#endif

		if (!m_Pawn)
			return;

		float t = Math.Min(1.0, DM_LOOK_TURN_SPEED * pDt);

		float bodyYaw = m_Pawn.GetOrientation()[0];
		float relTarget = AngleDiff(m_TargetLookYawAbs, bodyYaw);

		//! Head tracks the target, clamped to the head range.
		float headTarget = Math.Clamp(relTarget, -DM_LOOK_MAX_YAW, DM_LOOK_MAX_YAW);
		float dYaw = AngleDiff(headTarget, m_CurLookYaw);
		float applyYaw = dYaw * t;
		m_CurLookYaw += applyYaw;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Pawn);
		if (pawn)
		{
			pawn.SetLookYaw(m_CurLookYaw);
			pawn.SetLookPitch(m_TargetLookPitch);
			pawn.SetTargetBodyYaw(ComputeBodyYaw(headTarget));
		}

		if (Math.AbsFloat(applyYaw) > 0.1)
			#ifdef DM_BOT_TRACE_LOOK
			dmBotLog.Trace("UpdateLook() targetYawAbs=" + m_TargetLookYawAbs + " bodyYaw=" + bodyYaw + " relTarget=" + relTarget + " curYaw=" + m_CurLookYaw + " pitch=" + m_TargetLookPitch);
			#endif
	}

	//! Body-orientation policy ("comfort"): choose the body yaw from the look
	//! target and the movement direction.
	//!   - FULL (priority look holds the body) -> face the look target (strafe/backpedal);
	//!   - idle + AUTO -> turn over-shoulder to center the head;
	//!   - moving -> face the movement direction (walk where you go);
	//!   - idle + NONE -> keep the body.
	float ComputeBodyYaw(float headTarget)
	{
		if (m_LookTurnMode == dmBotLookTurn.FULL)
			return m_TargetLookYawAbs;
		if (!m_IsMoving && m_LookTurnMode == dmBotLookTurn.AUTO)
			return m_TargetLookYawAbs - headTarget;
		if (m_IsMoving)
			return m_MoveYaw;
		if (m_Pawn)
			return m_Pawn.GetOrientation()[0];
		return 0.0;
	}

	//! Signed angle difference, normalized to (-180, 180].
	static float AngleDiff(float a, float b)
	{
		float d = a - b;
		while (d > 180.0) d -= 360.0;
		while (d < -180.0) d += 360.0;
		return d;
	}

	//------------------------------------------------------------------
	// Registry (statics)
	//------------------------------------------------------------------

	//! Find the controller by pawn.
	static dmAISurvivor Find(PlayerBase pawn)
	{
		dmAISurvivor bot;
		if (s_ByPawn.Find(pawn, bot))
			return bot;
		return null;
	}

	//! Number of currently spawned bots.
	static int Count()
	{
		return s_All.Count();
	}

	//! Override the brain tick interval (seconds). Default DM_BOT_TICK_INTERVAL.
	static void SetTickInterval(float seconds)
	{
		s_TickInterval = seconds;
	}

	//! Advance every bot at a fixed rate. Called by the server driver every frame;
	//! time is accumulated and the brain only runs once per s_TickInterval, so it
	//! doesn't over-tick relative to the pawn simulation (see DM_BOT_TICK_INTERVAL).
	static void TickAll(float pDt)
	{
		s_TickAccum += pDt;
		if (s_TickAccum < s_TickInterval)
			return;

		float dt = s_TickAccum;
		s_TickAccum = 0.0;

		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Tick");
		#endif

		//! Backward iteration: OnUpdate may OnDeath() (death) and remove the bot
		//! from s_All mid-loop; going backward keeps the indices valid.
		for (int i = s_All.Count() - 1; i >= 0; i--)
			s_All[i].OnUpdate(dt);
	}
}
