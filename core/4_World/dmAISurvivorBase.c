//! dmAISurvivorBase — AI survivor entity (client-server mod).
//!
//! The entity IS the bot: it inherits PlayerBase (so it has full character
//! simulation) and provides the head-look via the vanilla heading model.
//!
//! Head look (vanilla mechanism):
//!   SDayZPlayerHeadingModel
//!     m_fOrientationAngle - horizontal body orientation (where you face) - rad
//!     m_fHeadingAngle     - horizontal aim angle (where you aim/look) - rad
//! Setting m_fHeadingAngle != m_fOrientationAngle makes the head/neck turn while
//! the body stays still (the same mechanism used by vanilla freelook/aim).

class dmAISurvivorBase : PlayerBase
{
	//! Head look yaw offset from the body (degrees, clamped to neck range).
	private float m_LookYawDeg = 0.0;

	void dmAISurvivorBase()
	{
	}

	//! Set the head look offset (degrees, relative to the body facing).
	void SetLookYaw(float offsetDeg)
	{
		m_LookYawDeg = Math.Clamp(offsetDeg, -DM_LOOK_MAX_YAW, DM_LOOK_MAX_YAW);
	}

	float GetLookYaw()
	{
		return m_LookYawDeg;
	}

	//! Drive the head look: body stays, head/aim turns by m_LookYawDeg.
	override bool HeadingModel(float pDt, SDayZPlayerHeadingModel pModel)
	{
		float bodyYawRad = GetOrientation()[0] * Math.DEG2RAD;

		pModel.m_fOrientationAngle = bodyYawRad;
		pModel.m_fHeadingAngle = bodyYawRad + m_LookYawDeg * Math.DEG2RAD;

		return true;
	}
}

//! Model-specific classes. The config (CfgVehicles) inherits the vanilla
//! SurvivorM_*/SurvivorF_* classes for the visual model.
class dmAI_SurvivorM_Denis : dmAISurvivorBase {};
class dmAI_SurvivorM_Mirek : dmAISurvivorBase {};
class dmAI_SurvivorF_Eva : dmAISurvivorBase {};
