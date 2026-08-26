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

	void dmAISurvivorBase()
	{
		RegisterNetSyncVariableFloat("m_LookYawDeg", -DM_LOOK_MAX_YAW, DM_LOOK_MAX_YAW, 1);
		RegisterNetSyncVariableFloat("m_LookPitchDeg", -DM_LOOK_MAX_PITCH, DM_LOOK_MAX_PITCH, 1);
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

			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("dmAISurvivorBase.BindLookVars() Look=" + m_VarLook + " LookDirX=" + m_VarLookDirX + " LookDirY=" + m_VarLookDirY + " TurnAmount=" + m_VarTurnAmount + " CmdTurn=" + m_CmdTurn + " CmdStopTurn=" + m_CmdStopTurn + " instType=" + GetInstanceType());
			#endif
		}
		else
		{
			#ifdef DM_BOT_DEBUG
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

			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("dmAISurvivorBase.OnVariablesSynchronized() lookYaw=" + m_LookYawDeg + " lookPitch=" + m_LookPitchDeg + " instType=" + GetInstanceType());
			#endif
		}
	}
#endif

	//! Called every tick during the deterministic simulation (the CommandHandler).
	//! Look vars are set before super; the turn commands are set AFTER super
	//! (matching how the Expansion AI calls its movement PreAnimUpdate after super),
	//! so the vanilla command processing doesn't consume/overwrite them.
	override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		ApplyLookVars();

		super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);

		ApplyBodyTurn(pDt);

		if (Math.AbsFloat(m_LookYawDeg - m_LastLogLookYaw) > 0.5 || Math.AbsFloat(m_LookPitchDeg - m_LastLogLookPitch) > 0.5)
		{
			m_LastLogLookYaw = m_LookYawDeg;
			m_LastLogLookPitch = m_LookPitchDeg;

			#ifdef DM_BOT_DEBUG
			dmBotLog.Debug("dmAISurvivorBase.CommandHandler() lookYaw=" + m_LookYawDeg + " lookPitch=" + m_LookPitchDeg + " instType=" + GetInstanceType());
			#endif
		}
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

		HumanCommandMove move = GetCommand_Move();
		bool moving = move && move.GetCurrentMovementSpeed() > 0.01;

		if (moving)
		{
			//! Cancel any in-progress foot-step turn; slide instead while walking.
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

	//! Set the desired body yaw (world, degrees). Called by the controller.
	void SetTargetBodyYaw(float yaw)
	{
		m_TargetBodyYaw = yaw;
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
}

//! Model-specific classes. The config (CfgVehicles) inherits the vanilla
//! SurvivorM_*/SurvivorF_* classes for the visual model.
class dmAI_SurvivorM_Denis : dmAISurvivorBase {};
class dmAI_SurvivorM_Mirek : dmAISurvivorBase {};
class dmAI_SurvivorF_Eva : dmAISurvivorBase {};
