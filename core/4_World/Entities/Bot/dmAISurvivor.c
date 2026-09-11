//! dmAISurvivor — bot controller (Layer 0 + head look).
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
	private dmAISurvivorBase m_Pawn;
	
	#ifdef DM_BOT_DEBUG_PATHFINDER
	PlayerBase m_DebugPlayer;
	#endif

	//! Desired look direction.
	//! m_TargetLookYawAbs - horizontal look target in WORLD space (degrees).
	//! m_TargetLookPitch  - vertical angle (degrees).
	private float m_TargetLookYawAbs = 0.0;
	private float m_TargetLookPitch = 0.0;

	//! Current (smoothed) horizontal head offset relative to the body, degrees.
	private float m_CurLookYaw = 0.0;

	//! Behaviour state machine (null until a preset is loaded).
	private ref dmBotFSM m_FSM;

	//! Perception (vision): scans for visible threats on a throttled cadence.
	private ref dmVision m_Vision;

	//! Perception (hearing): subscribes to dmNoiseSystem and updates targets on noise.
	private ref dmHearing m_Hearing;

	//! Loot: wishlist (desires), inventory analysis, and the needs coordinator
	//! (inventory -> desires; ticked from OnUpdate on a ~5s throttle).
	private ref dmWishlist m_Wishlist;
	private ref dmRequirements m_Requirements;
	private ref dmNeeds m_Needs;
	private ref dmExplorer m_Explorer;

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

	//! Persistent "tidy inventory" personality intent (stack ammo / reload / load
	//! magazines while out of combat). Recreated when it expires (auto-deadline).
	private ref dmBotIntent_TidyInventory m_TidyIntent;

	//! Reactive escape from a burning fireplace: personal CRITICAL+EXCLUSIVE move
	//! intent that walks DM_DANGER_ESCAPE_DIST away from the danger. null when idle.
	ref dmBotIntent_EscapeDanger m_EscapeIntent;

	//! Patrol points (world positions visited in order).
	private ref array<vector> m_PatrolPoints;

	//! Goal targets (memory, survives FSM transitions).
	private ref array<ref dmTarget> m_Targets;

	//! Entity the bot escorts (follows alongside). null when not escorting.
	private EntityAI m_FollowTarget;

	//! Strike cooldown (seconds) — ticked by the Fighting state, read by the
	//! HitTo/Evasion intents (see GetMeleeCooldown/SetMeleeCooldown).
	private float m_MeleeCooldown = 0.0;

	void dmAISurvivor()
	{
		m_FSMIntents = new dmBotIntentPool();
		m_PersonalityIntents = new dmBotIntentPool();
		m_CommandIntents = new dmBotIntentPool();
		m_PatrolPoints = new array<vector>();
		m_Targets = new array<ref dmTarget>();
		m_Wishlist = new dmWishlist();
		m_Requirements = new dmRequirements();
		m_Needs = new dmNeeds();
		m_Explorer = new dmExplorer();
		m_Hearing = new dmHearing(this);
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

		m_TidyIntent = new dmBotIntent_TidyInventory();
		AddPersonalityIntent(m_TidyIntent);

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

	dmAISurvivorBase GetPawn()
	{
		return m_Pawn;
	}

	vector GetPosition()
	{
		if (m_Pawn)
			return m_Pawn.GetWorldPosition();
		return vector.Zero;
	}

	vector GetDirection()
	{
		if (m_Pawn)
			return m_Pawn.GetDirection();
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

		if (!m_Pawn) return;

		//! Death -> release the brain; the corpse stays in the world (engine decay).
		if (!m_Pawn.IsAlive())
		{
			OnDeath();
			return;
		}

		//! Tick the strike cooldown (bot-level, read by Fighting systems).
		if ( m_MeleeCooldown > 0.0 ) m_MeleeCooldown =- pDt;
		if (m_MeleeCooldown < 0.0) m_MeleeCooldown = 0.0;

		//! Incapacitated (unconscious/restrained) -> skip the motor; the pawn's
		//! CommandHandler gate already stops actuation, so don't fight the body.
		if (m_Pawn.IsUnconscious() || m_Pawn.IsRestrained())
			return;

		if (!m_Vision)
			m_Vision = new dmVision();
		m_Vision.Update(this, pDt);

		m_Needs.Update(this, pDt);

		m_Explorer.OnUpdate(this, pDt);

		if (m_FSM)
			m_FSM.Update(pDt);

		//! The tidy intent never self-finishes (it idles when there's nothing to
		//! do), but the pool drops it on the auto-deadline (DM_INTENT_MAX_AGE) —
		//! recreate it then so the bot keeps tidying.
		if (!m_TidyIntent || m_TidyIntent.IsFinished() || m_TidyIntent.IsExpired())
		{
			m_TidyIntent = new dmBotIntent_TidyInventory();
			AddPersonalityIntent(m_TidyIntent);
		}

		UpdateIntents(pDt);
		m_Pawn.GetInventoryFrames().Tick();
		if (m_Pawn.GetAiming().IsEnabled())
			m_Pawn.GetAiming().OnUpdate(pDt);
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

	dmVision GetVision()
	{
		return m_Vision;
	}

	dmHearing GetHearing()
	{
		return m_Hearing;
	}

	dmWishlist GetWishlist()
	{
		return m_Wishlist;
	}

	dmRequirements GetRequirements()
	{
		return m_Requirements;
	}

	dmNeeds GetNeeds()
	{
		return m_Needs;
	}

	dmExplorer GetExplorer()
	{
		return m_Explorer;
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

	//! Firearm (Weapon_Base) currently in the bot's hands, or null.
	Weapon_Base GetWeaponInHands()
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Pawn);
		if (!pawn)
			return null;
		return Weapon_Base.Cast(pawn.GetHumanInventory().GetEntityInHands());
	}

	//! True when the equipped weapon has no live round in the chamber and no ammo
	//! in its magazine (detachable or internal).
	bool HasNoAmmo()
	{
		Weapon_Base wpn = GetWeaponInHands();
		if (!wpn)
			return true;
		int mi = wpn.GetCurrentMuzzle();
		bool chamberLive = !wpn.IsChamberEmpty(mi) && !wpn.IsChamberFiredOut(mi);
		bool magAmmo = false;
		Magazine mag = wpn.GetMagazine(mi);
		if (mag)
			magAmmo = mag.GetAmmoCount() > 0;
		else
			magAmmo = wpn.GetInternalMagazineCartridgeCount(mi) > 0;
		return !chamberLive && !magAmmo;
	}

	// IsReadyToShoot: IsChamberFiredOut=false IsJammed=false IsChamberEmpty=true
	// Это может означать, что магазин заряжен, но в стволе нет патрона - нужно передернуть затвор
	bool CheckNeedsChamber()
	{
		Weapon_Base wpn = GetWeaponInHands();
		if (!wpn)
			return false;

		int mi = wpn.GetCurrentMuzzle();
		if ( !wpn.IsChamberFiredOut(mi) && !wpn.IsJammed() && wpn.IsChamberEmpty(mi) )
		{
			Magazine mag = wpn.GetMagazine(mi);
			if (mag)
				return mag.GetAmmoCount() > 0;

			return wpn.GetInternalMagazineCartridgeCount(mi) > 0;
		}
		return false;
	}

	//! True when a firearm (non-melee Weapon_Base) is in the bot's hands.
	bool HasFirearmInHands()
	{
		Weapon_Base wpn = GetWeaponInHands();
		if (!wpn)
			return false;
		return !wpn.IsMeleeWeapon();
	}

	//! True when the given firearm has a live round in the chamber or ammo in its
	//! magazine (detachable or internal). Mirrors HasNoAmmo() for an arbitrary weapon.
	private bool IsWeaponLoaded(Weapon_Base wpn)
	{
		if (!wpn)
			return false;
		int mi = wpn.GetCurrentMuzzle();
		bool chamberLive = !wpn.IsChamberEmpty(mi) && !wpn.IsChamberFiredOut(mi);
		if (chamberLive)
			return true;
		Magazine mag = wpn.GetMagazine(mi);
		if (mag)
			return mag.GetAmmoCount() > 0;
		return wpn.GetInternalMagazineCartridgeCount(mi) > 0;
	}

	//! True when the weapon inherits Pistol_Base.
	bool IsPistol(Weapon_Base w)
	{
		if (!w)
			return false;
		return w.IsInherited(Pistol_Base);
	}

	//! True when the weapon inherits Rifle_Base.
	bool IsRifle(Weapon_Base w)
	{
		if (!w)
			return false;
		return w.IsInherited(Rifle_Base);
	}

	//! True when the bot has a loaded firearm (non-melee Weapon_Base with ammo)
	//! anywhere in its inventory, including the hands.
	bool HasLoadedFirearm()
	{
		if (!m_Pawn)
			return false;
		array<EntityAI> items = new array<EntityAI>();
		m_Pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			Weapon_Base w = Weapon_Base.Cast(items[i]);
			if (!w || w.IsMeleeWeapon())
				continue;
			if (IsWeaponLoaded(w))
				return true;
		}
		return false;
	}

	//! Pick the best loaded firearm for an engagement distance: prefer a pistol
	//! below DM_WEAPON_SEL_FAR, a rifle at/above it; fall back to any loaded
	//! firearm; null when there is none.
	Weapon_Base SelectFirearmForRange(float dist)
	{
		if (!m_Pawn) return null;

		array<EntityAI> items = new array<EntityAI>();
		m_Pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
		Weapon_Base anyLoaded = null;
		bool wantPistol = dist < DM_WEAPON_SEL_FAR;
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			Weapon_Base w = Weapon_Base.Cast(items[i]);
			if (!w || w.IsMeleeWeapon())
				continue;
			if (!IsWeaponLoaded(w))
				continue;
			if (!anyLoaded)
				anyLoaded = w;
			if (wantPistol)
			{
				if (IsPistol(w))
					return w;
			}
			else
			{
				if (IsRifle(w))
					return w;
			}
		}

		// TODO Good decision and reload if needed
		// m_TidyIntent = new dmBotIntent_TidyInventory();
		// GetOwner().AddPersonalityIntent(m_TidyIntent);
		if ( (!anyLoaded && HasFirearmInHands() && HasNoAmmo()) || CheckNeedsChamber() )
		{
			if ( m_Pawn.ReloadWeaponAI() )
			{
				return GetWeaponInHands();
			}
		}

		return anyLoaded;
	}

	//! Find a melee weapon in the inventory (including hands); null when none.
	EntityAI SelectMeleeWeapon()
	{
		if (!m_Pawn)
			return null;
		array<EntityAI> items = new array<EntityAI>();
		m_Pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			if ( EntityIsMelee(items[i]) && !items[i].IsRuined() )
				return items[i];
		}
		return null;
	}

	bool EntityIsMelee(EntityAI item)
	{
		return dmLoot.IsMelee(item);
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
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("PathFinder.FindPath");
		#endif

		if (!m_Pathfinder)
			m_Pathfinder = new dmBotPathfinder();

		vector sampled;
		if (!m_Pathfinder.SamplePosition(target, DM_PATH_SAMPLE_RADIUS, sampled))
			return false;

		return m_Pathfinder.FindPath(GetPosition(), sampled, path);
	}

	//! Обнаружить закрытую незапертую дверь прямо перед ботом и запустить
	//! EXCLUSIVE-интент dmBotIntent_OpenDoor (отойти → открыть → дождаться).
	//! Рейкаст вперёд на уровне глаз; возвращает true, если интент запущен.
	bool TryOpenDoorOnPath()
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("PathFinder.OpenDoor");
		#endif
		
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Pawn);
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
		int doorIdx = building.GetDoorIndex(hits[0].component);
		if (doorIdx < 0)
			return false;
		if (building.IsDoorOpen(doorIdx))
			return false;
		if (!building.CanDoorBeOpened(doorIdx, true))
			return false;

		dmBotIntent_OpenDoor intent = new dmBotIntent_OpenDoor();
		intent.m_Building = building;
		intent.m_DoorIdx = doorIdx;
		AddPersonalityIntent(intent);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Bot] TryOpenDoorOnPath: doorIdx=" + doorIdx + " building=" + building);
		#endif
		return true;
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
	// Melee cooldown (brain level)
	//------------------------------------------------------------------

	//! Strike cooldown (seconds) remaining. Ticked by dmBotState_Fighting, read by
	//! the HitTo/Evasion intents to pace strikes and time evasive strafes.
	float GetMeleeCooldown()
	{
		return m_MeleeCooldown;
	}

	void SetMeleeCooldown(float seconds)
	{
		m_MeleeCooldown = seconds;
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

	//! Сбежать от опасности: персональный CRITICAL-интент, цель — на DM_DANGER_ESCAPE_DIST
	//! от dangerPos. Повторный вызов игнорируется, пока побег активен.
	void EscapeDanger(vector dangerPos)
	{
		if (m_EscapeIntent && !m_EscapeIntent.IsFinished() && !m_EscapeIntent.IsExpired())
			return;
		m_EscapeIntent = new dmBotIntent_EscapeDanger();
		m_EscapeIntent.m_DangerPos = dangerPos;
		AddPersonalityIntent(m_EscapeIntent);
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

	//! Find a tracked target by its entity, or null if it isn't remembered.
	dmTarget FindTarget(EntityAI entity)
	{
		int i;
		for (i = 0; i < m_Targets.Count(); i++)
		{
			dmTarget t = m_Targets[i];
			if (t.m_Entity == entity)
				return t;
		}
		return null;
	}

	void RecalcTargetThreat(dmTarget t, float pDt)
	{
		if (t.m_Friendly) return;

		float now = GetGame().GetTickTime();
		ZombieBase z = ZombieBase.Cast(t.m_Entity);
		if ( z && z.IsAlive() )
		{
			vector tPos = z.GetPosition();
			if ( z.m_ActualTarget )
			{
				tPos = z.m_ActualTarget.GetPosition();
			}
			vector myPos = m_Pawn.GetPosition();
			vector d = tPos - myPos;
			d[1] = 0.0;
			float dist = d.Length();

			float newThreat = DM_TARGET_THREAT_ZOMBIE;
			if (dist < 5.0) newThreat = 0.9;
			else
			if (dist < 15.0)
				newThreat = 0.7;
				
			// no reason to relize as a threat zombie that do not attack us
			if (newThreat > t.m_Threat || now - t.m_LastDamage > 5.0)
				t.m_Threat = newThreat;
			return;
		}

		// Когда бот не видит цели, он теряет к цели интерес
		if ( !t.m_HasLOS && now - t.m_LastContact > 45.0 && now - t.m_LastDamage > 120.0 )
		{
			t.m_Threat -= Math.Clamp(0.005 * pDt, 0.0, 0.01);
		}
	}

	//! Add a newly discovered entity to the target memory in a "clean" state —
	//! not yet seen (m_HasLOS=false, no known position). LOS is filled later by
	//! the perception's per-target refresh loop.
	void DiscoverTarget(EntityAI entity, float threat, float attractiveness, bool friendly)
	{
		dmTarget t = FindTarget(entity);
		if (t)
		{
			if ( threat > t.m_Threat ) t.m_Threat = threat;
			if ( attractiveness > t.m_Attractiveness ) t.m_Attractiveness = attractiveness;
			return;
		}
		t = new dmTarget();
		t.m_Type = dmTargetType.DESTROY;
		t.m_Entity = entity;
		t.m_Threat = threat;
		t.m_Attractiveness = attractiveness;
		t.m_Friendly = friendly;
		t.m_HasLOS = false;
		t.m_LastPosition = vector.Zero;
		t.m_NextLOSUpdate = 0.0;
		t.m_LastContact = GetGame().GetTickTime();
		m_Targets.Insert(t);
	}

	//! An enemy dealt damage — register it as a threat immediately, even if it is
	//! outside the vision FOV (e.g. attacking from behind). `source` may be an item
	//! held by the player — the root player is resolved here.
	void RegisterDamageThreat(EntityAI source, float damage)
	{
		if (!source)
			return;
		EntityAI attacker = source;
		Man root = source.GetHierarchyRootPlayer();
		if (root)
			attacker = root;
		if (attacker == m_Pawn)
			return;
		if (attacker == m_FollowTarget)
			return;
		//! Регистрируем угрозой только ЖИВЫХ (люди/зомби/животные) — предметы и
		//! здания (костёр, автомобиль) не должны попадать в цели боя.
		if (!attacker.IsInherited(Man) && !attacker.IsInherited(DayZCreature))
			return;

		float threat = DM_DAMAGE_THREAT_HIGH;
		if (damage < DM_DAMAGE_THREAT_HP_THRESHOLD)
			threat = DM_DAMAGE_THREAT_LOW;
		
		ZombieBase z = ZombieBase.Cast( source );
		if ( z )
		{
			threat = 0.9;
		}

		dmTarget t = FindTarget(attacker);
		if (!t)
		{
			t = new dmTarget();
			t.m_Type = dmTargetType.DESTROY;
			t.m_Entity = attacker;
			m_Targets.Insert(t);
		}
		if (threat > t.m_Threat) t.m_Threat = threat;
		t.m_Friendly = false;
		t.m_LastPosition = attacker.GetPosition();
		t.m_LastContact = GetGame().GetTickTime();
		t.m_LastDamage = GetGame().GetTickTime();
	}

	//! Force-add an entity to the target memory as hostile (used by tests/orders).
	//! Unlike RegisterDamageThreat this is unconditional and takes an explicit threat.
	dmTarget RegisterHostile(EntityAI entity, float threat = 1.0, float spread = 0.0)
	{
		if (!entity) return null;

		dmTarget t = FindTarget(entity);
		if (!t)
		{
			t = new dmTarget();
			t.m_Type = dmTargetType.DESTROY;
			t.m_Entity = entity;
			t.m_Threat = threat;
			t.m_LastContact = GetGame().GetTickTime();
			t.m_LastPosition = entity.GetPosition();
			t.m_LastPositionSpread = spread;
			m_Targets.Insert(t);
		}
		if (threat > t.m_Threat) t.m_Threat = threat;
		t.m_Friendly = false;
		return t;
	}

	//! Слух: обновить/добавить цель по шуму. Цель с HasLOS=false получает свежую
	//! последнюю позицию и обновлённую привлекательность; новая цель добавляется с
	//! низким threat (услышал, не видел) и привлекательностью шума.
	void HearNoise(EntityAI entity, vector position, float attractiveness)
	{
		#ifdef DM_PERCEPTION_DEBUG
		if ( entity )
			dmBotLog.Debug("[Noise] HearNoise: position=" + position + " attractiveness=" + attractiveness + " [" + entity.GetType() + "]");
		else
			dmBotLog.Debug("[Noise] HearNoise: position=" + position + " attractiveness=" + attractiveness);
		#endif
		dmTarget t = FindTarget(entity);
		if (t)
		{
			if (!t.m_HasLOS)
			{
				if (!t.m_Friendly && attractiveness > t.m_Attractiveness)
					t.m_Attractiveness = attractiveness;

				vector botPos = GetPosition();
				float L1 = vector.Distance(botPos, position);       // бот → шум
				// facing: угол между направлением тела бота и направлением на шум (0..180)
				vector toNoise = position - botPos;
				float noiseYaw = toNoise.VectorToAngles()[0];
				vector bodyDir = GetDirection();
				float lookYaw = bodyDir.VectorToAngles()[0];
				float facing = Math.AbsFloat(dmAISurvivor.AngleDiff(noiseYaw, lookYaw));
				float facingFactor = DM_HUNT_FACING_MIN + (facing / 180.0) * (DM_HUNT_FACING_MAX - DM_HUNT_FACING_MIN);
				t.m_LastPositionSpread = Math.Clamp(L1 * facingFactor, DM_HUNT_SPREAD_MIN, DM_HUNT_SPREAD_MAX);

				t.m_LastPosition = position;
			}
			return;
		}

		DiscoverTarget(entity, DM_NOISE_THREAT, attractiveness, false);
		t = FindTarget(entity);
		if (t)
			t.m_LastPosition = position;
	}

	dmTarget m_LastHostileTarget; // текущая цель

	//! Ближайшая враждебная цель (threat >= DM_ATTACK_THREAT_THRESHOLD, не friendly,
	//! живая). Без ограничения дистанции; ближайшая побеждает (ничья — выше threat).
	dmTarget GetHostileTarget()
	{
		// если ближайшая видимая цель все еще жива и это текущая цель, то пока закрепляемся на ней
		dmTarget t = GetHostileTargetEx(true);
		if ( t && m_LastHostileTarget == t && t.m_Entity && t.m_Entity.IsAlive() )
		{
			return t;
		}
		// проверим ближайшую цель без учета видимости, если она и есть видимая, то идем по ней
		dmTarget n = GetHostileTargetEx(false);
		if ( t == n || !n ) return t;

		// единственная цель - вне видимости
		if ( !t && n )
		{
			m_LastHostileTarget = n;
			return n;
		}

		// иначе, нам нужно проверить дистанцию и решить какую цель выбрать
		vector myPos = GetPosition();
		vector dn = n.m_LastPosition - myPos; // до ближайшей цели
		vector dt = t.m_LastPosition - myPos; // до ближайшей видимой цели
		
		// все таки цель без учета видимости, если она на много ближе видимой цели, остается в фокусе (если она была в фокусе)
		if ( m_LastHostileTarget == n && n.m_Entity && n.m_Entity.IsAlive() && dn.Length() < dt.Length() / 10 )
		{
			return n;
		}

		// иначе переключаемся на видимую цель
		m_LastHostileTarget = t;
		return t;
	}

	dmTarget GetHostileTargetEx(bool visible)
	{
		dmTarget best = null;
		float bestDist = 0.0;
		float bestThreat = 0.0;
		vector myPos = GetPosition();
		int i;
		for (i = 0; i < m_Targets.Count(); i++)
		{
			dmTarget t = m_Targets[i];
			if (t.m_Friendly) continue;
			if (visible && !t.m_HasLOS) continue;

			if (t.m_Threat < DM_ATTACK_THREAT_THRESHOLD) continue;

			EntityAI e = t.m_Entity;
			if (!e || !e.IsAlive()) continue;

			vector tPos;
			if (e)
				tPos = e.GetPosition();
			else
				tPos = t.m_LastPosition;
				
			vector d = tPos - myPos;
			d[1] = 0.0;
			float dist = d.Length();
			if (!best || dist < bestDist || (dist == bestDist && t.m_Threat > bestThreat))
			{
				best = t;
				bestDist = dist;
				bestThreat = t.m_Threat;
			}
		}
		return best;
	}

	//! Первая цель, пригодная для охоты: невидимая (!m_HasLOS), не дружественная,
	//! живая, и либо враждебная с неточной позицией, либо достаточно привлекательная.
	dmTarget GetHuntTarget()
	{
		int i;
		for (i = 0; i < m_Targets.Count(); i++)
		{
			dmTarget t = m_Targets[i];
			if (t.m_Friendly)
				continue;
			if (t.m_HasLOS)
				continue;
			EntityAI e = t.m_Entity;
			if (e && !e.IsAlive())
				continue;
			bool hostileSpread = t.m_Threat >= DM_ATTACK_THREAT_THRESHOLD;
			bool attractive = t.m_Attractiveness > DM_HUNT_MIN_ATTRACTIVENESS;
			if (hostileSpread || attractive)
				return t;
		}
		return null;
	}

	//! Start a scan pass: mark every remembered target as not-seen; RememberTarget
	//! flips the visible ones back to true.
	void BeginTargetScan()
	{
		int i;
		for (i = 0; i < m_Targets.Count(); i++)
		{
			dmTarget t = m_Targets[i];
			t.m_HasLOS = false;
		}
	}

	//! Merge a freshly seen entity into the target memory (create or update).
	void RememberTarget(EntityAI entity, float threat, float attractiveness, bool friendly, vector pos)
	{
		dmTarget t = FindTarget(entity);
		if (!t)
		{
			t = new dmTarget();
			t.m_Type = dmTargetType.DESTROY;
			t.m_Entity = entity;
			m_Targets.Insert(t);
		}
		if (threat > t.m_Threat) t.m_Threat = threat;
		t.m_Attractiveness = attractiveness;
		t.m_Friendly = friendly;
		t.m_LastPosition = pos;
		t.m_HasLOS = true;
		t.m_LastContact = GetGame().GetTickTime();
	}

	//! Drop targets with no contact for longer than the timeout (backward loop —
	//! safe while removing items).
	void ForgetStaleTargets(float timeout)
	{
		float now = GetGame().GetTickTime();
		int i;
		for (i = m_Targets.Count() - 1; i >= 0; i--)
		{
			dmTarget t = m_Targets[i];
			// default contact - it means that the record is just added
			if ( t.m_LastContact == 0 )
				t.m_LastContact = GetGame().GetTickTime();
			if (!t.m_Entity || now - t.m_LastContact > timeout || !t.m_Entity.IsAlive())
				m_Targets.RemoveItem(t);
		}
	}

	//! Remove a specific target from the memory.
	void RemoveTarget(dmTarget t)
	{
		m_Targets.RemoveItem(t);
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

	//------------------------------------------------------------------
	// Intents arbitration
	//------------------------------------------------------------------

	ref map<dmBotIntentsChannel, dmBotIntent> m_Winner = new map<dmBotIntentsChannel, dmBotIntent>();

	//! Resolve and execute intents each tick (arbitration, recomputed every tick).
	void UpdateIntents(float pDt)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Intents");
		#endif

		// Clear before fair arbitration
		m_Winner.Set(dmBotIntentsChannel.NONE, null);
		m_Winner.Set(dmBotIntentsChannel.LOOK, null);
		m_Winner.Set(dmBotIntentsChannel.MOVE, null);
		m_Winner.Set(dmBotIntentsChannel.STANCE, null);
		m_Winner.Set(dmBotIntentsChannel.EMOTION, null);
		m_Winner.Set(dmBotIntentsChannel.ATTACK, null);

		IntentsArbitrationPool(m_PersonalityIntents, pDt);
		IntentsArbitrationPool(m_CommandIntents, pDt);
		IntentsArbitrationPool(m_FSMIntents, pDt);

		//! Default rest: channels nobody won fall back to "at ease".
		if (!m_Winner.Get(dmBotIntentsChannel.LOOK)) LookForward();
		if (!m_Winner.Get(dmBotIntentsChannel.MOVE)) SetWalk(false);
		if (!m_Winner.Get(dmBotIntentsChannel.STANCE)) SetStance(DayZPlayerConstants.STANCEIDX_ERECT);

		IntentsTickAges(m_PersonalityIntents, pDt);
		IntentsTickAges(m_CommandIntents, pDt);
		IntentsTickAges(m_FSMIntents, pDt);
	}

	private void IntentsArbitrationPool(dmBotIntentPool pool, float pDt)
	{
		ref array<ref dmBotIntent> intents = pool.GetIntents();
		if (IntentsArbitration(intents, dmBotIntentPriority.CRITICAL, pDt)) return;
		if (IntentsArbitration(intents, dmBotIntentPriority.DESIRABLE, pDt)) return;
		if (IntentsArbitration(intents, dmBotIntentPriority.IDLE, pDt)) return;
	}

	private bool IntentsArbitration(array<ref dmBotIntent> intents, dmBotIntentPriority priority, float pDt)
	{
		if (IntentsArbitrationConcurrency(intents, priority, dmBotIntentConcurrency.EXCLUSIVE, pDt)) return true;
		IntentsArbitrationConcurrency(intents, priority, dmBotIntentConcurrency.PARALLEL, pDt);
		return false;
	}

	private bool IntentsArbitrationConcurrency(array<ref dmBotIntent> intents, dmBotIntentPriority priority, dmBotIntentConcurrency concurrency, float pDt)
	{
		for (int i = 0; i < intents.Count(); i++)
		{
			dmBotIntent intent = intents[i];
			if (intent.m_Priority != priority) continue;

			if (intent.IsFinished()) continue;
			if (intent.IsFailed()) continue;
			if (intent.IsExpired()) continue;
			if (!intent.IsActive()) continue;
			if (intent.m_Concurrency != concurrency) continue;
			
			dmBotIntent winner = m_Winner.Get(intent.m_Manage);
			if ( winner && winner != intent )
			{
				intent.OnSkip(this, pDt);
				continue;
			}

			m_Winner.Set(intent.m_Manage, intent);
			intent.OnUpdate(this, pDt);
			if (concurrency == dmBotIntentConcurrency.EXCLUSIVE) return true;
		}
		return false;
	}

	private void IntentsTickAges(dmBotIntentPool pool, float pDt)
	{
		pool.Tick(this, pDt);
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

	//! Убить всех заспавненных ботов (Health=0). Трупы остаются движку на протухание.
	//! Возвращает число убитых. Итерация с конца — безопасна, если смерть снимет из s_All.
	static int KillAll()
	{
		int killed = 0;
		int i;
		for (i = s_All.Count() - 1; i >= 0; i--)
		{
			dmAISurvivor bot = s_All[i];
			if (bot && bot.IsSpawned() && bot.GetPawn().IsAlive())
			{
				bot.GetPawn().SetHealth("", "Health", 0.0);
				killed++;
			}
		}
		return killed;
	}

	//! Удалить всех ботов из мира (Despawn → ObjectDelete) и снять их из реестра.
	//! Возвращает число удалённых.
	static int ClearAll()
	{
		int cleared = 0;
		int i;
		for (i = s_All.Count() - 1; i >= 0; i--)
		{
			dmAISurvivor bot = s_All[i];
			if (bot)
			{
				bot.Despawn();
				cleared++;
			}
		}
		return cleared;
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

	//------------------------------------------------------------------
	// Looting
	//------------------------------------------------------------------

	float CalcDesired(ItemBase item)
	{
		if ( item.IsClothing() )
		{
			if ( IsBetterClothing(item) ) return 1.0;
			return 0.0;   // не лучше надетого — не берём
		}

		float wishIndex = m_Wishlist.CalcDesired( item );
		if ( wishIndex >= 1.0 ) return 1.0;

		if (item.IsMagazine())
		{
			if ( IsSuitableMagazine(item) ) wishIndex = 0.7;
		}

		if (item.IsAmmoPile())
		{
			if ( IsSuitableAmmo(item) ) wishIndex = 0.9;
		}

		return wishIndex;
	}

	//! True when `item` is better clothing than what's worn in its slot: an empty
	//! slot wins, then a strict lexicographic order — cargo capacity, heat isolation,
	//! health, max health; first difference decides, all equal → not better.
	bool IsBetterClothing(ItemBase item)
	{
		PlayerBase pawn = m_Pawn;
		if (!pawn || !item)
			return false;
		GameInventory inv = pawn.GetInventory();
		if (!inv)
			return false;

		array<string> slotNames = new array<string>();
		item.ConfigGetTextArray("inventorySlot", slotNames);

		int i;
		for (i = 0; i < slotNames.Count(); i++)
		{
			int slotId = InventorySlots.GetSlotIdFromString(slotNames[i]);
			if (slotId == InventorySlots.INVALID)
				continue;

			ItemBase current = ItemBase.Cast(inv.FindAttachment(slotId));
			if (!current)
				return true;

			int itemCargo = CargoCapacity(item);
			int currentCargo = CargoCapacity(current);
			if (itemCargo > currentCargo)
				return true;
			if (itemCargo < currentCargo)
				return false;

			if (item.GetHeatIsolation() > current.GetHeatIsolation())
				return true;
			if (item.GetHeatIsolation() < current.GetHeatIsolation())
				return false;

			if (item.GetHealth() > current.GetHealth())
				return true;
			if (item.GetHealth() < current.GetHealth())
				return false;

			if (item.GetMaxHealth() > current.GetMaxHealth())
				return true;
			return false;
		}
		return false;
	}

	//! Storage capacity (grid cells) of an item's own cargo; 0 when it has no cargo.
	private int CargoCapacity(ItemBase ib)
	{
		CargoBase cargo = ib.GetInventory().GetCargo();
		if (!cargo)
			return 0;
		return cargo.GetWidth() * cargo.GetHeight();
	}

	bool IsSuitableAmmo(ItemBase item)
	{
		if (!item || !item.IsAmmoPile())
			return false;
		if (!m_Pawn)
			return false;
		Magazine ammo = Magazine.Cast(item);
		if (!ammo)
			return false;

		array<EntityAI> items = new array<EntityAI>();
		m_Pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int i;
		for (i = 0; i < items.Count(); i++)
		{
			Weapon_Base w = Weapon_Base.Cast(items[i]);
			if (!w || EntityIsMelee(w))
				continue;
			int mi;
			for (mi = 0; mi < w.GetMuzzleCount(); mi++)
			{
				if (w.CanChamberFromMag(mi, ammo))
				{
					#ifdef DM_BOT_DEBUG_LOOTING
					dmBotLog.Debug("[Loot] IsSuitableAmmo: " + item.GetType() + " подходит к " + w.GetType());
					#endif
					return true;
				}
			}
		}
		return false;
	}

	bool IsSuitableMagazine(ItemBase item)
	{
		if (!item || !item.IsMagazine())
			return false;
		if (!m_Pawn)
			return false;
		Magazine mag = Magazine.Cast(item);
		if (!mag)
			return false;

		WeaponManager wm = m_Pawn.GetWeaponManager();
		if (!wm)
			return false;

		array<EntityAI> items = new array<EntityAI>();
		m_Pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int i;
		for (i = 0; i < items.Count(); i++)
		{
			Weapon_Base w = Weapon_Base.Cast(items[i]);
			if (!w || EntityIsMelee(w))
				continue;
			if (wm.CanAttachMagazine(w, mag) || wm.CanSwapMagazine(w, mag))
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] IsSuitableMagazine: " + item.GetType() + " подходит к " + w.GetType());
				#endif
				return true;
			}
		}
		return false;
	}
}
