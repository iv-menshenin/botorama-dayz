//! dmAISurvivorBase — AI survivor entity (client-server mod).
//!
//! The entity IS the bot: it inherits PlayerBase (so it has full character
//! simulation). The head look is applied in CommandHandler by setting the custom
//! animation graph variables dmAI_Look/dmAI_LookDirX/dmAI_LookDirY (added to a
//! custom player_main.agr, referenced via enfAnimSys/graphName in config.cpp).
//! The vanilla Look/LookDirX/LookDirY are kept so the engine's native "look at"
//! still finds them by hash; the graph's look poses now read the custom variables.

//! Aiming mode: how the weapon is brought to bear (hip-fire / ADS).
enum dmBotAimMode
{
	HIP, // от бедра — raised, без ADS
	ADS  // прицельно — смотрим в то, что есть у оружия (мушка или оптика)
};

class dmAISurvivorBase : PlayerBase
{
	//! Head look offset (degrees, relative to the body).
	private float m_LookYawDeg = 0.0;
	private float m_LookPitchDeg = 0.0;

	//! Last logged look (client side) to throttle the OnVariablesSynchronized log.
	private float m_LastLogLookYaw = 0.0;
	private float m_LastLogLookPitch = 0.0;

	//! Animation graph variable indices (bound lazily on each side).
	private int m_VarLook = -1;
	private int m_VarLookDirX = -1;
	private int m_VarLookDirY = -1;
	private bool m_VarsBound = false;

	//! Body turn (foot-stepping) animation variable and commands.
	private int m_VarTurnAmount = -1;
	private int m_CmdTurn = -1;
	private int m_CmdStopTurn = -1;

	//! Weapon raise (aiming) state: a flag + timer, like Expansion eAIBase, rather
	//! than a raised stance. AimX/AimY are bound now but used by A3.
	private bool m_WeaponRaised = false;
	private float m_WeaponRaisedTimer = 0.0;
	private float m_RaiseReadyDuration = 0.5;
	private dmBotAimMode m_AimMode = dmBotAimMode.ADS;
	private int m_VarRaised = -1;
	private int m_VarAimX = -1;
	private int m_VarAimY = -1;
	private int m_VarAimIKX = -1;

	//! Weapon aim direction relative to the body: left/right (yaw) and up/down
	//! (pitch), degrees. Computed by SetAimTarget, pushed to dmAI_AimX/dmAI_AimY.
	private float m_AimRelAngleLR = 0.0;
	private float m_AimRelAngleUD = 0.0;

	//! World-space aim direction (normalized), stored directly by SetAimDirection
	//! and read by ComputeShot. Kept authoritative instead of being reconstructed
	//! from body yaw + relative angles (the body may have turned since then).
	private vector m_AimWorldDirection;

	//! Идеальный прицел для тестов: нулевой разброс (личный и оружейный).
	private bool m_PerfectAim;

	void SetPerfectAim(bool v)
	{
		m_PerfectAim = v;
	}

	bool IsPerfectAim()
	{
		return m_PerfectAim;
	}

	//! Smoothed copy of m_AimRelAngleLR/UD, pushed to the animation graph so the
	//! barrel rotates smoothly. The raw angles stay authoritative for
	//! GetWeaponAimDirection() (the actual shot direction).
	private float m_AimSmoothedLR = 0.0;
	private float m_AimSmoothedUD = 0.0;

	//! Last time (GetGame().GetTickTime()) the weapon-aim debug log was printed.
	private float m_LastAimLogTime = 0.0;

	//! Last time (GetGame().GetTickTime()) the raise-readiness debug log was printed.
	private float m_LastReadyLogTime = 0.0;

	//! Last time (GetGame().GetTickTime()) the ADS/aim-mode debug log was printed.
	private float m_LastADSLogTime = 0.0;

	//! Shooting accuracy model (dispersion). Created here, wired into the fire
	//! path in Phase 3. dmAiming is a plain class -> ref.
	private ref dmAiming m_Aiming;

	//! Inventory frame-sequence manager (Phase 2 builders). dmInventoryFrames is a
	//! plain class -> ref.
	private ref dmInventoryFrames m_InventoryFrames;

	//! Desired body yaw (world, degrees), set by the controller each tick.
	private float m_TargetBodyYaw = 0.0;

	//! 0 = not turning, 1 = foot-stepping turn in progress.
	private int m_TurnState = 0;
	private float m_TurnTime = 0.0;

	//! Whether the brain intends the bot to move (set by SetMove). Used to pick
	//! slide-turn (moving) vs foot-step turn (idle) — NOT GetCurrentMovementSpeed(),
	//! which flips non-zero during the foot-step turn itself and cancels it.
	private bool m_IsMoving = false;

	//! Desired movement direction (relative to body, degrees) and speed (0..3),
	//! written by the brain and applied per-frame by ApplyMovement.
	private float m_DesiredMoveAngle = 0.0;
	private float m_DesiredSpeed = 0.0;

	//! Desired stance (STANCEIDX_*), written by the brain, applied by ApplyStance.
	private int m_DesiredStance;

	//! Timeout until the next stance-change step may be forced.
	private float m_StanceTimeout = 0.0;

	//! Actual (smoothed) movement speed, ramped toward m_DesiredSpeed each frame.
	private float m_ActualSpeed = 0.0;

	//! True while moving with a sharp turn (|dBody| > DM_MOVE_TURN_SLOW_THRESHOLD);
	//! ApplyMovement caps the speed to DM_MOVE_TURN_SLOW_SPEED while turning.
	private bool m_TurnSharp = false;

	//! Accumulator for the reduced-frequency body-modifier tick (see
	//! DM_BOT_MODIFIER_TICK_INTERVAL).
	private float m_ModifierTickAccum = 0.0;

	//! Accumulator for the periodic body-stats debug log (DM_BOT_DEBUG_BODY).
	private float m_BodyDebugAccum = 0.0;

	//! Accumulator for the periodic movement-apply debug log (DM_BOT_DEBUG_BODY).
	private float m_MoveDebugAccum = 0.0;

	//! Diagnostic: last logged position, for the per-frame displacement log
	//! (DM_BOT_DEBUG_PERFRAME_MOVING_LOG).
	private vector m_LastLogPos;
	private bool m_LastLogPosValid = false;

	//! One-shot melee attack request from the brain (see RequestMeleeAttack). The
	//! fight logic consumes it as soon as the strike starts.
	private bool m_MeleeAttackRequest = false;
	private EntityAI m_MeleeTarget;

	//! One-shot fire request from the brain (see RequestFire). Processed by
	//! TryFireWeapon inside the CommandHandler; m_FireCooldown throttles cadence.
	private bool m_FireRequest = false;
	private float m_FireCooldown = 0.0;

	//! Time (GetGame().GetTickTime()) of the last successful shot, for the
	//! burst/auto series pacing and the proactive fire-mode refresh.
	private float m_LastFireTime = 0.0;

	void dmAISurvivorBase()
	{
		m_DesiredStance = DayZPlayerConstants.STANCEIDX_ERECT;

		RegisterNetSyncVariableFloat("m_LookYawDeg", -DM_LOOK_MAX_YAW, DM_LOOK_MAX_YAW, 1);
		RegisterNetSyncVariableFloat("m_LookPitchDeg", -DM_LOOK_MAX_PITCH, DM_LOOK_MAX_PITCH, 1);

		//! Replace the vanilla melee combat + fight logic. The vanilla
		//! DayZPlayerMeleeFightLogic_LightHeavy.HandleFightLogic null-derefs hcm
		//! (HumanCommandMove) whenever the bot isn't in the MOVE command, because
		//! CanFight() is true for an AI bot (no ActionManager). The fight logic
		//! must be created after the combat, so it picks up our dmBotMeleeCombat.
		m_MeleeCombat = new dmBotMeleeCombat(this);
		m_MeleeFightLogic = new dmBotMeleeFightLogic_LightHeavy(this);

		//! Replace the vanilla WeaponManager (its StartAction returns false on a
		//! multiplayer server without a control_action) with our server-path
		//! subclass so reload/unjam/eject run from the CommandHandler.
		m_WeaponManager = new dmBotWeaponManager(this);

		m_Aiming = new dmAiming(this);
		m_InventoryFrames = new dmInventoryFrames(this);
	}

	//! The inventory frame-sequence manager (Phase 2 builders enqueue into it).
	dmInventoryFrames GetInventoryFrames()
	{
		return m_InventoryFrames;
	}

	//! Build a single action frame (verb + item + slot + target container).
	private dmInventoryFrame MakeInventoryAction(dmInventoryDoing verb, ItemBase item, int slotId, EntityAI to)
	{
		dmInventoryFrame frame = new dmInventoryFrame();
		frame.m_ToDo = verb;
		frame.m_Item = item;
		frame.m_SlotId = slotId;
		frame.m_To = to;
		return frame;
	}

	//! Цепочка переноса ВСЕГО карго из `from` в `to` (по предмету за фрейм; не влезло/уничтожен — на пол). Возвращает голову или null.
	dmInventoryFrame InventoryMoveCargo(ItemBase from, ItemBase to)
	{
		CargoBase cargo = from.GetInventory().GetCargo();
		if (!cargo)
			return null;
		dmInventoryFrame head = null;
		int i;
		for (i = cargo.GetItemCount() - 1; i >= 0; i--)
		{
			ItemBase item = ItemBase.Cast(cargo.GetItem(i));
			if (!item)
				continue;
			dmInventoryFrame move = MakeInventoryAction(dmInventoryDoing.TAKEINTOCARGO, item, -1, to);
			dmInventoryFrame drop = MakeInventoryAction(dmInventoryDoing.PLACEONGROUND, item, -1, null);
			move.m_OnSuccess = head;
			move.m_OnFail = drop;
			drop.m_OnSuccess = head;
			drop.m_OnFail = head;
			head = move;
		}
		return head;
	}

