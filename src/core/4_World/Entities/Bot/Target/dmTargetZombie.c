//! dmTargetZombie — опознанный зомби.
class dmTargetZombie : dmTarget
{
	override bool IsZombie()
	{
		return true;
	}
	override ZombieBase GetZombie()
	{
		return ZombieBase.Cast(m_Entity);
	}
}
