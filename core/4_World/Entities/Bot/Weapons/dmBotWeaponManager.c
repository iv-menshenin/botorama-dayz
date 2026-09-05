//! dmBotWeaponManager — server-side WeaponManager for AI bots.
//!
//! The vanilla WeaponManager.StartAction returns false on a multiplayer server
//! without a control_action (it relies on client input + sync junctures). This
//! subclass reproduces the Expansion eAIWeaponManager server path: it marks the
//! action ready and posts the weapon event directly, so reload/unjam/eject run
//! entirely from the server CommandHandler.
//!
//! Two more overrides are required for that server path to actually work:
//!  - OnWeaponActionEnd: the vanilla version only clears sync junctures on a
//!    server, but our StartAction reserves inventory via AddInventoryReservationEx;
//!    those must be cleared here or the weapon/magazine stay reserved forever and
//!    every later reload is rejected.
//!  - Update: the vanilla version tries to sync the jam chance to the client via
//!    DayZPlayerSyncJunctures.SendWeaponJamChance, which is meaningless for an
//!    AI_SERVER bot (no client); set the sync jam chance directly instead.

class dmBotWeaponManager : WeaponManager
{
	override bool StartAction(int action, Magazine mag, InventoryLocation il, ActionBase control_action = NULL)
	{
		if (control_action)
		{
			m_ControlAction = ActionBase.Cast(control_action);
			m_PendingWeaponAction = action;
			m_InProgress = true;
			m_IsEventSended = false;
			m_PendingTargetMagazine = mag;
			m_PendingInventoryLocation = il;
			StartPendingAction();
			return true;
		}
		m_WeaponInHand = Weapon_Base.Cast(m_player.GetHumanInventory().GetEntityInHands());
		if (!m_WeaponInHand) return false;

		if (!InventoryReservation(mag, il))
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[Weapon] StartAction: reservation failed action=" + action);
			#endif
			return false;
		}

		m_PendingWeaponAction = action;
		m_InProgress = true;
		m_IsEventSended = false;

		m_readyToStart = true;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Weapon] StartAction: ready action=" + action + " mag=" + mag);
		#endif

		return true;
	}

	override void StartPendingAction()
	{
		m_WeaponInHand = Weapon_Base.Cast(m_player.GetHumanInventory().GetEntityInHands());
		if (!m_WeaponInHand)
		{
			OnWeaponActionEnd();
			return;
		}

		switch (m_PendingWeaponAction)
		{
			case AT_WPN_ATTACH_MAGAZINE:
			{
				PostWeaponEvent(new WeaponEventAttachMagazine(m_player, m_PendingTargetMagazine));
				break;
			}
			case AT_WPN_SWAP_MAGAZINE:
			{
				PostWeaponEvent(new WeaponEventSwapMagazine(m_player, m_PendingTargetMagazine, m_PendingInventoryLocation));
				break;
			}
			case AT_WPN_DETACH_MAGAZINE:
			{
				Magazine mag = Magazine.Cast(m_PendingInventoryLocation.GetItem());
				PostWeaponEvent(new WeaponEventDetachMagazine(m_player, mag, m_PendingInventoryLocation));
				break;
			}
			case AT_WPN_UNJAM:
			{
				PostWeaponEvent(new WeaponEventUnjam(m_player, NULL));
				break;
			}
			case AT_WPN_EJECT_BULLET:
			{
				PostWeaponEvent(new WeaponEventMechanism(m_player, NULL));
				break;
			}
			case AT_WPN_LOAD_BULLET:
			{
				m_WantContinue = false;
				PostWeaponEvent(new WeaponEventLoad1Bullet(m_player, m_PendingTargetMagazine));
				break;
			}
			case AT_WPN_LOAD_MULTI_BULLETS_START:
			{
				PostWeaponEvent(new WeaponEventLoad1Bullet(m_player, m_PendingTargetMagazine));
				break;
			}
			case AT_WPN_LOAD_MULTI_BULLETS_END:
			{
				PostWeaponEvent(new WeaponEventContinuousLoadBulletEnd(m_player));
				break;
			}
			default:
				m_InProgress = false;
		}

		m_IsEventSended = true;
		m_canEnd = false;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Weapon] StartPendingAction: action=" + m_PendingWeaponAction + " inHands=" + m_WeaponInHand);
		#endif
	}

	//! Post a weapon event through the player inventory. The event is picked up
	//! and processed by the weapon FSM inside the next CommandHandler tick.
	void PostWeaponEvent(WeaponEventBase e)
	{
		DayZPlayerInventory inventory = m_player.GetDayZPlayerInventory();
		if (inventory)
			inventory.PostWeaponEvent(e);
	}

	override void OnWeaponActionEnd()
	{
		if (!m_InProgress)
			return;

		if (!m_ControlAction)
		{
			InventoryLocation il = new InventoryLocation();
			il.SetHands(m_player, m_player.GetItemInHands());
			m_player.GetInventory().ClearInventoryReservation(m_player.GetItemInHands(), il);

			if (m_PendingTargetMagazine)
				m_player.GetInventory().ClearInventoryReservation(m_PendingTargetMagazine, m_TargetInventoryLocation);

			if (m_PendingInventoryLocation)
				m_player.GetInventory().ClearInventoryReservation(m_PendingInventoryLocation.GetItem(), m_PendingInventoryLocation);
		}

		m_ControlAction = NULL;
		m_PendingWeaponAction = -1;
		m_PendingTargetMagazine = NULL;
		m_PendingInventoryLocation = NULL;
		m_TargetInventoryLocation = NULL;
		m_PendingWeaponActionAcknowledgmentID = -1;
		m_InProgress = false;
		m_readyToStart = false;
		m_WantContinue = true;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[Weapon] OnWeaponActionEnd: action done");
		#endif
	}

	override void Update(float deltaT)
	{
		if (m_WeaponInHand != m_player.GetItemInHands())
		{
			if (m_WeaponInHand)
			{
				m_SuitableMagazines.Clear();
				OnWeaponActionEnd();
			}
			m_WeaponInHand = Weapon_Base.Cast(m_player.GetItemInHands());
			if (m_WeaponInHand)
			{
				m_MagazineInHand = null;
				SetSutableMagazines();
				m_WeaponInHand.SetSyncJammingChance(0);
			}
			m_AnimationRefreshCooldown = 0;
		}

		if (m_WeaponInHand)
		{
			if (m_AnimationRefreshCooldown)
			{
				m_AnimationRefreshCooldown--;
				if (m_AnimationRefreshCooldown == 0)
					RefreshAnimationState();
			}

			m_WeaponInHand.SetSyncJammingChance(m_WeaponInHand.GetChanceToJam());

			if (m_readyToStart)
			{
				StartPendingAction();
				m_readyToStart = false;
				return;
			}

			if (!m_InProgress || !m_IsEventSended)
				return;

			if (m_canEnd)
			{
				if (m_WeaponInHand.IsIdle())
					OnWeaponActionEnd();
			}
			else
			{
				m_canEnd = true;
				m_justStart = true;
			}
		}
		else
		{
			if (m_MagazineInHand != m_player.GetItemInHands())
			{
				m_MagazineInHand = MagazineStorage.Cast(m_player.GetItemInHands());
				if (m_MagazineInHand)
					SetSutableMagazines();
			}
		}
	}
}
