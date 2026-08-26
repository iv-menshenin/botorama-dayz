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
			m_VarsBound = true;

			dmBotLog.Debug("dmAISurvivorBase.BindLookVars() Look=" + m_VarLook + " LookDirX=" + m_VarLookDirX + " LookDirY=" + m_VarLookDirY + " instType=" + GetInstanceType());
		}
		else
		{
			dmBotLog.Debug("dmAISurvivorBase.BindLookVars() GetAnimInterface() returned null instType=" + GetInstanceType());
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

			dmBotLog.Debug("dmAISurvivorBase.OnVariablesSynchronized() lookYaw=" + m_LookYawDeg + " lookPitch=" + m_LookPitchDeg + " instType=" + GetInstanceType());
		}
	}
#endif

	//! Called every tick during the deterministic simulation (the CommandHandler).
	//! We set the look vars AFTER super.CommandHandler() so the native "look at"
	//! modifier (which runs inside super and resets them from the aim) does not
	//! overwrite our values.
	override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);

		ApplyLookVars();

		if (Math.AbsFloat(m_LookYawDeg - m_LastLogLookYaw) > 0.5 || Math.AbsFloat(m_LookPitchDeg - m_LastLogLookPitch) > 0.5)
		{
			m_LastLogLookYaw = m_LookYawDeg;
			m_LastLogLookPitch = m_LookPitchDeg;

			dmBotLog.Debug("dmAISurvivorBase.CommandHandler() lookYaw=" + m_LookYawDeg + " lookPitch=" + m_LookPitchDeg + " instType=" + GetInstanceType());
		}
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
