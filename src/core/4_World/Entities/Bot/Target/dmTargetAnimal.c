//! dmTargetAnimal — опознанное животное.
class dmTargetAnimal : dmTarget
{
	override bool IsAnimal()
	{
		return true;
	}
	override AnimalBase GetAnimal()
	{
		return AnimalBase.Cast(m_Entity);
	}
}