	//! Сменить одежду: сбросить старую → надеть новую → перенести карго; при неудаче
	//! надеть старую обратно. Ставит цепочку в очередь, возвращает корень (контроль IsAllDone).
	dmInventoryFrame InventoryChangeClothes(ItemBase newItem)
	{
		if (!newItem)
			return null;
		array<string> slotNames = new array<string>();
		newItem.ConfigGetTextArray("inventorySlot", slotNames);
		if (slotNames.Count() == 0)
			return null;
		int slotId = InventorySlots.GetSlotIdFromString(slotNames[0]);
		if (slotId == InventorySlots.INVALID)
			return null;

		ItemBase old = ItemBase.Cast(GetInventory().FindAttachment(slotId));

		dmInventoryFrame moveCargo = null;
		if (old)
			moveCargo = InventoryMoveCargo(old, newItem);

		dmInventoryFrame wearNew = MakeInventoryAction(dmInventoryDoing.ATTACHTOSLOT, newItem, slotId, null);
		wearNew.m_OnSuccess = moveCargo;

		dmInventoryFrame root;
		if (old)
		{
			dmInventoryFrame wearBack = MakeInventoryAction(dmInventoryDoing.ATTACHTOSLOT, old, slotId, null);
			wearNew.m_OnFail = wearBack;

			dmInventoryFrame dropOld = MakeInventoryAction(dmInventoryDoing.PLACEONGROUND, old, -1, null);
			dropOld.m_OnSuccess = wearNew;
			dropOld.m_OnFail = null;
			root = dropOld;
		}
		else
		{
			root = wearNew;
		}

		if (m_InventoryFrames)
			m_InventoryFrames.Enqueue(root);
		return root;
	}

	//! Подобрать предмет: одежда → InventoryChangeClothes; прочее → в карго рюкзака (Back). Возвращает корень или null.
	dmInventoryFrame InventoryPickUp(ItemBase item)
	{
		if (!item)
			return null;
		if (item.IsClothing())
			return InventoryChangeClothes(item);

		EntityAI bag = GetInventory().FindAttachment(InventorySlots.GetSlotIdFromString("Back"));
		if (!bag)
			return null;

		dmInventoryFrame frame = MakeInventoryAction(dmInventoryDoing.TAKEINTOCARGO, item, -1, bag);
		if (m_InventoryFrames)
			m_InventoryFrames.Enqueue(frame);
		return frame;
	}

	//! Bind the custom head-look animation graph variables.
	void BindLookVars()
	{
		if (m_VarsBound)
			return;

		HumanAnimInterface hai = GetAnimInterface();
		if (hai)
		{
			m_VarLook = hai.BindVariableBool("dmAI_Look");
			m_VarLookDirX = hai.BindVariableFloat("dmAI_LookDirX");
			m_VarLookDirY = hai.BindVariableFloat("dmAI_LookDirY");
			m_VarTurnAmount = hai.BindVariableFloat("dmAI_TurnAmount");
			m_CmdTurn = hai.BindCommand("dmAI_Turn");
			m_CmdStopTurn = hai.BindCommand("dmAI_StopTurn");
			m_VarRaised = hai.BindVariableBool("dmAI_Raised");
			m_VarAimX = hai.BindVariableFloat("dmAI_AimX");
			m_VarAimY = hai.BindVariableFloat("dmAI_AimY");
			m_VarAimIKX = hai.BindVariableFloat("dmAI_AimIKX");
			m_VarsBound = true;

			#ifdef DM_BOT_DEBUG_PAWN
			dmBotLog.Debug("dmAISurvivorBase.BindLookVars() Look=" + m_VarLook + " LookDirX=" + m_VarLookDirX + " LookDirY=" + m_VarLookDirY + " TurnAmount=" + m_VarTurnAmount + " CmdTurn=" + m_CmdTurn + " CmdStopTurn=" + m_CmdStopTurn + " instType=" + GetInstanceType());
			#endif
		}
		else
		{
			#ifdef DM_BOT_DEBUG_PAWN
			dmBotLog.Debug("dmAISurvivorBase.BindLookVars() GetAnimInterface() returned null instType=" + GetInstanceType());
			#endif
		}
	}

	//! Push the current look state into the animation graph variables.
	void ApplyLookVars()
	{
		BindLookVars();

		bool hasLook = Math.AbsFloat(m_LookYawDeg) > 0.01 || Math.AbsFloat(m_LookPitchDeg) > 0.01;

		if (m_VarLook >= 0)
			AnimSetBool(m_VarLook, hasLook);
		if (m_VarLookDirX >= 0)
			AnimSetFloat(m_VarLookDirX, m_LookYawDeg);
		if (m_VarLookDirY >= 0)
			AnimSetFloat(m_VarLookDirY, m_LookPitchDeg);
	}

	//! Raise/lower the weapon. This is a flag + timer (raised pose driven by the
	//! dmAI_Raised graph variable), NOT ForceStance(RAISEDERECT) — see
	//! docs/research/combat.md. Called by the brain (A3 shooting state).
	void RaiseWeapon(bool up = true)
	{
		if (up && !m_WeaponRaised)
			m_RaiseReadyDuration = GetRaiseReadyDuration();
		m_WeaponRaised = up;
	}

	//! Select the aiming mode (hip-fire / ironsights / optics), applied each frame
	//! by ApplyWeaponADS.
	void SetAimMode(dmBotAimMode mode)
	{
		m_AimMode = mode;
		if (m_WeaponRaised && m_WeaponRaisedTimer < m_RaiseReadyDuration)
			m_RaiseReadyDuration = GetRaiseReadyDuration();
	}

	//! Full readiness duration (seconds) for the current aim mode: HIP is just the
	//! raise, ADS adds bringing the sight onto the target, and a magnified optic
	//! adds acquiring the target in the magnification.
	float GetRaiseReadyDuration()
	{
		if (m_AimMode == dmBotAimMode.HIP)
			return DM_AIM_RAISE_TIME;

		float duration = DM_AIM_RAISE_TIME + DM_AIM_LOOK_TIME;
		Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
		if (weapon)
		{
			ItemOptics optics = weapon.GetAttachedOptics();
			if (optics && optics.GetZoomMax() > 0.0)
				duration += DM_AIM_ACQUIRE_TIME;
		}
		return duration;
	}

	//! Whether the weapon is still going through its readiness timings (raising,
	//! bringing the sight up, acquiring the target in the optic).
	bool IsRaising()
	{
		return m_WeaponRaised && m_WeaponRaisedTimer < m_RaiseReadyDuration;
	}

	bool IsWeaponReady()
	{
		if (IsClimbing() || IsFalling() || IsSwimming() || IsClimbingLadder()) return false;

		Weapon_Base wpn = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
		if (!wpn) return false;

		int mi = wpn.GetCurrentMuzzle();
		if (wpn.IsChamberFiredOut(mi) || wpn.IsJammed() || wpn.IsChamberEmpty(mi)) return false;

		return true;
	}

	//! Whether the weapon is fully ready to fire (all readiness timings elapsed).
	bool IsReadyToShoot()
	{
		if (IsClimbing() || IsFalling() || IsSwimming() || IsClimbingLadder()) return false;

		if ( m_WeaponRaised && m_WeaponRaisedTimer >= m_RaiseReadyDuration )
		{
			Weapon_Base wpn = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
			if (!wpn) return false;

			int mi = wpn.GetCurrentMuzzle();
			if (wpn.IsChamberFiredOut(mi) || wpn.IsJammed() || wpn.IsChamberEmpty(mi))
			{
				#ifdef DM_WEAPON_DEBUG_FSM
				dmBotLog.Debug("[Weapon] IsReadyToShoot: IsChamberFiredOut=" + wpn.IsChamberFiredOut(mi) + " IsJammed=" + wpn.IsJammed() + " IsChamberEmpty=" + wpn.IsChamberEmpty(mi));
				#endif
				return false;
			}

			return true;
		}

		return false;
	}

	bool CheckNeedsChamber(Weapon_Base wpn, int mi)
	{
		if ( !wpn.IsChamberFiredOut(mi) && !wpn.IsJammed() && wpn.IsChamberEmpty(mi) )
		{
			Magazine mag = wpn.GetMagazine(mi);
			if (mag)
				return mag.GetAmmoCount() > 0;

			return wpn.GetInternalMagazineCartridgeCount(mi) > 0;
		}
		return false;
	}

	//! Whether the weapon is currently raised (override of the vanilla flag).
	override bool IsRaised()
	{
		return m_WeaponRaised;
	}

	//! Whether the raise animation has finished (raised for more than 0.5 s).
	override bool IsWeaponRaiseCompleted()
	{
		return m_WeaponRaisedTimer > 0.5;
	}

	//! Gate for RaiseWeapon: blocked while climbing, falling, swimming or on a
	//! ladder (those commands own the body and would override the raised pose).
	bool CanRaiseWeapon()
	{
		if (IsClimbing() || IsFalling() || IsSwimming() || IsClimbingLadder())
			return false;

		return true;
	}

	//! Disable the vanilla client aiming model (mouse-driven) — an AI bot has no
	//! aim input, so it would oscillate the weapon IK / recoil. Aim is driven by
	//! SetAimTarget/GetWeaponAimDirection instead.
	override bool AimingModel(float pDt, SDayZPlayerAimingModel pModel)
	{
		return false;
	}

	//! Tick the raise timer and push the raised flag into the animation graph.
	//! The raise ANIMATION is driven by the custom dmAI_Raised graph variable (bound
	//! in BindLookVars), NOT by a raised stance — the vanilla "Raised" var is
	//! engine-driven from the stance and not settable via AnimSetBool.
	void ApplyWeaponRaise(float pDt)
	{
		BindLookVars();

		if (m_WeaponRaised)
			m_WeaponRaisedTimer += pDt;
		else
			m_WeaponRaisedTimer = 0.0;

		if (m_VarRaised >= 0)
			AnimSetBool(m_VarRaised, m_WeaponRaised);

		#ifdef DM_BOT_DEBUG_FSM
		if (GetGame().GetTickTime() - m_LastReadyLogTime >= 2.0)
		{
			m_LastReadyLogTime = GetGame().GetTickTime();
			dmBotLog.Debug("[Aim] raised=" + m_WeaponRaised + " mode=" + m_AimMode + " readyDur=" + m_RaiseReadyDuration);
			dmBotLog.Debug("[Aim] timer=" + m_WeaponRaisedTimer + " raising=" + IsRaising() + " ready=" + IsReadyToShoot());
		}
		#endif
	}

