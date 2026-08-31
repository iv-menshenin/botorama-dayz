//! modded WeaponFire — route the weapon FSM's fire through the AI's explicit aim
//! direction instead of the vanilla TryFireWeapon (which reads GetCameraPoint, an
//! internal weapon aim that an AI bot never drives). dmBot_Fire handles the server
//! (bullet toward GetWeaponAimDirection) vs client (vanilla muzzle effects) split.
modded class WeaponFire
{
	override void OnEntry(WeaponEventBase e)
	{
		if (e)
		{
			if (g_Game.IsServer())
			{
				PlayerBase playerOwner;
				Class.CastTo(playerOwner, m_weapon.GetHierarchyParent());
				m_weapon.AddJunctureToAttachedMagazine(playerOwner, 100);
			}

			m_dtAccumulator = 0;

			if (LogManager.IsWeaponLogEnable()) { wpnPrint("[wpnfsm] " + Object.GetDebugName(m_weapon) + " WeaponFire bang!"); }
			int mi = m_weapon.GetCurrentMuzzle();
			if (m_weapon.dmBot_Fire(mi))
			{
				DayZPlayerImplement p;
				if (Class.CastTo(p, e.m_player))
					p.GetAimingModel().SetRecoil(m_weapon);
				m_weapon.OnFire(mi);
			}
		}
		super.OnEntry(e);
	}
}
