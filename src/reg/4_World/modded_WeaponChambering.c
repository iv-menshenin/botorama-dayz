//! modded WeaponChambering_Base — silence the vanilla DropBullet Error.
//!
//! On abort mid-chambering (death / vault-climb) the vanilla DropBullet throws
//! Error("cannot drop bullet - lost") when the cartridge was already consumed.
//! For an AI bot that is recoverable noise: the round is gone either way, so
//! just return false instead of spamming the log.
modded class WeaponChambering_Base
{
	override bool DropBullet(WeaponEventBase e)
	{
		if (GetGame().IsServer())
		{
			if (m_magazineType.Length() > 0 && m_type.Length() > 0)
				return DayZPlayerUtils.HandleDropCartridge(e.m_player, m_damage, m_type, m_magazineType);
			return false;
		}
		return true;
	}
}
