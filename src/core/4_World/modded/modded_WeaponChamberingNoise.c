//! modded weapon-FSM reload states — publish a reload noise so bots can hear a
//! player reloading. The vanilla weapon FSM runs on both client and server; the
//! signal is server-only (dmNoiseSystem is a server-side bot-hearing channel).
//!
//! Only the "outer" states emit — one noise per reload cycle, not per cartridge:
//!   WeaponChambering                (load 1 round)
//!   ChamberMultiBullet              (load N rounds)
//!   WeaponAttachMagazine            (insert magazine)
//!   WeaponReplacingMagAndChamberNext (swap magazine + chamber next)
//! The WeaponChambering_* sub-states are deliberately skipped (they fire per round).

modded class WeaponChambering
{
	override void OnEntry(WeaponEventBase e)
	{
		super.OnEntry(e);
		#ifdef SERVER
		if (e.m_player)
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("", "[Noise] reload");
			#endif
			dmNoiseSystem.AddNoise(e.m_player, e.m_player.GetPosition(), DM_NOISE_RELOAD_STRENGTH, dmNoiseType.SOUND);
		}
		#endif
	}
}

modded class ChamberMultiBullet
{
	override void OnEntry(WeaponEventBase e)
	{
		super.OnEntry(e);
		#ifdef SERVER
		if (e.m_player)
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("", "[Noise] reload");
			#endif
			dmNoiseSystem.AddNoise(e.m_player, e.m_player.GetPosition(), DM_NOISE_RELOAD_STRENGTH, dmNoiseType.SOUND);
		}
		#endif
	}
}

modded class WeaponAttachMagazine
{
	override void OnEntry(WeaponEventBase e)
	{
		super.OnEntry(e);
		#ifdef SERVER
		if (e.m_player)
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("", "[Noise] reload");
			#endif
			dmNoiseSystem.AddNoise(e.m_player, e.m_player.GetPosition(), DM_NOISE_RELOAD_STRENGTH, dmNoiseType.SOUND);
		}
		#endif
	}
}

modded class WeaponReplacingMagAndChamberNext
{
	override void OnEntry(WeaponEventBase e)
	{
		super.OnEntry(e);
		#ifdef SERVER
		if (e.m_player)
		{
			#ifdef DM_PERCEPTION_DEBUG
			dmBotLog.Debug("", "[Noise] reload");
			#endif
			dmNoiseSystem.AddNoise(e.m_player, e.m_player.GetPosition(), DM_NOISE_RELOAD_STRENGTH, dmNoiseType.SOUND);
		}
		#endif
	}
}
