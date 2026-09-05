//! dmBotMeleeFightLogic_LightHeavy — bot melee driver (replaces the old stub).
//!
//! The vanilla DayZPlayerMeleeFightLogic_LightHeavy.HandleFightLogic null-derefs
//! hcm (HumanCommandMove) outside the MOVE command, and CanFight() is always true
//! for an AI bot (no ActionManager), so the vanilla code throws a VM exception.
//! This subclass drives a single light/heavy attack per request from the brain,
//! targeting the entity chosen by dmBotMeleeCombat (no raycast, no raised stance).

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

		#ifdef DM_BOT_DEBUG_FSM
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

		m_Player.StartCommand_Melee2(target, m_HitType == EMeleeHitType.HEAVY, 1.0, m_MeleeCombat.GetHitPos());

		if (m_HitType == EMeleeHitType.HEAVY)
			m_Player.DepleteStamina(EStaminaModifiers.MELEE_HEAVY);
		else
			m_Player.DepleteStamina(EStaminaModifiers.MELEE_LIGHT);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("dmBotMeleeFightLogic: start melee2 target=" + target);
		#endif

		m_Bot.ConsumeMeleeAttackRequest();
		return true;
	}

	//! Apply melee damage by component NAME (no component index), since the magic
	//! target selection sets hitZoneIdx = -1. GetDefaultHitComponent() returns the
	//! damage-zone name (e.g. "Torso"), which ProcessMeleeHitName expects.
	override protected void EvaluateHit(InventoryItem weapon)
	{
		EntityAI target = m_MeleeCombat.GetTargetEntity();
		if (!target)
			return;

		string compName = target.GetDefaultHitComponent();
		int weaponMode = m_MeleeCombat.GetWeaponMode();
		vector hitPos = m_MeleeCombat.GetHitPos();

		//! Zombies take DM_MELEE_DAMAGE_MULT_ZOMBIE hits per strike — the magic
		//! melee has no raycast, so the damage is applied directly by name.
		int mult = 1;
		if (ZombieBase.Cast(target))
			mult = DM_MELEE_DAMAGE_MULT_ZOMBIE;
		int i;
		for (i = 0; i < mult; i++)
			m_Player.ProcessMeleeHitName(weapon, weaponMode, target, compName, hitPos);
	}
}
