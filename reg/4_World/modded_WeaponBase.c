//! modded Weapon_Base — let the client hear the AI bot's gunshot.
//!
//! The muzzle sound/flash is a client-side effect played by the weapon FSM's
//! TryFireWeapon on the client. Vanilla SyncEventToRemote only sends the
//! INPUT_UDT_WEAPON_REMOTE_EVENT for INSTANCETYPE_SERVER; an AI bot is
//! INSTANCETYPE_AI_SERVER, so the event is dropped and the client stays silent.
//! This override mirrors the vanilla send for AI_SERVER too (the client AI_REMOTE
//! side falls through to super, which is a no-op there — no double-send).
modded class Weapon_Base
{
	override void SyncEventToRemote(WeaponEventBase e)
	{
		DayZPlayer p = DayZPlayer.Cast(GetHierarchyParent());
		if (p && p.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
		{
			ScriptRemoteInputUserData ctx = new ScriptRemoteInputUserData();
			ctx.Write(INPUT_UDT_WEAPON_REMOTE_EVENT);
			e.WriteToContext(ctx);
			p.StoreInputForRemotes(ctx);
		}
		else
		{
			super.SyncEventToRemote(e);
		}
	}

	//! Fire one shot with the EXPLICIT aim direction (magic 100%): the bullet is
	//! spawned from the neck bone toward dmAISurvivorBase.GetWeaponAimDirection(),
	//! bypassing the vanilla GetCameraPoint (which an AI bot never drives).
	//! Server-only: on the client the weapon FSM still runs for the AI_REMOTE pawn
	//! and must fall back to the vanilla TryFireWeapon so the muzzle flash/sound
	//! (enabled by our SyncEventToRemote override) keeps playing.
	bool dmBot_Fire(int muzzleIndex)
	{
		#ifdef SERVER
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(GetHierarchyParent());
		if (!pawn)
			return false;
		int neck = pawn.GetBoneIndexByName("neck");
		if (neck < 0)
			return false;
		vector pos = pawn.GetBonePositionWS(neck);
		vector dir = pawn.GetWeaponAimDirection();
		pos = pos + dir * 0.2;
		bool fired = Fire(muzzleIndex, pos, dir, dir);
		if (fired)
		{
			float recoilPitch = pawn.GetAiming().AddRecoil(DM_AIM_RECOIL_MODIFIER);
			pawn.KickRecoilVisual(recoilPitch);
		}
		return fired;
		#else
		return TryFireWeapon(this, muzzleIndex);
		#endif
	}
}
