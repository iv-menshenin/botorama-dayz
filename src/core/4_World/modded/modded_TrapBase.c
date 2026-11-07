//! modded TrapBase — bind the victim (who stepped on the trap) into the
//! cross-module bridge so that the explosion noise in DayZGame.ExplosionEffectsEx
//! can be attributed to them. The binding MUST be written BEFORE super.SnapOnObject,
//! because the vanilla SnapOnObject calls OnSteppedOn (→ Explode → ExplosionEffectsEx)
//! at the end.
modded class TrapBase
{
	#ifdef SERVER
	override void SnapOnObject(EntityAI victim)
	{
		if (victim)
		{
			dmNoiseThrowerBridge.BindTriggered(this, victim);
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("", "[Noise] bind triggered " + victim.GetType());
			#endif
		}
		super.SnapOnObject(victim);
	}
	#endif
}
