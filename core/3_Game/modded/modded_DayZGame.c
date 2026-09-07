//! modded DayZGame — publish a bullet-impact noise when a projectile hits a
//! surface. Vanilla FirearmEffects already adds a native NoiseSystem ping at the
//! impact point on the server; we mirror that with our own dmNoiseSystem signal in
//! the same place (source = null: on reception this reads as "a bullet landed
//! nearby", not a gunshot from a shooter).
modded class DayZGame
{
	override void FirearmEffects(Object source, Object directHit, int componentIndex, string surface, vector pos, vector surfNormal,
		vector exitPos, vector inSpeed, vector outSpeed, bool isWater, bool deflected, string ammoType)
	{
		super.FirearmEffects(source, directHit, componentIndex, surface, pos, surfNormal, exitPos, inSpeed, outSpeed, isWater, deflected, ammoType);
		#ifdef SERVER
		dmNoiseSystem.AddNoise(null, pos, DM_NOISE_BULLETIMPACT_STRENGTH, dmNoiseType.BULLETIMPACT);
		dmBallisticsBridge.OnImpact(EntityAI.Cast(source), directHit != null, pos, inSpeed.Length());
		#ifdef DM_BOT_DEBUG_BALLISTICS
		dmBotLog.Debug("[Ballistics] IMPACT time=" + GetGame().GetTime() + " pos=" + pos + " speed=" + inSpeed.Length());
		#endif
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("[Noise] FirearmEffects: pos=" + pos);
		#endif
		#endif
	}
}