	//! Convert a world-space direction into the relative aim angles (left/right,
	//! up/down), the exact inverse of GetWeaponAimDirection(). Used by SetAimTarget
	//! and by dmBotIntent_Aim to push dmAiming's dispersed shot direction into the
	//! fire path.
	void SetAimDirection(vector worldDir)
	{
		if (worldDir.Length() < 0.01)
		{
			m_AimWorldDirection = vector.Zero;
			m_AimRelAngleLR = 0.0;
			m_AimRelAngleUD = 0.0;
			return;
		}

		worldDir.Normalize();
		m_AimWorldDirection = worldDir;

		vector angles = worldDir.VectorToAngles();
		float bodyYaw = GetOrientation()[0];
		m_AimRelAngleLR = dmAISurvivor.AngleDiff(angles[0], bodyYaw);

		float pitch = angles[1];
		if (pitch > 180.0)
			pitch -= 360.0;
		m_AimRelAngleUD = pitch;
	}

	//! Compute and store the relative aim angles (left/right, up/down) toward the
	//! target. The barrel direction is eyePos (neck) -> aimPos (target chest/head);
	//! yaw/pitch come from VectorToAngles (same convention as LookAtPoint).
	void SetAimTarget(EntityAI target)
	{
		if (!target)
		{
			m_AimRelAngleLR = 0.0;
			m_AimRelAngleUD = 0.0;
			return;
		}

		//! Aim point: center mass for humans (Spine3, like the melee code), head
		//! for creatures (no Spine3); fallback to the feet + eye height. Bone
		//! lookup lives on Human/DayZCreature, not EntityAI.
		vector aimPos = target.GetPosition() + Vector(0, DM_EYE_HEIGHT, 0);
		int bone = -1;
		Human human = Human.Cast(target);
		if (human)
		{
			bone = human.GetBoneIndexByName("Spine3");
			if (bone < 0)
				bone = human.GetBoneIndexByName("Head");
		}
		else
		{
			DayZCreature creature = DayZCreature.Cast(target);
			if (creature)
				bone = creature.GetBoneIndexByName("Head");
		}
		if (bone >= 0)
			aimPos = target.GetBonePositionWS(bone);

		//! Eye position: the neck bone (how the model holds the gun); fallback to
		//! feet + eye height.
		vector eyePos = GetPosition() + Vector(0, DM_EYE_HEIGHT, 0);
		int neckBone = GetBoneIndexByName("Neck");
		if (neckBone >= 0)
			eyePos = GetBonePositionWS(neckBone);

		vector aimDir = aimPos - eyePos;

		#ifdef DM_BOT_DEBUG_BALLISTICS
		dmBotLog.Debug("[Ballistics] AIM bone=" + bone + " aimPos=" + aimPos);
		dmBotLog.Debug("[Ballistics] AIM eyePos=" + eyePos + " aimDir=" + aimDir);
		#endif

		SetAimDirection(aimDir);
	}

	//! World-space barrel direction from the relative aim angles. Used by the
	//! Fire() native (A4). Reconstructs the absolute yaw/pitch and converts back
	//! with AnglesToVector (exact inverse of the VectorToAngles used in SetAimTarget).
	vector GetWeaponAimDirection()
	{
		float bodyYaw = GetOrientation()[0];
		vector angles = Vector(bodyYaw + m_AimRelAngleLR, m_AimRelAngleUD, 0.0);
		return angles.AnglesToVector();
	}

	//! Current world-space aim direction (from dmAiming). Stored directly, not
	//! reconstructed from body yaw + relative angle (the body may have turned).
	vector GetAimWorldDirection()
	{
		return m_AimWorldDirection;
	}

	//! The shooting accuracy model (used by the fire path in Phase 3).
	dmAiming GetAiming()
	{
		return m_Aiming;
	}

	//! Bullet spawn point: the neck bone; fallback to the feet + eye height.
	vector GetShotOrigin()
	{
		vector origin = GetPosition() + Vector(0, DM_EYE_HEIGHT, 0);
		int neck = GetBoneIndexByName("Neck");
		if (neck >= 0)
			origin = GetBonePositionWS(neck);
		#ifdef DM_BOT_DEBUG_BALLISTICS
		dmBotLog.Debug("[Ballistics] SHOTORIGIN neck=" + neck + " origin=" + origin);
		#endif
		return origin;
	}

	//! Bullet velocity as a VECTOR. The Fire() native treats its speed argument as
	//! a UNIT DIRECTION — the magnitude comes from CfgAmmo initSpeed (see the
	//! Expansion Fire(mi,pos,dir,dir) reference). GetAmmoInitSpeed is only used in
	//! ComputeBulletTravelTime for the bullet-drop compensation.
	vector ComputeShotVelocity(Weapon_Base weapon, int mi, vector direction)
	{
		return direction;
	}

	//! Full shot computation: spawn point + direction (aim + bullet-drop
	//! compensation) + velocity.
	void ComputeShot(Weapon_Base weapon, int mi, out vector origin, out vector direction, out vector velocity)
	{
		origin = GetShotOrigin();
		direction = GetAimWorldDirection();
		ApplyPersonalDispersion(direction);
		CompensateBulletDrop(weapon, mi, origin, direction);
		ApplyWeaponDispersion(weapon, mi, direction);
		velocity = ComputeShotVelocity(weapon, mi, direction);
	}

	//! Recoil strength (fixed for now; later derived from the weapon config).
	float ComputeRecoilModifier(Weapon_Base weapon)
	{
		return DM_AIM_RECOIL_MODIFIER;
	}

	//! Apply recoil to the aiming model (reusable — call from anywhere).
	void ApplyRecoil(Weapon_Base weapon)
	{
		float pitch = GetAiming().AddRecoil(ComputeRecoilModifier(weapon));
		KickRecoilVisual(pitch);
	}

	//! Compensate bullet drop: raycast along the aim direction -> distance -> flight
	//! time (initSpeed from CfgAmmo) -> drop = 0.5*g*t^2 -> tilt the direction up.
	void CompensateBulletDrop(Weapon_Base weapon, int mi, vector origin, inout vector direction)
	{
		if (weapon.IsChamberEmpty(mi) || weapon.IsChamberFiredOut(mi))
			return;   // нет патрона — дроп-компенсация не нужна
		vector end = origin + direction * DM_AI_SHOT_MAX_DISTANCE;
		vector hitPosition;
		vector hitNormal;
		int contactComponent;
		if (!DayZPhysics.RaycastRV(origin, end, hitPosition, hitNormal, contactComponent, null, null, this, false, false, ObjIntersectView, 0.01))
			return;
		float distance = vector.Distance(origin, hitPosition);
		float travelTime = ComputeBulletTravelTime(weapon, mi, distance);
		float drop = 0.5 * DM_AI_GRAVITY * travelTime * travelTime;
		if (drop > 0.1)
		{
			vector projected = origin + direction * distance;
			projected[1] = projected[1] + drop * 0.8;
			vector newDir = vector.Direction(origin, projected);
			newDir.Normalize();
			direction = newDir;
		}
	}

	//! Bullet flight time to a distance: step-wise integration of speed under air
	//! friction (speed = e^(airFriction·d)·initSpeed, 0.05 s step, max 6 s). At
	//! airFriction = 0 the integration degenerates to distance / initSpeed.
	float ComputeBulletTravelTime(Weapon_Base weapon, int mi, float distance)
	{
		float initSpeed = GetAmmoInitSpeed(weapon, mi);
		if (initSpeed <= 0.0)
			initSpeed = DM_AI_DEFAULT_INIT_SPEED;
		float airFriction = GetAmmoAirFriction(weapon, mi);

		float distanceTraveled = 0.0;
		float timeTraveled = 0.0;
		float simulationStep = 0.05;
		float speed;
		while (distanceTraveled < distance && timeTraveled < 6.0)
		{
			speed = Math.Pow(Math.EULER, airFriction * distanceTraveled) * initSpeed;
			if (speed <= 0.0)
				break;
			timeTraveled = timeTraveled + simulationStep;
			distanceTraveled = distanceTraveled + speed * simulationStep;
		}
		return timeTraveled;
	}

	//! Резолв типа пули патронника в CfgAmmo <bullet>; false если нет.
	bool GetChamberedBulletType(Weapon_Base weapon, int mi, out string bullet)
	{
		if (mi < 0 || mi >= weapon.GetMuzzleCount())
			return false;
		string ammoMag = weapon.GetChamberedCartridgeMagazineTypeName(mi);
		if (ammoMag == "")
			return false;
		return g_Game.ConfigGetText(CFG_MAGAZINESPATH + " " + ammoMag + " ammo", bullet);
	}

	//! initSpeed of the chambered cartridge: CfgMagazines <ammoMagazine> ammo ->
	//! CfgAmmo <bullet> initSpeed.
	float GetAmmoInitSpeed(Weapon_Base weapon, int mi)
	{
		string bullet;
		if (!GetChamberedBulletType(weapon, mi, bullet))
			return 0.0;
		return g_Game.ConfigGetFloat(CFG_AMMO + " " + bullet + " initSpeed");
	}

	//! airFriction of the chambered cartridge: CfgAmmo <bullet> airFriction.
	float GetAmmoAirFriction(Weapon_Base weapon, int mi)
	{
		string bullet;
		if (!GetChamberedBulletType(weapon, mi, bullet))
			return 0.0;
		return g_Game.ConfigGetFloat(CFG_AMMO + " " + bullet + " airFriction");
	}

