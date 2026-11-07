//! modded DayZGame — publish a bullet-impact noise when a projectile hits a
//! surface. Vanilla FirearmEffects already adds a native NoiseSystem ping at the
//! impact point on the server; we mirror that with our own dmNoiseSystem signal in
//! the same place. The shooter is resolved from the projectile (source) so the bot
//! knows WHOSE bullet landed nearby (not just "a bullet landed").
modded class DayZGame
{
	override void FirearmEffects(Object source, Object directHit, int componentIndex, string surface, vector pos, vector surfNormal,
		vector exitPos, vector inSpeed, vector outSpeed, bool isWater, bool deflected, string ammoType)
	{
		super.FirearmEffects(source, directHit, componentIndex, surface, pos, surfNormal, exitPos, inSpeed, outSpeed, isWater, deflected, ammoType);
		#ifdef SERVER
		EntityAI shooter;
		if (source)
			shooter = EntityAI.Cast(source).GetHierarchyRootPlayer();
		dmNoiseSystem.AddNoise(shooter, pos, DM_NOISE_BULLETIMPACT_STRENGTH, dmNoiseType.BULLETIMPACT);
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
		dmBotLog.Debug("", "[Ballistics] IMPACT time=" + GetGame().GetTime() + " source=" + src + " directHit=" + hit);
		dmBotLog.Debug("", "[Ballistics] IMPACT componentIndex=" + componentIndex + " surface=" + surface);
		dmBotLog.Debug("", "[Ballistics] IMPACT pos=" + pos + " surfNormal=" + surfNormal + " exitPos=" + exitPos);
		dmBotLog.Debug("", "[Ballistics] IMPACT inSpeed=" + inSpeed + " outSpeed=" + outSpeed);
		dmBotLog.Debug("", "[Ballistics] IMPACT isWater=" + isWater + " deflected=" + deflected + " ammoType=" + ammoType);
		#endif
		#ifdef DM_PERCEPTION_DEBUG
		dmBotLog.Debug("", "[Noise] FirearmEffects: pos=" + pos);
		#endif
		#endif
	}

	//! Publish an explosion noise attributed to the culprit (the thrower of a
	//! grenade, or the victim of a mine). The culprit is resolved from the
	//! cross-module bridge (bound by HandActionThrow / TrapBase.SnapOnObject).
	//! Vanilla ExplosionEffectsEx already adds a native NoiseSystem ping at the
	//! explosion point; we mirror that with our own dmNoiseSystem signal so bots
	//! know WHOSE explosion they heard.
	override void ExplosionEffectsEx(Object source, Object directHit, int componentIndex, float energyFactor, float explosionFactor, HitInfo hitInfo)
	{
		super.ExplosionEffectsEx(source, directHit, componentIndex, energyFactor, explosionFactor, hitInfo);
		#ifdef SERVER
		EntityAI sourceEnt = EntityAI.Cast(source);
		EntityAI culprit = dmNoiseThrowerBridge.GetThrower(sourceEnt);
		if (!culprit)
			culprit = dmNoiseThrowerBridge.GetTriggered(sourceEnt);
		#ifdef DM_PERCEPTION_DEBUG
		string src = "null";
		if (sourceEnt)
			src = sourceEnt.GetType();
		string cul = "null";
		if (culprit)
			cul = culprit.GetType();
		dmBotLog.Debug("", "[Noise] ExplosionEffectsEx source=" + src + " culprit=" + cul);
		#endif
		if (culprit)
			dmNoiseSystem.AddNoise(culprit, hitInfo.GetPosition(), DM_NOISE_EXPLOSION_STRENGTH, dmNoiseType.EXPLOSION);
		dmNoiseThrowerBridge.ClearThrower(sourceEnt);
		dmNoiseThrowerBridge.ClearTriggered(sourceEnt);
		#endif
	}
}
