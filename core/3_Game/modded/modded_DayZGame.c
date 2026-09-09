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
		dmBallisticsBridge.OnImpact(EntityAI.Cast(source), directHit, pos, inSpeed.Length());
		#ifdef DM_BOT_DEBUG_BALLISTICS
		string src = "null";
		EntityAI srcEnt = EntityAI.Cast(source);
		if (srcEnt)
			src = srcEnt.GetType();
		string hit = "null";
		EntityAI hitEnt = EntityAI.Cast(directHit);
		if (hitEnt)
			hit = hitEnt.GetType();
		dmBotLog.Debug("[Ballistics] IMPACT time=" + GetGame().GetTime() + " source=" + src + " directHit=" + hit);
		dmBotLog.Debug("[Ballistics] IMPACT componentIndex=" + componentIndex + " surface=" + surface);
		dmBotLog.Debug("[Ballistics] IMPACT pos=" + pos + " surfNormal=" + surfNormal + " exitPos=" + exitPos);
		dmBotLog.Debug("[Ballistics] IMPACT inSpeed=" + inSpeed + " outSpeed=" + outSpeed);
		dmBotLog.Debug("[Ballistics] IMPACT isWater=" + isWater + " deflected=" + deflected + " ammoType=" + ammoType);
		#endif
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("[Noise] FirearmEffects: pos=" + pos);
		#endif
		#endif
	}
}
