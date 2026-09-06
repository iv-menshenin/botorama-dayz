//! Standalone wrapper for the global native GetHive(). The modded
//! DayZPlayerImplement.GetHive() method shadows the global, so this wrapper is
//! the only way to reach the original native from inside that method.
Hive dmBotGlobalGetHive()
{
	return GetHive();
}
