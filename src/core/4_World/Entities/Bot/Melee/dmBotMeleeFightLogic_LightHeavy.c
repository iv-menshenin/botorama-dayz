//! dmBotMeleeFightLogic_LightHeavy — bot melee driver (replaces the old stub).
//!
//! The vanilla DayZPlayerMeleeFightLogic_LightHeavy.HandleFightLogic null-derefs
//! hcm (HumanCommandMove) outside the MOVE command, and CanFight() is always true
//! for an AI bot (no ActionManager), so the vanilla code throws a VM exception.
//! This subclass drives a single light/heavy attack per request from the brain,
//! targeting the entity chosen by dmBotMeleeCombat (head->torso raycast, no raised stance).

class dmBotMeleeFightLogic_LightHeavy : DayZPlayerMeleeFightLogic_LightHeavy
{
	dmAISurvivorBase m_Bot;

	override void Init(DayZPlayerImplement player)
	{
		super.Init(player);
		m_Bot = dmAISurvivorBase.Cast(player);
	}

	//! Heavy when the bot has stamina for it, light otherwise (inputs ignored).
	override protected EMeleeHitType GetAttackTypeFromInputs(HumanInputController pInputs)
	{
		if ( m_Bot && m_Bot.GetHumanInventory() )
		{
			Weapon_Base w = Weapon_Base.Cast(m_Bot.GetHumanInventory().GetEntityInHands());
			if ( w )
			{
				if ( w.IsInherited(Rifle_Base) )
					return EMeleeHitType.WPN_HIT_BUTTSTOCK;	
				return EMeleeHitType.WPN_HIT;
			}
		}

		if (m_Player.CanConsumeStamina(EStaminaConsumers.MELEE_HEAVY)) return EMeleeHitType.HEAVY;

		return EMeleeHitType.LIGHT;
	}

	override bool HandleFightLogic(int pCurrentCommandID, HumanInputController pInputs, EntityAI pEntityInHands, HumanMovementState pMovementState, out bool pContinueAttack)
	{
		pContinueAttack = false;

		InventoryItem itemInHands = InventoryItem.Cast(pEntityInHands);

		//! Damage is applied on the Hit anim event. It must run every frame during
		//! the MELEE2 command, even though the one-shot request flag is already
		//! consumed and GetCommand_Move() is null while in MELEE2.
		HandleHitEvent(pCurrentCommandID, pInputs, itemInHands, pMovementState, pContinueAttack);

		if (!m_Bot || !m_Bot.HasMeleeAttackRequest())
			return false;

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("dmBotMeleeFightLogic: melee request");
		#endif

		HumanCommandMove hcm = m_Player.GetCommand_Move();
		if (!hcm)
			return false;

		if (pCurrentCommandID != DayZPlayerConstants.COMMANDID_MOVE)
			return false;

		m_HitType = GetAttackTypeFromInputs(pInputs);
		m_MeleeCombat.Update(itemInHands, m_HitType);

		EntityAI target = m_MeleeCombat.GetTargetEntity();
		if (!target)
		{
			m_Bot.ConsumeMeleeAttackRequest();
			return false;
		}

		m_Player.StartCommand_Melee2(null, m_HitType == EMeleeHitType.HEAVY, 1.0, m_MeleeCombat.GetHitPos());

		if (m_HitType == EMeleeHitType.HEAVY)
			m_Player.DepleteStamina(EStaminaModifiers.MELEE_HEAVY);
		else
			m_Player.DepleteStamina(EStaminaModifiers.MELEE_LIGHT);

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("dmBotMeleeFightLogic: start melee2 target=" + target);
		#endif

		m_Bot.ConsumeMeleeAttackRequest();
		return true;
	}

	//! Vanilla hit path: super.EvaluateHit applies one full hit (damage + blood/sound
	//! effects + block check via EvaluateHit_Player/EvaluateHit_Infected ->
	//! EvaluateHit_Common) using the valid hitZone from dmBotMeleeCombat.TargetSelection.
	//! For zombies we then apply the remaining (N-1)x multiplier deferred to the next
	//! frame (CallLater), so it does not collide with the per-frame damage dedup that
	//! made the old direct-damage loop ineffective.
	override protected void EvaluateHit(InventoryItem weapon)
	{
		super.EvaluateHit(weapon);

		EntityAI target = m_MeleeCombat.GetTargetEntity();
		if (target && ZombieBase.Cast(target))
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ApplyZombieExtraDamage, 0, false, target);
		}
	}

	//! Apply the remaining zombie damage multiplier (N-1)x, deferred by one frame.
	private void ApplyZombieExtraDamage(EntityAI target)
	{
		if (!target || !target.IsAlive())
			return;

		int weaponMode = m_MeleeCombat.GetWeaponMode();

		string ammoName;
		InventoryItem weapon = InventoryItem.Cast(m_Player.GetHumanInventory().GetEntityInHands());
		if (weapon)
			ammoName = weapon.GetMeleeCombatData().GetAmmoTypeName(weaponMode);
		else
			ammoName = m_Player.GetMeleeCombatData().GetAmmoTypeName(weaponMode);

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("dmBotMeleeFightLogic: ApplyZombieExtraDamage ammo=" + ammoName + " target=" + target);
		#endif

		target.ProcessDirectDamage(DamageType.CLOSE_COMBAT, m_Player, target.GetDefaultHitComponent(), ammoName, target.GetDefaultHitPosition(), DM_MELEE_DAMAGE_MULT_ZOMBIE - 1.0);
	}
}