	//! Push the aim angles into the graph. Runs before super. The values written
	//! here are smoothed toward m_AimRelAngleLR/UD so the barrel doesn't jitter;
	//! the raw m_AimRelAngleLR/UD stay authoritative for GetWeaponAimDirection()
	//! (the actual shot direction). ADS is toggled separately after super.
	void ApplyWeaponAim()
	{
		BindLookVars();

		m_AimSmoothedLR = m_AimSmoothedLR * 0.7 + m_AimRelAngleLR * 0.3;
		m_AimSmoothedUD = m_AimSmoothedUD * 0.7 + m_AimRelAngleUD * 0.3;

		if (m_VarAimX >= 0)
			AnimSetFloat(m_VarAimX, m_AimSmoothedLR);
		if (m_VarAimY >= 0)
			AnimSetFloat(m_VarAimY, m_AimSmoothedUD);
		if (m_VarAimIKX >= 0)
			AnimSetFloat(m_VarAimIKX, m_AimSmoothedLR);

		#ifdef DM_BOT_DEBUG_PAWN
		if (GetGame().GetTickTime() - m_LastAimLogTime >= 2.0)
		{
			m_LastAimLogTime = GetGame().GetTickTime();
			dmBotLog.Debug("[Aim] relLR=" + m_AimRelAngleLR + " relUD=" + m_AimRelAngleUD + " raised=" + m_WeaponRaised);
			dmBotLog.Debug("[Aim] smoothLR=" + m_AimSmoothedLR + " smoothUD=" + m_AimSmoothedUD);
		}
		#endif
	}

	//! Apply the current aiming mode (hip-fire / ADS). Runs AFTER super.CommandHandler
	//! so the vanilla HandleWeapons/ExitSights (executed inside super) can't overwrite
	//! it back every frame. HIP and a lowered weapon both drop ADS and exit any active
	//! optic; ADS enters ironsights — which picks up the iron sight or the attached
	//! optic depending on the weapon.
	void ApplyWeaponADS()
	{
		HumanCommandWeapons hcw = GetCommandModifier_Weapons();
		Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
		ItemOptics optic = null;
		if (weapon)
			optic = weapon.GetAttachedOptics();

		#ifdef DM_BOT_DEBUG_PAWN
		if (GetGame().GetTickTime() - m_LastADSLogTime >= 2.0)
		{
			m_LastADSLogTime = GetGame().GetTickTime();
			dmBotLog.Debug("[ADS] raised=" + m_WeaponRaised + " mode=" + m_AimMode + " hasOptic=" + (optic != null));
		}
		#endif

		if (!m_WeaponRaised || m_AimMode == dmBotAimMode.HIP)
		{
			if (hcw)
				hcw.SetADS(false);
			if (optic && optic.IsInOptics())
				SwitchOptics(optic, false);
			return;
		}

		//! ADS: enter ironsights/optic. Replicate SetIronsights(true) but WITHOUT
		//! hic.ResetFreeLookToggle() — the AI input controller prints "not
		//! implemented" on every call (console spam).
		if (weapon)
			weapon.SetWasIronSight(m_CameraIronsight);
		m_CameraIronsight = true;
		if (hcw)
			hcw.SetADS(true);
	}

//! Called on the client whenever the synced variables arrive from the server.
#ifndef SERVER
	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();

		if (Math.AbsFloat(m_LookYawDeg - m_LastLogLookYaw) > 0.5 || Math.AbsFloat(m_LookPitchDeg - m_LastLogLookPitch) > 0.5)
		{
			m_LastLogLookYaw = m_LookYawDeg;
			m_LastLogLookPitch = m_LookPitchDeg;

			#ifdef DM_BOT_DEBUG_PAWN
			dmBotLog.Debug("dmAISurvivorBase.OnVariablesSynchronized() lookYaw=" + m_LookYawDeg + " lookPitch=" + m_LookPitchDeg + " instType=" + GetInstanceType());
			#endif
		}
	}
