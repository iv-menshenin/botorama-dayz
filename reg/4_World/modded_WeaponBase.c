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
	//! spawned from the neck bone toward dmAISurvivorBase's stored world aim
	//! direction (ComputeShot: aim + bullet-drop compensation), bypassing the
	//! vanilla GetCameraPoint (which an AI bot never drives).
	//! Server-only: on the client the weapon FSM still runs for the AI_REMOTE pawn
	//! and must fall back to the vanilla TryFireWeapon so the muzzle flash/sound
	//! (enabled by our SyncEventToRemote override) keeps playing.
	bool dmBot_Fire(int muzzleIndex)
	{
		#ifdef SERVER
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(GetHierarchyParent());
		if (!pawn)
			return false;
		vector origin;
		vector direction;
		vector velocity;
		pawn.ComputeShot(this, muzzleIndex, origin, direction, velocity);
		vector pos = origin + direction * 0.2;
		if ( pos == vector.Zero ) return false;

		#ifdef DM_WEAPON_DEBUG_FSM
		dmBotLog.Debug("[Weapon] dmBot_Fire: mi=" + muzzleIndex + " origin=" + origin + " pos=" + pos);
		dmBotLog.Debug("[Weapon] dmBot_Fire: direction=" + direction + " velocity=" + velocity + " chamberEmpty=" + IsChamberEmpty(muzzleIndex));
		dmBotLog.Debug("[Weapon] dmBot_Fire: firedOut=" + IsChamberFiredOut(muzzleIndex) + " jammed=" + IsJammed() + " ammo=" + GetChamberedCartridgeMagazineTypeName(muzzleIndex));
		#endif

		//! x != x is true only for NaN (catches NaN vector components that Length() <= 0.0 misses).
		bool dirNaN = (direction[0] != direction[0]) || (direction[1] != direction[1]) || (direction[2] != direction[2]);
		bool velNaN = (velocity[0] != velocity[0]) || (velocity[1] != velocity[1]) || (velocity[2] != velocity[2]);
		if (dirNaN || velNaN || direction.Length() <= 0.0 || velocity.Length() <= 0.0)
		{
			#ifdef DM_WEAPON_DEBUG_FSM
			dmBotLog.Debug("[Weapon] dmBot_Fire: невалидное направление/скорость (NaN/нуль), пропускаю");
			#endif
			return false;
		}

		bool fired = Fire(muzzleIndex, pos, direction, velocity);
		if (fired)
		{
			pawn.ApplyRecoil(this);
			vector ownerPos = pawn.GetPosition();
			dmNoiseSystem.AddNoise(pawn, ownerPos, DM_NOISE_GUNSHOT_STRENGTH);
		}
		return fired;
		#else
		return TryFireWeapon(this, muzzleIndex);
		#endif
	}

	//! Publish a gunshot noise after a successful shot for the VANILLA shooter path.
	//! The AI bot's own shot already emits noise inside dmBot_Fire (server branch),
	//! so this override skips AI owners to avoid a double noise (dmBot_Fire's
	//! AddNoise + this AddNoise).
	override void OnFire(int muzzle_index)
	{
		super.OnFire(muzzle_index);
		#ifdef SERVER
		Man owner = GetHierarchyRootPlayer();
		if (!owner)
			return;
		if (dmAISurvivorBase.Cast(owner))
			return;
		vector ownerPos = owner.GetPosition();
		dmNoiseSystem.AddNoise(owner, ownerPos, DM_NOISE_GUNSHOT_STRENGTH);
		#endif
	}
}
