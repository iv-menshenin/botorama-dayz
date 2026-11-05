//! dmTargetPlayer — опознанный игрок ИЛИ бот (разницы не делаем).
class dmTargetPlayer : dmTarget
{
	override bool IsPlayer()
	{
		return true;
	}
	override bool IsPlayerAI()
	{
		return dmAISurvivorBase.Cast(m_Entity) != null;
	}
	override PlayerBase GetPlayer()
	{
		return PlayerBase.Cast(m_Entity);
	}
}
