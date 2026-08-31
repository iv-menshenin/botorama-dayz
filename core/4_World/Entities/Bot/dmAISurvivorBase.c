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

	//! One-shot melee attack request from the brain (see RequestMeleeAttack). The
	//! fight logic consumes it as soon as the strike starts.
	private bool m_MeleeAttackRequest = false;
	private EntityAI m_MeleeTarget;

	//! One-shot fire request from the brain (see RequestFire). Processed by
	//! TryFireWeapon inside the CommandHandler; m_FireCooldown throttles cadence.
	private bool m_FireRequest = false;
	private float m_FireCooldown = 0.0;

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

	//! Whether the weapon is fully ready to fire (all readiness timings elapsed).
	bool IsReadyToShoot()
	{
		return m_WeaponRaised && m_WeaponRaisedTimer >= m_RaiseReadyDuration;
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
			m_AimRelAngleLR = 0.0;
			m_AimRelAngleUD = 0.0;
			return;
		}

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
		int neckBone = GetBoneIndexByName("neck");
		if (neckBone >= 0)
			eyePos = GetBonePositionWS(neckBone);

		vector aimDir = aimPos - eyePos;
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

	//! The shooting accuracy model (used by the fire path in Phase 3).
	dmAiming GetAiming()
	{
		return m_Aiming;
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
		if (source)
		{
			dmAISurvivor bot = dmAISurvivor.Find(this);
			if (bot)
				bot.RegisterDamageThreat(source, damageResult.GetHighestDamage("Health"));
		}
	}

	//! Death — logging only: report whether the vanilla EEKilled registered the
	//! corpse for decay (depends on the pawn having a CE profile from CreateObject).
	override void EEKilled(Object killer)
	{
		super.EEKilled(killer);

		#ifdef DM_BOT_DEBUG_BODY
		dmBotLog.Debug("EEKilled: hasCEProfile=" + (GetEconomyProfile() != null) + " corpseProcessing=" + m_CorpseProcessing + " corpseState=" + m_CorpseState + " lifetime=" + GetLifetime());
		#endif
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
				float step = Math.Clamp(dBody, -DM_MOVE_TURN_RATE * pDt, DM_MOVE_TURN_RATE * pDt);
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

		wm.Fire(weapon);
		ConsumeFireRequest();
		m_FireCooldown = DM_BOT_FIRE_COOLDOWN;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Bot] TryFireWeapon: fired weapon=" + weapon);
		#endif
	}

	//! Simplified server reload of the weapon in hands (see docs/research/combat.md
	//! "Перезарядка"): unjam > eject a chambered-out bullet > attach/swap a
	//! non-empty magazine from the inventory. Ammo-pile/bullet-per-bullet loading
	//! is not handled yet (later pass).
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

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Bot] ReloadWeaponAI: unjam weapon=" + weapon);
			#endif
			return true;
		}

		int mi = weapon.GetCurrentMuzzle();
		if (weapon.IsChamberFiredOut(mi) && weapon.GetInternalMagazineCartridgeCount(mi) > 0 && wm.CanEjectBullet(weapon))
		{
			wm.EjectBullet();

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Bot] ReloadWeaponAI: eject bullet weapon=" + weapon);
			#endif
			return true;
		}

		Magazine mag = FindReloadMagazine(weapon, wm);
		if (!mag)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Bot] ReloadWeaponAI: no suitable magazine weapon=" + weapon);
			#endif
			return false;
		}

		if (wm.CanAttachMagazine(weapon, mag))
		{
			wm.AttachMagazine(mag);

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Bot] ReloadWeaponAI: attach mag=" + mag + " weapon=" + weapon);
			#endif
			return true;
		}
		else if (wm.CanSwapMagazine(weapon, mag))
		{
			wm.SwapMagazine(mag);

			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Bot] ReloadWeaponAI: swap mag=" + mag + " weapon=" + weapon);
			#endif
			return true;
		}

		return false;
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

	//! Try to vault/climb the obstacle in front of the bot. First a DoClimbTest:
	//! if there is a vault/climb edge ahead, start m_JumpClimb.JumpOrClimb() (the
	//! full cycle: test -> type -> CanClimb -> StartCommand_Climb). Returns true if
	//! a climb was started.
	bool TryVaultClimb()
	{
		SHumanCommandClimbResult res = new SHumanCommandClimbResult();
		if (!HumanCommandClimb.DoClimbTest(this, res, 0))
			return false;
		if (!res.m_bIsClimb && !res.m_bIsClimbOver)
			return false;

		m_JumpClimb.JumpOrClimb();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Bot] TryVaultClimb: isClimb=" + res.m_bIsClimb + " isClimbOver=" + res.m_bIsClimbOver);
		#endif
		return true;
	}
}

//! Model-specific classes. The config (CfgVehicles) inherits the vanilla
//! SurvivorM_*/SurvivorF_* classes for the visual model.
class dmAI_SurvivorM_Denis : dmAISurvivorBase {};
class dmAI_SurvivorM_Mirek : dmAISurvivorBase {};
class dmAI_SurvivorF_Eva : dmAISurvivorBase {};
