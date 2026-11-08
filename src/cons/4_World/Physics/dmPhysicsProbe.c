//! dmPhysicsProbe — static wrapper around the native DayZ physics queries used
//! across the bot hotpaths (raycast proxy / raycast / bullet cast / scanbox).
//!
//! Every call site swaps ONLY the function name (same signature/semantics as the
//! native), so telemetry and future throttling live in ONE place instead of ~35
//! scattered sites. Each wrapper folds a length/volume observation into
//! dmPerfTelemetry under #ifdef DM_BOT_PROFILE (zero overhead otherwise, beyond
//! the one extra function call).
//!
//! Lives in cons/4_World (not core/4_World) because roads/4_World must see it
//! too: roads only depends on cons, not on core.

class dmPhysicsProbe
{
	//! RaycastRVProxy wrapper. Same signature/semantics as the native.
	static bool CastProxy(RaycastRVParams params, out array<ref RaycastRVResult> hits)
	{
		#ifdef DM_BOT_PROFILE
		dmPerfTelemetry.Raycast(vector.Distance(params.begPos, params.endPos));
		#endif
		return DayZPhysics.RaycastRVProxy(params, hits);
	}

	//! RaycastRV wrapper. Same signature/semantics (and defaults) as the native.
	static bool CastRV(vector beg, vector end, out vector contactPos, out vector contactDir, out int contactComponent, set<Object> results = null, Object withObj = null, Object ignoreObj = null, bool sorted = false, bool groundOnly = false, int intersectType = ObjIntersectView, float radius = 0.0, CollisionFlags flags = CollisionFlags.NEARESTCONTACT)
	{
		#ifdef DM_BOT_PROFILE
		dmPerfTelemetry.Raycast(vector.Distance(beg, end));
		#endif
		return DayZPhysics.RaycastRV(beg, end, contactPos, contactDir, contactComponent, results, withObj, ignoreObj, sorted, groundOnly, intersectType, radius, flags);
	}

	//! RayCastBullet wrapper. Same signature/semantics as the native.
	static bool CastBullet(vector from, vector to, PhxInteractionLayers layers, Object exclude, out Object hitObj, out vector hitPos, out vector hitNormal, out float hitFrac)
	{
		#ifdef DM_BOT_PROFILE
		dmPerfTelemetry.Raycast(vector.Distance(from, to));
		#endif
		return DayZPhysics.RayCastBullet(from, to, layers, exclude, hitObj, hitPos, hitNormal, hitFrac);
	}

	//! SceneGetEntitiesInBox wrapper. Same signature/semantics as the native.
	static void ScanBox(vector min, vector max, out array<EntityAI> entities, int flags = QueryFlags.DYNAMIC)
	{
		#ifdef DM_BOT_PROFILE
		vector d = max - min;
		dmPerfTelemetry.ScanBox(d[0] * d[1] * d[2]);
		#endif
		DayZPlayerUtils.SceneGetEntitiesInBox(min, max, entities, flags);
	}
}
