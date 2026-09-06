//! modded WeaponFire — route the weapon FSM's fire through the AI's explicit aim
//! direction instead of the vanilla TryFireWeapon (which reads GetCameraPoint, an
//! internal weapon aim that an AI bot never drives). dmBot_Fire handles the server
//! (bullet toward the pawn's stored world aim direction) vs client (vanilla muzzle
//! effects) split.
//!
//! The AI branches can't call super.OnEntry(e): in a modded class, super() runs the
//! ORIGINAL vanilla OnEntry, which would re-fire through TryFireWeapon and
//! double-shoot after dmBot_Fire (the Fire() native ignores the emptied chamber).
//! So the vanilla "tail" (start the nested FSM + play the weapon action) is factored
//! into dmBot_Vanilla_OnEntry, mirroring Expansion's eAI_Vanilla_OnEntry.

modded class WeaponStateBase
{
	void dmBot_Vanilla_OnEntry(WeaponEventBase e)
	{
		if (HasFSM() && !m_fsm.IsRunning())
			m_fsm.Start(e);
	}
}

modded class WeaponStartAction
{
	override void dmBot_Vanilla_OnEntry(WeaponEventBase e)
	{
		super.dmBot_Vanilla_OnEntry(e);
		if (e)
		{
			if (e.m_player)
			{
				HumanCommandWeapons hcw = e.m_player.GetCommandModifier_Weapons();
				if (hcw)
				{
					HumanCommandAdditives ad = e.m_player.GetCommandModifier_Additives();
					if (ad)
						ad.CancelModifier();
					hcw.StartAction(m_action, m_actionType);
				}
			}
		}
	}
}

modded class WeaponFire
{
	override void OnEntry(WeaponEventBase e)
	{
		if (e)
		{
			dmAISurvivorBase pawn;
			if (Class.CastTo(pawn, e.m_player))
			{
				m_dtAccumulator = 0;
				if (LogManager.IsWeaponLogEnable()) { wpnPrint("[wpnfsm] " + Object.GetDebugName(m_weapon) + " WeaponFire bang!"); }
				int mi = m_weapon.GetCurrentMuzzle();
				if (m_weapon.dmBot_Fire(mi))
					m_weapon.OnFire(mi);
				super.dmBot_Vanilla_OnEntry(e);
				return;
			}
		}
		super.OnEntry(e);
	}
}

modded class WeaponFireWithEject
{
	override void OnEntry(WeaponEventBase e)
	{
		if (e)
		{
			dmAISurvivorBase pawn;
			if (Class.CastTo(pawn, e.m_player))
			{
				m_dtAccumulator = 0;
				if (LogManager.IsWeaponLogEnable()) { wpnPrint("[wpnfsm] " + Object.GetDebugName(m_weapon) + " WeaponFire bang!"); }
				int mi = m_weapon.GetCurrentMuzzle();
				if (m_weapon.dmBot_Fire(mi))
				{
					m_weapon.EjectCasing(mi);
					m_weapon.EffectBulletHide(mi);
					m_weapon.OnFire(mi);
				}
				super.dmBot_Vanilla_OnEntry(e);
				return;
			}
		}
		super.OnEntry(e);
	}
}

modded class WeaponFireMultiMuzzle
{
	override void OnEntry(WeaponEventBase e)
	{
		if (e)
		{
			dmAISurvivorBase pawn;
			if (Class.CastTo(pawn, e.m_player))
			{
				m_dtAccumulator = 0;
				if (LogManager.IsWeaponLogEnable()) { wpnPrint("[wpnfsm] " + Object.GetDebugName(m_weapon) + " WeaponFire bang bang!"); }
				int mi = m_weapon.GetCurrentMuzzle();
				int b = m_weapon.GetCurrentModeBurstSize(mi);
				int muzzleCount = m_weapon.GetMuzzleCount();

				#ifdef DM_WEAPON_DEBUG_FSM
				dmBotLog.Debug("[Weapon] WeaponFireMultiMuzzle: b=" + b + " muzzleCount=" + muzzleCount);
				dmBotLog.Debug("[Weapon] WeaponFireMultiMuzzle: mi=" + mi + " mode=" + m_weapon.GetCurrentMode(mi));
				#endif

				if (b > 1)
				{
					int maxMuzzle = muzzleCount;
					for (int i = 0; i < b && i < maxMuzzle; i++)
					{
						if (m_weapon.dmBot_Fire(i))
							m_weapon.OnFire(i);
					}
				}
				else
				{
					if (m_weapon.dmBot_Fire(mi))
						m_weapon.OnFire(mi);
				}
				if (mi >= m_weapon.GetMuzzleCount() - 1)
					m_weapon.SetCurrentMuzzle(0);
				else
					m_weapon.SetCurrentMuzzle(mi + 1);
				#ifdef DM_WEAPON_DEBUG_FSM
				dmBotLog.Debug("[Weapon] WeaponFireMultiMuzzle: re-sync FSM after double fire");
				#endif
				m_weapon.RandomizeFSMState();
				super.dmBot_Vanilla_OnEntry(e);
				return;
			}
		}
		super.OnEntry(e);
	}
}

modded class WeaponFireToJam
{
	override void OnEntry(WeaponEventBase e)
	{
		if (e)
		{
			dmAISurvivorBase pawn;
			if (Class.CastTo(pawn, e.m_player))
			{
				m_dtAccumulator = 0;
				if (LogManager.IsWeaponLogEnable()) { wpnPrint("[wpnfsm] " + Object.GetDebugName(m_weapon) + " WeaponFire bang! and jam?"); }
				int mi = m_weapon.GetCurrentMuzzle();
				if (m_weapon.dmBot_Fire(mi))
				{
					m_weapon.SetJammed(true);
					m_weapon.OnFire(mi);
				}
				m_weapon.ResetBurstCount();
				super.dmBot_Vanilla_OnEntry(e);
				return;
			}
		}
		m_weapon.ResetBurstCount();
		super.OnEntry(e);
	}
}
