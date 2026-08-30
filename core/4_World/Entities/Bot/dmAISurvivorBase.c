//! dmAISurvivorBase — AI survivor entity (client-server mod).
//!
//! The entity IS the bot: it inherits PlayerBase (so it has full character
//! simulation). The head look is applied in CommandHandler by setting the custom
//! animation graph variables dmAI_Look/dmAI_LookDirX/dmAI_LookDirY (added to a
//! custom player_main.agr, referenced via enfAnimSys/graphName in config.cpp).
//! The vanilla Look/LookDirX/LookDirY are kept so the engine's native "look at"
//! still finds them by hash; the graph's look poses now read the custom variables.

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
			ApplyLookVars();

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

		ApplyBodyTurn(pDt);
		ApplyMovement(pDt);
		ApplyStance(pDt);

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
				bot.RegisterDamageThreat(source);
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
}

//! Model-specific classes. The config (CfgVehicles) inherits the vanilla
//! SurvivorM_*/SurvivorF_* classes for the visual model.
class dmAI_SurvivorM_Denis : dmAISurvivorBase {};
class dmAI_SurvivorM_Mirek : dmAISurvivorBase {};
class dmAI_SurvivorF_Eva : dmAISurvivorBase {};