#endif

	//! Replace the vanilla weapon handling (sights/fire/optics driven by player
	//! input) — an AI bot drives raise/aim/ADS/fire itself (ApplyWeaponRaise/
	//! ApplyWeaponAim/ApplyWeaponADS/TryFireWeapon). We only forward the weapon
	//! events (reload/jam/…) so they still process inside the CommandHandler.
	override void HandleWeapons(float pDt, Entity pInHands, HumanInputController pInputs, out bool pExitIronSights)
	{
		GetDayZPlayerInventory().HandleWeaponEvents(pDt, pExitIronSights);
	}

	//! The AI drives ADS itself (ApplyWeaponADS) — skip the vanilla input-driven
	//! ADS handling, which would call ExitSights()->SetADS(false) and fight ours.
	override void HandleADS()
	{
	}

	//! The AI doesn't use standalone handheld optics — skip the vanilla optic
	//! handling (mirrors Expansion eAIBase.HandleOptic).
	override void HandleOptic(notnull ItemOptics optic, bool inHands, HumanInputController pInputs, out bool pExitOptics)
	{
	}

	//! Called every tick during the deterministic simulation (the CommandHandler).
	//! Look vars are set before super; the turn commands are set AFTER super
	//! (matching how the Expansion AI calls its movement PreAnimUpdate after super),
	//! so the vanilla command processing doesn't consume/overwrite them.
	//! All body actuation is gated on CanAct() so the vanilla body state (death,
	//! unconsciousness, restraint) is respected instead of overridden.
	override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("CommandHandler");
		#endif

		if (IsAlive())
			TickBodySystems(pDt);

		bool canAct = CanAct();

		if (canAct)
		{
			ApplyLookVars();
			ApplyWeaponRaise(pDt);
			ApplyWeaponAim();
		}

		super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);

		//! Transition to/from unconscious AFTER super: the vanilla command logic
		//! (melee fight etc.) reads HumanCommandMove inside super, so starting the
		//! unconscious command before it would null hcm and throw a VM exception.
		if (IsAlive())
			UpdateUnconsciousBridge();

		if (!canAct)
		{
			ResetActuation();
			return;
		}

		if ( TickVehicle() ) return;
			
		ApplyWeaponADS();
		ApplyBodyTurn(pDt);
		ApplyMovement(pDt);
		ApplyStance(pDt);

		if (m_FireCooldown > 0.0)
			m_FireCooldown -= pDt;
		TryFireWeapon();

		if (Math.AbsFloat(m_LookYawDeg - m_LastLogLookYaw) > 0.5 || Math.AbsFloat(m_LookPitchDeg - m_LastLogLookPitch) > 0.5)
		{
			m_LastLogLookYaw = m_LookYawDeg;
			m_LastLogLookPitch = m_LookPitchDeg;

			#ifdef DM_BOT_DEBUG_PAWN
			dmBotLog.Debug("dmAISurvivorBase.CommandHandler() lookYaw=" + m_LookYawDeg + " lookPitch=" + m_LookPitchDeg + " instType=" + GetInstanceType());
			#endif
		}
	}

	//! Vanilla ticks body systems in OnScheduledTick, gated on IsPlayerSelected()
	//! and m_AllowModifierTick (enabled only in OnSelectPlayer) — neither happens
	//! for an AI bot. Enable and tick them at a reduced rate so shock refill,
	//! broken legs, bleeding (blood loss) etc. actually run. The systems throttle
	//! internally, so a 4 Hz tick is enough.
	void TickBodySystems(float pDt)
	{
		ModifiersManager mngr = GetModifiersManager();
		if (!mngr)
			return;

		mngr.SetModifiers(true);

		#ifdef DM_BOT_DEBUG_BODY
		m_BodyDebugAccum += pDt;
		if (m_BodyDebugAccum >= 2.0)
		{
			m_BodyDebugAccum = 0.0;
			dmBotLog.Debug("Body: blood=" + GetHealth("", "Blood") + " shock=" + GetHealth("", "Shock") + " bleeding=" + IsBleeding() + " unconscious=" + IsUnconscious() + " brokenLegs=" + (GetBrokenLegs() != eBrokenLegs.NO_BROKEN_LEGS));
		}
		#endif

		m_ModifierTickAccum += pDt;
		if (m_ModifierTickAccum < DM_BOT_MODIFIER_TICK_INTERVAL)
			return;

		float dt = m_ModifierTickAccum;
		m_ModifierTickAccum = 0.0;

		mngr.OnScheduledTick(dt);

		BleedingSourcesManagerServer bsm = GetBleedingManagerServer();
		if (bsm)
			bsm.OnTick(dt);
	}

	//! Bridge the vanilla unconscious command from the shock value. The vanilla
	//! UnconsciousnessMdfr signals it via a server<->client sync juncture, and the
	//! command itself is normally started in a CommandHandler block gated on
	//! m_ActionManager — null for an AI_SERVER bot. So start/stop the command
	//! directly from the shock value.
	void UpdateUnconsciousBridge()
	{
		float shock = GetHealth("", "Shock");

		if (!IsUnconscious() && shock <= PlayerConstants.UNCONSCIOUS_THRESHOLD)
		{
			if (!m_ShouldBeUnconscious)
			{
				m_ShouldBeUnconscious = true;
				StartCommand_Unconscious(0);

				#ifdef DM_BOT_DEBUG_BODY
				dmBotLog.Debug("UnconsciousBridge: shock=" + shock + " -> StartCommand_Unconscious(0)");
				#endif
			}
		}
		else if (IsUnconscious() && shock >= PlayerConstants.CONSCIOUS_THRESHOLD)
		{
			m_ShouldBeUnconscious = false;
			HumanCommandUnconscious hcu = GetCommand_Unconscious();
			if (hcu)
			{
				hcu.WakeUp(DayZPlayerConstants.STANCEIDX_PRONE);

				#ifdef DM_BOT_DEBUG_BODY
				dmBotLog.Debug("UnconsciousBridge: shock=" + shock + " -> WakeUp");
				#endif
			}
		}
	}

	//! Taking damage: register the source as a maximum threat immediately, even if
	//! it is outside the vision FOV (attacking from behind). This drives the bot's
	//! reactive defence before perception would ever notice the attacker.
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

		#ifdef DM_BOT_DEBUG_BALLISTICS
		dmBotLog.Debug("[Ballistics] HIT time=" + GetGame().GetTime() + " ammo=" + ammo);
		dmBotLog.Debug("[Ballistics] HIT zone=" + dmgZone + " health=" + damageResult.GetDamage(dmgZone, "Health"));
		#endif

		//! Halve the shock dealt by zombies: vanilla already applied full shock,
		//! add back half so the bot isn't knocked out as easily.
		if (ZombieBase.Cast(source))
		{
			float shock = damageResult.GetDamage("", "Shock");
			if (shock > 0.0)
				AddHealth("", "Shock", shock * 0.5);
		}

		if (source)
		{
			dmAISurvivor bot = dmAISurvivor.Find(this);
			if (bot)
				bot.RegisterDamageThreat(source, damageResult.GetHighestDamage("Health"));
		}
	}

	//! Death: run the vanilla PlayerBase.EEKilled chain. Its GetHive().
	//! CharacterKill() is skipped for AI bots because the modded
	//! DayZPlayerImplement.GetHive() returns null for INSTANCETYPE_AI_SERVER (no
	//! character id), so no "Can't kill player with id -1" spam. The vanilla flow
	//! does the death cleanup + corpse registration + SendDeathJuncture.
	override void EEKilled(Object killer)
	{
		super.EEKilled(killer);

		#ifdef DM_BOT_DEBUG_BODY
		dmBotLog.Debug("EEKilled: hasCEProfile=" + (GetEconomyProfile() != null) + " corpseProcessing=" + m_CorpseProcessing + " corpseState=" + m_CorpseState + " lifetime=" + GetLifetime());
		#endif
	}

	//! No-op: don't activate MDF_AREAEXPOSURE on entering a contaminated (gas)
	//! zone — the vanilla AreaExposureMdfr.OnActivate runs TeleportCheck and
	//! teleports the AI out of the zone ("Персонаж перемещён из опасной зоны").
	//! TODO: properly avoid gas zones in navigation (Expansion
	//! s_Expansion_DangerousAreas + FindClosestPointOutsideCluster).
	override void OnContaminatedAreaEnterServer()
	{
	}

	//! Whether the body may actuate (move/turn/stance/look). False while dead,
	//! unconscious or restrained — the vanilla body state must win.
	bool CanAct()
	{
		return IsAlive() && !IsUnconscious() && !IsRestrained();
	}

	//! Stop any in-progress body actuation and drop the head to neutral. Called
	//! every frame while CanAct() is false, so a foot-step turn or a movement
	//! speed can't resume mid-incapacitation.
	void ResetActuation()
	{
		if (m_TurnState != 0)
		{
			if (m_CmdStopTurn >= 0)
				AnimCallCommand(m_CmdStopTurn, 0, 0.0);
			if (m_VarTurnAmount >= 0)
				AnimSetFloat(m_VarTurnAmount, 0.0);
		}
		m_TurnState = 0;
		m_TurnTime = 0.0;
		m_TurnSharp = false;
		m_ActualSpeed = 0.0;
		m_FireRequest = false;

		if (m_VarLook >= 0)
			AnimSetBool(m_VarLook, false);
	}

	//! Disable the vanilla body-turn (HeadingModel::RotateOrient) for the MOVE
	//! command: it rotates the body toward the movement direction and overrides our
	//! SetOrientation. We control the body orientation manually in ApplyBodyTurn.
	//! Mirrors Expansion eAIBase.HeadingModel.
	override bool HeadingModel(float pDt, SDayZPlayerHeadingModel pModel)
	{
		GetMovementState(m_MovementState);
		if (m_MovementState.m_CommandTypeId == DayZPlayerConstants.COMMANDID_MOVE)
		{
			m_fLastHeadingDiff = 0;
			float angle = GetOrientation()[0] * Math.DEG2RAD;
			pModel.m_fHeadingAngle = angle;
			pModel.m_fOrientationAngle = angle;
			return true;
		}
		return super.HeadingModel(pDt, pModel);
	}

	//! Signed angle difference, normalized to (-180, 180].
	static float AngleDiff(float a, float b)
	{
		float d = a - b;
		while (d > 180.0) d -= 360.0;
		while (d < -180.0) d += 360.0;
		return d;
	}

	//! Rotate the body toward m_TargetBodyYaw. Two modes, mirroring Expansion AI
	//! (eAICommandMove):
	//!   - while moving  -> slide-turn via SetOrientation (limited rate), no foot-step;
	//!   - while idle    -> native foot-stepping "Turn" state (root motion).
	//! Runs inside the CommandHandler after super.
	void ApplyBodyTurn(float pDt)
	{
		float bodyYaw = GetOrientation()[0];
		float dBody = AngleDiff(m_TargetBodyYaw, bodyYaw);

		bool moving = m_IsMoving;

		//! Slide-turn (SetOrientation) while moving (vanilla HeadingModel disabled,
		//! so SetOrientation is authoritative).
		if (moving)
		{
			//! Sharp turn while moving -> slow down (consumed by ApplyMovement).
			m_TurnSharp = Math.AbsFloat(dBody) > DM_MOVE_TURN_SLOW_THRESHOLD;

			//! Cancel any in-progress foot-step turn; slide instead.
			if (m_TurnState != 0)
			{
				if (m_CmdStopTurn >= 0)
					AnimCallCommand(m_CmdStopTurn, 0, 0.0);
				if (m_VarTurnAmount >= 0)
					AnimSetFloat(m_VarTurnAmount, 0.0);
				m_TurnState = 0;
			}

			if (Math.AbsFloat(dBody) > 1.0)
			{
				float maxStep = DM_MOVE_TURN_RATE * pDt * DM_MOVE_TURN_SPEED;
				float step = dBody * DM_MOVE_TURN_RESPONSE;
				step = Math.Clamp(step, -maxStep, maxStep);
				SetOrientation(Vector(bodyYaw + step, 0.0, 0.0));
			}
			return;
		}

		m_TurnSharp = false;

		//! Idle: native foot-stepping turn (root motion) for ALL stances — the
		//! graph's TurnStanceSTM picks the stance-specific turn (erect step, crouch
		//! step, prone roll).
		if (m_TurnState == 0)
		{
			if (Math.AbsFloat(dBody) > 1.0)
			{
				if (m_VarTurnAmount >= 0)
					AnimSetFloat(m_VarTurnAmount, Math.Clamp(dBody / 90.0, -2.0, 2.0));
				if (m_CmdTurn >= 0)
					AnimCallCommand(m_CmdTurn, 0, 0.0);
				m_TurnTime = 0.0;
				m_TurnState = 1;
			}
		}
		else
		{
			m_TurnTime += pDt;
			if (m_TurnTime > 2.0 || Math.AbsFloat(dBody) < 1.0)
			{
				if (m_CmdStopTurn >= 0)
					AnimCallCommand(m_CmdStopTurn, 0, 0.0);
				if (m_VarTurnAmount >= 0)
					AnimSetFloat(m_VarTurnAmount, 0.0);
				m_TurnState = 0;
			}
		}
	}

	//! Apply the desired movement (direction + speed) via ONE_FRAME overrides.
	//! The actual speed is ramped toward the desired speed (smooth acceleration/
	//! deceleration); while turning sharply it is capped to DM_MOVE_TURN_SLOW_SPEED.
	//! The vanilla sprint limit (hic.LimitsDisableSprint) is bypassed by our
	//! OverrideMovementSpeed, so enforce the same condition here: no sprint when
	//! the body can't consume sprint stamina or can't sprint (broken legs etc.).
	//! The broken-legs walk shock (BrokenLegWalkShock) is dealt by the vanilla
	//! modifier at jog speed, so keep jog allowed here to let that chain run.
	//! ONE_FRAME auto-disables after this CommandHandler, so a stale override can't
	//! accumulate if the brain stops writing the desired state.
	void ApplyMovement(float pDt)
	{
		float target = m_DesiredSpeed;
		if (m_TurnSharp)
			target = Math.Min(target, DM_MOVE_TURN_SLOW_SPEED);
		if (target > DM_SPEED_IDX_JOG && !(CanConsumeStamina(EStaminaConsumers.SPRINT) && CanSprint()))
			target = DM_SPEED_IDX_JOG;

		float maxStep = DM_MOVE_ACCEL_RATE * pDt;
		m_ActualSpeed += Math.Clamp(target - m_ActualSpeed, -maxStep, maxStep);

		HumanInputController hic = GetInputController();
		hic.OverrideMovementAngle(HumanInputControllerOverrideType.ONE_FRAME, m_DesiredMoveAngle);
		hic.OverrideMovementSpeed(HumanInputControllerOverrideType.ONE_FRAME, m_ActualSpeed);

		#ifdef DM_BOT_DEBUG_PERFRAME_MOVING_LOG
		if (m_DesiredSpeed > 0.0)
		{
			HumanCommandMove move = GetCommand_Move();
			float vanillaSpeed = 0.0;
			if (move)
				vanillaSpeed = move.GetCurrentMovementSpeed();
			bool canSprintNow = CanSprint();
			bool canConsumeSprint = CanConsumeStamina(EStaminaConsumers.SPRINT);

			vector curPos = GetPosition();
			float posDelta = 0.0;
			if (m_LastLogPosValid)
			{
				vector d = curPos - m_LastLogPos;
				d[1] = 0.0;
				posDelta = d.Length();
			}
			m_LastLogPos = curPos;
			m_LastLogPosValid = true;

			float bodyYaw = GetOrientation()[0];

			dmBotLog.Debug("[MOV] desired=" + m_DesiredSpeed + " target=" + target + " actual=" + m_ActualSpeed + " vanilla=" + vanillaSpeed);
			dmBotLog.Debug("[MOV] turnSharp=" + m_TurnSharp + " canSprint=" + canSprintNow + " stamina=" + canConsumeSprint + " angle=" + m_DesiredMoveAngle);
			dmBotLog.Debug("[MOV] bodyYaw=" + bodyYaw + " targetYaw=" + m_TargetBodyYaw + " posDelta=" + posDelta);
		}
		#endif

		#ifdef DM_BOT_DEBUG_BODY
		if (m_DesiredSpeed > 0.0)
		{
			m_MoveDebugAccum += pDt;
			if (m_MoveDebugAccum >= 2.0)
			{
				m_MoveDebugAccum = 0.0;
				dmBotLog.Debug("Move: hic=" + (hic != null) + " angle=" + m_DesiredMoveAngle + " desired=" + m_DesiredSpeed + " actual=" + m_ActualSpeed + " turnSharp=" + m_TurnSharp + " alive=" + IsAlive() + " unconscious=" + IsUnconscious() + " restrained=" + IsRestrained());
			}
		}
		#endif
	}

	//! Apply the desired stance, stepping through crouch for erect<->prone.
	void ApplyStance(float pDt)
	{
		HumanCommandMove move = GetCommand_Move();
		if (!move)
			return;

		GetMovementState(m_MovementState);
		int current = m_MovementState.m_iStanceIdx;
		if (current >= DayZPlayerConstants.STANCEIDX_RAISED)
			current -= DayZPlayerConstants.STANCEIDX_RAISED;

		if (m_DesiredStance == current)
		{
			m_StanceTimeout = 0.0;
			return;
		}

		if (m_StanceTimeout > 0.0)
		{
			m_StanceTimeout -= pDt;
			return;
		}

		//! erect<->prone can't be done directly; step through crouch.
		int next = m_DesiredStance;
		if (current == DayZPlayerConstants.STANCEIDX_ERECT && m_DesiredStance == DayZPlayerConstants.STANCEIDX_PRONE)
			next = DayZPlayerConstants.STANCEIDX_CROUCH;
		else if (current == DayZPlayerConstants.STANCEIDX_PRONE && m_DesiredStance == DayZPlayerConstants.STANCEIDX_ERECT)
			next = DayZPlayerConstants.STANCEIDX_CROUCH;

		move.ForceStance(next);

		if (next == DayZPlayerConstants.STANCEIDX_PRONE || current == DayZPlayerConstants.STANCEIDX_PRONE)
			m_StanceTimeout = DM_STANCE_TIMEOUT_PRONE;
		else
			m_StanceTimeout = DM_STANCE_TIMEOUT_CROUCH;
	}

	//! Set the desired body yaw (world, degrees). Called by the controller.
	void SetTargetBodyYaw(float yaw)
	{
		m_TargetBodyYaw = yaw;
	}

	//! Set the desired movement (direction relative to body + speed). Called by
	//! the brain; applied by ApplyMovement in the CommandHandler.
	void SetMove(float angle, float speed)
	{
		if (Math.AbsFloat(angle) < DM_MOVE_ANGLE_DEADZONE)
			angle = 0.0;
		m_DesiredMoveAngle = angle;
		m_DesiredSpeed = speed;
		m_IsMoving = speed > 0.0;
	}

	//! Set the desired stance (STANCEIDX_*). Applied by ApplyStance.
	void SetStance(int stanceIdx)
	{
		m_DesiredStance = stanceIdx;
	}

	//! Set the head look horizontal offset (degrees, relative to the body facing).
	//! Called on the server by the controller. The value is synced to the client
	//! (RegisterNetSyncVariableFloat) so the client's CommandHandler can apply it.
	void SetLookYaw(float offsetDeg)
	{
		offsetDeg = Math.Clamp(offsetDeg, -DM_LOOK_MAX_YAW, DM_LOOK_MAX_YAW);
		if (m_LookYawDeg != offsetDeg)
		{
			m_LookYawDeg = offsetDeg;
			SetSynchDirty();
		}
	}

	float GetLookYaw()
	{
		return m_LookYawDeg;
	}

	//! Set the head look vertical offset (degrees).
	//! Called on the server by the controller, synced to the client.
	void SetLookPitch(float pitchDeg)
	{
		pitchDeg = Math.Clamp(pitchDeg, -DM_LOOK_MAX_PITCH, DM_LOOK_MAX_PITCH);
		if (m_LookPitchDeg != pitchDeg)
		{
			m_LookPitchDeg = pitchDeg;
			SetSynchDirty();
		}
	}

	//! Instantly kick the smoothed barrel aim up (visual recoil), bypassing the
	//! per-tick blend so the recoil is a sharp kick, not a slow sway.
	void KickRecoilVisual(float pitchDeg)
	{
		m_AimSmoothedUD = m_AimSmoothedUD + pitchDeg;
	}

	float GetLookPitch()
	{
		return m_LookPitchDeg;
	}

	//! Ask the fight logic to perform one melee strike against the given target.
	//! The request is consumed by dmBotMeleeFightLogic_LightHeavy.HandleFightLogic.
	void RequestMeleeAttack(EntityAI target)
	{
		m_MeleeAttackRequest = true;
		m_MeleeTarget = target;
	}

	bool HasMeleeAttackRequest()
	{
		return m_MeleeAttackRequest;
	}

	EntityAI GetMeleeAttackTarget()
	{
		return m_MeleeTarget;
	}

	//! Play a gesture/emote animation by EmoteConstants ID. Gated by the vanilla
	//! CanPlayEmote (alive, not climbing/fighting/swimming, etc.). Returns false
	//! when the emote can't start. Completion is detected by the Emote intent via
	//! GetCommand_Action()/GetCommandModifier_Action() == null (NOT IsEmotePlaying,
	//! which never clears for a server AI — EmoteManager.Update is gated by
	//! IsPlayerSelected).
	bool PlayEmote(int emoteID)
	{
		EmoteManager em = GetEmoteManager();
		if (!em || !em.CanPlayEmote(emoteID))
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Bot] PlayEmote: refuse id=" + emoteID);
			#endif
			return false;
		}
		em.PlayEmote(emoteID);
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Bot] PlayEmote: start id=" + emoteID);
		#endif
		return true;
	}
	
	vector m_VehicleSitPos;
	vector m_VehicleSitDir;
	bool m_VehiclePendingEnter;
	bool m_VehiclePendingExit;

	//! Enter a vehicle at the given crew seat. Starts the vanilla vehicle command
	//! (get-in animation). Returns false when the command can't start.
	bool GetInVehicle(Transport transport, int seatIndex)
	{
		if (!transport) return false;

		int seatAnim = transport.GetSeatAnimationType(seatIndex);
		HumanCommandVehicle cmd = StartCommand_Vehicle(transport, seatIndex, seatAnim);
		if (!cmd)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Bot] GetInVehicle: fail seat=" + seatIndex);
			#endif
			return false;
		}
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Bot] GetInVehicle: seat=" + seatIndex);
		#endif
		cmd.SetVehicleType(transport.GetAnimInstance());
		m_VehiclePendingEnter = true;
		return true;
	}

	//! Exit the vehicle the bot is seated in. Starts the vanilla get-out animation.
	//! Returns false when there is no active vehicle command (not in a vehicle).
	bool GetOutVehicle()
	{
		HumanCommandVehicle cmd = GetCommand_Vehicle();
		if (!cmd)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Bot] GetOutVehicle: not in vehicle");
			#endif
			return false;
		}
		Transport transport = cmd.GetTransport();
		if (transport)
			transport.CrewEntryWS(cmd.GetVehicleSeat(), m_VehicleSitPos, m_VehicleSitDir);

		cmd.KeepInVehicleSpaceAfterLeave(false);
		cmd.GetOutVehicle();
		m_VehiclePendingExit = true;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Bot] GetOutVehicle: start");
		#endif
		return true;
	}

	bool TickVehicle()
	{
		if (m_VehiclePendingExit)
		{
			if (GetCommand_Vehicle()) return true;

			SetPosition(m_VehicleSitPos);
			SetOrientation(Vector(m_VehicleSitDir.VectorToAngles()[0], 0.0, 0.0));
			m_VehiclePendingExit = false;
			return true;
		}

		if ( m_VehiclePendingEnter )
		{
			if ( GetCommand_Vehicle())
				m_VehiclePendingEnter = GetCommand_Vehicle().IsGettingIn();
			return true;
		}

		return false;
	}

	void ConsumeMeleeAttackRequest()
	{
		m_MeleeAttackRequest = false;
		m_MeleeTarget = null;
	}

	//! Ask for a single shot. With a target, aim is computed immediately
	//! (SetAimTarget); with null, fire along the direction already set via
	//! SetAimDirection (used by dmBotIntent_Aim). The shot itself fires next
	//! CommandHandler in TryFireWeapon.
	void RequestFire(EntityAI target = null)
	{
		if (target)
			SetAimTarget(target);
		m_FireRequest = true;
	}

	bool HasFireRequest()
	{
		return m_FireRequest;
	}

	void ConsumeFireRequest()
	{
		m_FireRequest = false;
	}

	//! Fire the weapon if a request is pending and the body is ready. LOS/distance
	//! are gated by the Shooting state (B1), not here; here we only check that the
	//! weapon can physically fire and the vanilla weapon-FSM is idle. Runs after
	//! super.CommandHandler so the weapon-FSM processed by super is settled.
	void TryFireWeapon()
	{
		if (!m_FireRequest)
			return;

		Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
		if (!weapon || !IsRaised() || !IsWeaponRaiseCompleted() || !weapon.CanFire())
		{
			ConsumeFireRequest();
			return;
		}

		WeaponManager wm = GetWeaponManager();
		if (!wm || wm.IsRunning())
		{
			ConsumeFireRequest();
			return;
		}

		#ifdef DM_WEAPON_DEBUG_FSM
		dmBotLog.Debug("[Weapon] TryFireWeapon: weapon=" + weapon + " raised=" + IsRaised() + " raiseDone=" + IsWeaponRaiseCompleted());
		dmBotLog.Debug("[Weapon] TryFireWeapon: canFire=" + weapon.CanFire() + " wmRunning=" + wm.IsRunning());
		dmBotLog.Debug("[Weapon] TryFireWeapon: unconscious=" + IsUnconscious() + " alive=" + IsAlive());
		#endif
		wm.Fire(weapon);
		m_LastFireTime = GetGame().GetTickTime();
		ConsumeFireRequest();
		m_FireCooldown = DM_BOT_FIRE_COOLDOWN;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Bot] TryFireWeapon: fired weapon=" + weapon);
		#endif
	}

	//! Доступные режимы огня оружия (кэш dmWeaponFireInfo).
	ref array<ref dmFireMode> GetAvailableFireModes(Weapon_Base weapon)
	{
		return dmWeaponFireInfo.Get(weapon).m_Modes;
	}

	//! Предпочтительный режим по дистанции. Пороги: Double <30м; Auto <30м;
	//! Burst <75м; иначе Single.
	dmFireMode GetPreferredFireModeByDistance(float distance, array<ref dmFireMode> modes)
	{
		dmFireMode m;
		if (distance < 30.0)
		{
			m = FindMode(modes, dmFireModeType.DOUBLE);
			if (m)
				return m;
			m = FindMode(modes, dmFireModeType.AUTO);
			if (m)
				return m;
			m = FindMode(modes, dmFireModeType.BURST);
			if (m)
				return m;
		}
		else if (distance < 75.0)
		{
			m = FindMode(modes, dmFireModeType.BURST);
			if (m)
				return m;
		}
		m = FindMode(modes, dmFireModeType.SINGLE);
		if (m)
			return m;
		if (modes.Count() > 0)
			return modes[0];
		return null;
	}

	//! Установить режим на оружии (SetCurrentMode по индексу).
	void SetFireMode(Weapon_Base weapon, dmFireMode mode)
	{
		if (weapon && mode)
			weapon.SetCurrentMode(weapon.GetCurrentMuzzle(), mode.m_Index);
	}

	//! Найти режим заданного типа; null если нет.
	dmFireMode FindMode(array<ref dmFireMode> modes, int type)
	{
		int i;
		for (i = 0; i < modes.Count(); i++)
		{
			if (modes[i].m_Type == type)
				return modes[i];
		}
		return null;
	}

	//! Время (сек) с последнего выстрела.
	float GetTimeSinceLastShot()
	{
		return GetGame().GetTickTime() - m_LastFireTime;
	}

	//! Текущий режим огня оружия (dmFireMode по индексу GetCurrentMode).
	dmFireMode GetCurrentFireMode(Weapon_Base weapon)
	{
		int mi = weapon.GetCurrentMuzzle();
		int modeIndex = weapon.GetCurrentMode(mi);
		ref array<ref dmFireMode> modes = GetAvailableFireModes(weapon);
		if (modeIndex >= 0 && modeIndex < modes.Count())
			return modes[modeIndex];
		return null;
	}

	//! Число выстрелов в серии для текущего режима: AUTO=random 3..12, BURST=m_Burst,
	//! SINGLE/DOUBLE=1 (Double: оба ствола стреляет FSM WeaponFireMultiMuzzle за один вызов).
	int ComputeQueuedShots(Weapon_Base weapon)
	{
		dmFireMode mode = GetCurrentFireMode(weapon);
		if (!mode)
			return 1;
		if (mode.m_Type == dmFireModeType.AUTO)
			return Math.RandomIntInclusive(3, 12);
		if (mode.m_Type == dmFireModeType.BURST)
			return mode.m_Burst;
		return 1;
	}

	//! Предпочтительный режим для текущего оружия и цели (через мозг dmAISurvivor.Find).
	dmFireMode GetPreferredFireMode()
	{
		Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
		if (!weapon)
			return null;
		ref array<ref dmFireMode> modes = GetAvailableFireModes(weapon);
		float distance = DM_AI_SHOT_MAX_DISTANCE;
		dmAISurvivor brain = dmAISurvivor.Find(this);
		if (brain)
		{
			dmTarget t = brain.GetHostileTarget();
			if (t && t.m_Entity)
			{
				vector botPos = GetPosition();
				vector tPos = t.m_Entity.GetPosition();
				distance = vector.Distance(botPos, tPos);
			}
		}
		return GetPreferredFireModeByDistance(distance, modes);
	}

	//! Перевыставить предпочтительный режим на оружии (если отличается от текущего).
	void RefreshPreferredFireMode()
	{
		Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
		if (!weapon)
			return;
		dmFireMode preferred = GetPreferredFireMode();
		if (!preferred)
			return;
		if (weapon.GetCurrentMode(weapon.GetCurrentMuzzle()) != preferred.m_Index)
			SetFireMode(weapon, preferred);
	}

	//! Личный разброс стрелка (dmAiming) — случайный доворот направления на выстрел.
	void ApplyPersonalDispersion(inout vector direction)
	{
		if (m_PerfectAim)
			return;
		if (!m_Aiming)
			return;
		float angLR;
		float angUD;
		if (!m_Aiming.GetShotDispersion(angLR, angUD))
			return;
		vector angles = direction.VectorToAngles();
		angles[0] = angles[0] + angLR * Math.RAD2DEG;
		angles[1] = angles[1] + angUD * Math.RAD2DEG;
		direction = angles.AnglesToVector();
		direction.Normalize();
	}

	//! Оружейный разброс: случайный доворот направления в конусе полуугла dispersion
	//! (rad). Кладётся СВЕРХУ личного разброса dmAiming.
	void ApplyWeaponDispersion(Weapon_Base weapon, int mi, inout vector direction)
	{
		if (m_PerfectAim)
			return;
		dmFireMode mode = GetCurrentFireMode(weapon);
		if (!mode || mode.m_Dispersion <= 0.0)
			return;
		//! VectorToAngles/AnglesToVector работают в градусах, конфиг dispersion — в радианах.
		float disp = mode.m_Dispersion * Math.RAD2DEG;
		vector angles = direction.VectorToAngles();
		angles[0] = angles[0] + Math.RandomFloat(-disp, disp);
		angles[1] = angles[1] + Math.RandomFloat(-disp, disp);
		direction = angles.AnglesToVector();
		direction.Normalize();
	}

	//! Simplified server reload of the weapon in hands (see docs/research/combat.md
	//! "Перезарядка"): unjam > eject a chambered-out bullet > attach/swap a
	//! non-empty magazine from the inventory. Ammo-pile/bullet-per-bullet loading
	//! is not handled yet (later pass).
	//! TODO: возвращать кол-о секунд необходимое для выполенния действия, чтобы на вызывающей стороне ставить правильный кулдаун
	bool ReloadWeaponAI()
	{
		Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
		if (!weapon)
			return false;

		WeaponManager wm = GetWeaponManager();
		if (!wm)
			return false;
		if (wm.IsRunning())
			return true;

		if (wm.CanUnjam(weapon))
		{
			wm.Unjam();

			#ifdef DM_WEAPON_DEBUG_FSM
			dmBotLog.Debug("[Weapon] ReloadWeaponAI: unjam weapon=" + weapon);
			#endif
			return true;
		}

		int mi = weapon.GetCurrentMuzzle();
		if (weapon.IsChamberFiredOut(mi) && weapon.GetInternalMagazineCartridgeCount(mi) > 0 && wm.CanEjectBullet(weapon))
		{
			wm.EjectBullet();

			#ifdef DM_WEAPON_DEBUG_FSM
			dmBotLog.Debug("[Weapon] ReloadWeaponAI: eject bullet weapon=" + weapon);
			#endif
			return true;
		}

		Magazine mag = FindReloadMagazine(weapon, wm);
		if (!mag)
		{
			Magazine pile = FindChamberAmmo(weapon, wm);
			if (pile)
			{
				wm.LoadMultiBullet(pile);

				#ifdef DM_WEAPON_DEBUG_FSM
				dmBotLog.Debug("[Weapon] ReloadWeaponAI: chamber-load pile=" + pile + " weapon=" + weapon);
				#endif
				return true;
			}

			#ifdef DM_WEAPON_DEBUG_FSM
			dmBotLog.Debug("[Weapon] ReloadWeaponAI: no suitable magazine weapon=" + weapon);
			#endif
			return false;
		}
		
		if ( CheckNeedsChamber(weapon, mi) )
		{
			wm.EjectBullet();
			#ifdef DM_WEAPON_DEBUG_FSM
			dmBotLog.Debug("[Weapon] ReloadWeaponAI: calling EjectBullet");
			#endif
			return true;
		}
		else if (wm.CanAttachMagazine(weapon, mag))
		{
			wm.AttachMagazine(mag);

			#ifdef DM_WEAPON_DEBUG_FSM
			dmBotLog.Debug("[Weapon] ReloadWeaponAI: attach mag=" + mag + " weapon=" + weapon);
			#endif
			return true;
		}
		else if (wm.CanSwapMagazine(weapon, mag))
		{
			wm.SwapMagazine(mag);

			#ifdef DM_WEAPON_DEBUG_FSM
			dmBotLog.Debug("[Weapon] ReloadWeaponAI: swap mag=" + mag + " weapon=" + weapon);
			#endif
			return true;
		}

		return false;
	}

	//! Надеть item в руки с ручным ре-синком сети (готча задокументирована в docs/research/loot.md):
	//! SERVER-перенос у AI-бота не кладёт оружие в руки сам.
	bool TakeToHands(ItemBase item)
	{
		InventoryLocation src = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(src))
			return false;

		InventoryLocation dst = new InventoryLocation();
		dst.SetHands(this, item);

		GetGame().RemoteObjectTreeDelete(item);
		bool ok = LocalTakeToDst(src, dst);
		GetItemAccessor().HideItemInHands(true);
		GetItemAccessor().HideItemInHands(false);
		GetGame().RemoteObjectTreeCreate(item);
		return ok;
	}

	//! Перенести item в карго контейнера `to` (ручной ре-синк сети, готча — в docs/research/loot.md).
	bool TakeIntoCargo(ItemBase item, EntityAI to)
	{
		if (!item || !to)
			return false;
		InventoryLocation src = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(src))
			return false;
		InventoryLocation dst = new InventoryLocation();
		if (!to.GetInventory().FindFreeLocationFor(item, FindInventoryLocationType.CARGO, dst))
			return false;
		GetGame().RemoteObjectTreeDelete(item);
		bool ok = LocalTakeToDst(src, dst);
		GetGame().RemoteObjectTreeCreate(item);
		return ok;
	}

	//! Сбросить item на землю (server-side). Это ОВЕРРАЙД ванильного
	//! PlayerBase.DropItem(ItemBase) — ванильный вызывать нельзя (он делает
	//! PredictiveDropEntity и валит сервер, см. docs/research/loot.md). Возвращает
	//! true при успехе. Вызывающий сам выбирает предмет (GetDiscardOrder) и помечает
	//! его Ignore, чтобы не подобрать заново.
	override bool DropItem(ItemBase item)
	{
		if (!item)
			return false;

		GetGame().RemoteObjectTreeDelete(item);
		bool ok = LocalDropEntity(item);
		GetGame().RemoteObjectTreeCreate(item);

		if (ok)
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] DropItem: сбросил " + item.GetType());
			#endif
			return true;
		}
		return false;
	}

	//! Надеть item СТРОГО в слот slotId (dst.SetAttachment(this, item, slotId)) с ручным
	//! ре-синком сети: item приходит с земли, а SERVER-перенос у AI-бота не синкается
	//! сам (готча — docs/research/loot.md). При неудаче возвращает false (предмет
	//! остаётся на земле). SetAttachment — аналог dst.SetHands(this, item) в TakeToHands.
	bool TakeToAttachmentSlot(ItemBase item, int slotId)
	{
		InventoryLocation src = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(src))
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] TakeToAttachmentSlot: нет InventoryLocation у " + item.GetType());
			#endif
			return false;
		}

		InventoryLocation dst = new InventoryLocation();
		dst.SetAttachment(this, item, slotId);

		GetGame().RemoteObjectTreeDelete(item);
		bool ok = LocalTakeToDst(src, dst);
		GetGame().RemoteObjectTreeCreate(item);
		return ok;
	}

	//! Find a non-empty magazine in the inventory that fits the weapon (prefer an
	//! attachable one, else a swappable one). Returns null if there is none.
	Magazine FindReloadMagazine(Weapon_Base weapon, WeaponManager wm)
	{
		array<EntityAI> items = new array<EntityAI>();
		GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int i;
		Magazine mag;
		for (i = 0; i < items.Count(); i++)
		{
			mag = Magazine.Cast(items[i]);
			if (!mag || mag.IsAmmoPile() || mag.GetAmmoCount() <= 0)
				continue;

			if (wm.CanAttachMagazine(weapon, mag))
				return mag;
		}

		for (i = 0; i < items.Count(); i++)
		{
			mag = Magazine.Cast(items[i]);
			if (!mag || mag.IsAmmoPile() || mag.GetAmmoCount() <= 0)
				continue;

			if (wm.CanSwapMagazine(weapon, mag))
				return mag;
		}

		return null;
	}

	//! Find a non-empty loose ammo pile in the inventory that can be chamber-loaded
	//! into the weapon (break-action / single-round loading). Returns null if none.
	Magazine FindChamberAmmo(Weapon_Base weapon, WeaponManager wm)
	{
		array<EntityAI> items = new array<EntityAI>();
		GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int i;
		Magazine pile;
		for (i = 0; i < items.Count(); i++)
		{
			pile = Magazine.Cast(items[i]);
			if (!pile || !pile.IsAmmoPile() || pile.GetAmmoCount() <= 0)
				continue;

			if (wm.CanLoadBullet(weapon, pile))
				return pile;
		}
		return null;
	}

	//! Merge two identical ammo piles (server-side, no animation): find a pair of
	//! loose-round piles of the same type where one has free space and combine them
	//! via the vanilla Magazine.CombineItems (it transfers cartridges, keeps their
	//! damage/type, and the engine drops the emptied pile through destroyOnEmpty).
	//! One merge per call; the TidyInventory intent throttles the cadence.
	bool StackAmmoAI()
	{
		array<EntityAI> items = new array<EntityAI>();
		GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		string dstType;
		string srcType;
		int i;
		int j;
		Magazine a;
		Magazine b;
		for (i = 0; i < items.Count(); i++)
		{
			a = Magazine.Cast(items[i]);
			if (!a || !a.IsAmmoPile() || a.GetAmmoCount() <= 0)
				continue;

			for (j = i + 1; j < items.Count(); j++)
			{
				b = Magazine.Cast(items[j]);
				if (!b || !b.IsAmmoPile() || b.GetAmmoCount() <= 0)
					continue;
				if (a.GetType() != b.GetType())
					continue;

				if (a.GetAmmoCount() < a.GetAmmoMax())
				{
					dstType = a.GetType();
					srcType = b.GetType();
					a.CombineItems(b);

					#ifdef DM_BOT_DEBUG_FSM
					dmBotLog.Debug("[Tidy] StackAmmoAI: merged " + srcType + " into " + dstType);
					#endif
					return true;
				}
				else if (b.GetAmmoCount() < b.GetAmmoMax())
				{
					dstType = b.GetType();
					srcType = a.GetType();
					b.CombineItems(a);

					#ifdef DM_BOT_DEBUG_FSM
					dmBotLog.Debug("[Tidy] StackAmmoAI: merged " + srcType + " into " + dstType);
					#endif
					return true;
				}
			}
		}

		return false;
	}

	//! Load one empty/partial magazine from a compatible ammo pile (server-side, no
	//! animation — the weapon-FSM load-bullet path needs client input and is not
	//! wired for AI). Fills as many cartridges as fit (or until the pile runs dry),
	//! keeping each cartridge's damage/type via Acquire/Store. One magazine per
	//! call; the TidyInventory intent throttles the cadence.
	bool LoadMagazineAI()
	{
		array<EntityAI> items = new array<EntityAI>();
		GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int i;
		int j;
		Magazine mag;
		Magazine pile;
		for (i = 0; i < items.Count(); i++)
		{
			mag = Magazine.Cast(items[i]);
			if (!mag || mag.IsAmmoPile() || mag.GetAmmoCount() >= mag.GetAmmoMax())
				continue;

			for (j = 0; j < items.Count(); j++)
			{
				if (j == i)
					continue;
				pile = Magazine.Cast(items[j]);
				if (!pile || !pile.IsAmmoPile() || pile.GetAmmoCount() <= 0)
					continue;
				if (!mag.IsCompatiableAmmo(pile))
					continue;

				return TransferCartridges(pile, mag);
			}
		}

		return false;
	}

	//! Move cartridges from an ammo pile into a magazine until one runs out of
	//! space or bullets. Returns true if at least one cartridge was transferred.
	private bool TransferCartridges(Magazine src, Magazine dst)
	{
		bool moved = false;
		while (src.GetAmmoCount() > 0 && dst.CanAddCartridges(1))
		{
			float dmg;
			string cartType;
			if (!src.ServerAcquireCartridge(dmg, cartType))
				break;
			dst.ServerStoreCartridge(dmg, cartType);
			moved = true;
		}

		#ifdef DM_BOT_DEBUG_FSM
		if (moved)
			dmBotLog.Debug("[Tidy] LoadMagazineAI: loaded " + dst.GetType());
		#endif
		return moved;
	}

	//! Try to vault/climb the obstacle in front of the bot. We deliberately avoid
	//! m_JumpClimb.JumpOrClimb(): it re-runs its own DoPerformClimbTest (a different
	//! native than DoClimbTest, so the result can disagree with ours) and falls back
	//! to Jump() on failure — a useless hop that never starts the climb. Instead we
	//! start the climb directly from OUR DoClimbTest result, like Expansion's
	//! Expansion_Climb. Returns true if a climb was started.
	bool TryVaultClimb(float yaw = 0.0)
	{
		if ( yaw != 0.0 )
		{
			SetOrientation(Vector(yaw, 0.0, 0.0));
			SetTargetBodyYaw(yaw);
		}
		SHumanCommandClimbResult res = new SHumanCommandClimbResult();
		if (!HumanCommandClimb.DoClimbTest(this, res, 0))
			return false;
		if (!res.m_bIsClimb && !res.m_bIsClimbOver)
			return false;

		int climbType = GetClimbTypeLocal(res.m_fClimbHeight);
		if (climbType == -1)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Bot] TryVaultClimb: height=" + res.m_fClimbHeight + " out of range, abort");
			#endif
			return false;
		}

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Bot] TryVaultClimb: height=" + res.m_fClimbHeight + " type=" + climbType + " isClimb=" + res.m_bIsClimb + " isClimbOver=" + res.m_bIsClimbOver);
		#endif

		StartCommand_Climb(res, climbType);
		return true;
	}

	//! Local copy of the vanilla GetClimbType() thresholds: map the tested climb
	//! height to the climb command type (0/1 = vault, 2 = climb, -1 = out of range).
	private int GetClimbTypeLocal(float pHeight)
	{
		if (pHeight < 1.1)
			return 0;
		if (pHeight < 1.7)
			return 1;
		if (pHeight < 2.75)
			return 2;
		return -1;
	}
}

//! Model-specific classes. The config (CfgVehicles) inherits the vanilla
//! SurvivorM_*/SurvivorF_* classes for the visual model.
class dmAI_SurvivorM_Denis : dmAISurvivorBase {};
class dmAI_SurvivorM_Mirek : dmAISurvivorBase {};
class dmAI_SurvivorF_Eva : dmAISurvivorBase {};
