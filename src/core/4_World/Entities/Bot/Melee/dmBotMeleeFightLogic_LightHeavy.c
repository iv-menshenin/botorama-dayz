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
				if ( w.HasBayonetAttached() )
					return EMeleeHitType.WPN_STAB;
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

		//! Блок: подняты руки + желаемое движение назад → SetMeleeBlock (визуальная
		//! поза) + SetBlock (скриптовый флаг, по которому EvaluateHit_Player гасит
		//! входящий урон при развороте к атакующему). Выполняется каждый кадр, до
		//! проверки request. Читаем ЖЕЛАЕМЫЙ угол движения (m_DesiredMoveAngle из
		//! SetMove), а не нативный фильтрованный угол — при упоре в препятствие
		//! фактическое смещение 0, но желание «назад» должно давать блок.
		HumanCommandMove hcm = m_Player.GetCommand_Move();
		if (m_Bot && hcm)
		{
			float desiredAngle = m_Bot.GetDesiredMoveAngle();
			bool shouldBlock = m_Bot.IsRaised() && (desiredAngle < -130.0 || desiredAngle > 130.0);

			#ifdef DM_BOT_DEBUG_FIGHT
			if (IsInBlock() != shouldBlock)
				dmBotLog.Debug("dmBotMeleeFightLogic: block=" + shouldBlock + " raised=" + m_Bot.IsRaised() + " angle=" + desiredAngle);
			#endif

			hcm.SetMeleeBlock(shouldBlock);
			SetBlock(shouldBlock);
		}

		//! Комбо: пока MELEE2 активен, цель в зоне комбо и CanFightCombo — продолжаем
		//! серию через ванильный HandleComboHit (выбор типа удара + ContinueCombo +
		//! снятие стамины). Идёт до проверки request: серия не зависит от нового
		//! запроса мозга, первый StartCommand_Melee2 уже запустил команду.
		if (pCurrentCommandID == DayZPlayerConstants.COMMANDID_MELEE2 && m_Bot && m_Bot.CanFightCombo())
		{
			HumanCommandMelee2 hmc2 = m_Player.GetCommand_Melee2();
			if (hmc2 && hmc2.IsInComboRange())
			{
				#ifdef DM_BOT_DEBUG_FIGHT
				dmBotLog.Debug("dmBotMeleeFightLogic: combo hit");
				#endif

				return HandleComboHit(pCurrentCommandID, pInputs, itemInHands, pMovementState, pContinueAttack);
			}
		}

		if (!m_Bot || !m_Bot.HasMeleeAttackRequest())
			return false;

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("dmBotMeleeFightLogic: melee request");
		#endif

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
		EntityAI target = m_MeleeCombat.GetTargetEntity();

		#ifdef DM_BOT_DEBUG_DAMAGE
		if (target)
			dmBotLog.Debug("EvaluateHit called target=" + target.GetType() + " hitZone=" + m_MeleeCombat.GetHitZoneIdx());
		else
			dmBotLog.Debug("EvaluateHit called target=null");
		#endif

		super.EvaluateHit(weapon);

		if (target && ZombieBase.Cast(target))
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ApplyZombieExtraDamage, 0, false, target);
		}
	}

	//! Vanilla EvaluateHit_Player derives the block angle from the target's
	//! INPUT-CONTROLLER heading (MiscGameplayFunctions.GetHeadingVector ->
	//! GetInputController().GetHeadingAngle()). An AI bot turns its BODY through
	//! SetOrientation/foot-step without syncing the input-controller heading, so
	//! that heading stays stale and hitAngle > PVP_MAX_BLOCKABLE_ANGLE -> the block
	//! never cancels damage. Here the angle is taken from the PHYSICAL body
	//! orientation (GetOrientation()[0] = yaw in degrees, 0 = north), same formula.
	override protected void EvaluateHit_Player(InventoryItem weapon, Object target)
	{
		int weaponMode = m_MeleeCombat.GetWeaponMode();
		int forcedWeaponMode = -1;
		bool forcedDummy = false;

		PlayerBase targetPlayer = PlayerBase.Cast(target);
		if (targetPlayer)
		{
			vector targetPos = targetPlayer.GetPosition();
			vector agressorPos = m_Player.GetPosition();

			float yawRad = targetPlayer.GetOrientation()[0] * Math.DEG2RAD;
			vector targetForward = Vector(Math.Cos(yawRad + Math.PI_HALF), 0.0, Math.Sin(yawRad + Math.PI_HALF));
			float hitAngle = Math.RAD2DEG * Math.AbsFloat(Math3D.AngleFromPosition(targetPos, targetForward, agressorPos));

			bool inBlock = targetPlayer.GetMeleeFightLogic() && targetPlayer.GetMeleeFightLogic().IsInBlock();
			if (inBlock && hitAngle <= GameConstants.PVP_MAX_BLOCKABLE_ANGLE)
			{
				if (weaponMode > 0)
					forcedWeaponMode = --weaponMode;
				else
					forcedDummy = true;
			}

			#ifdef DM_BOT_DEBUG_DAMAGE
			dmBotLog.Debug("EvaluateHit_Player hitAngle=" + hitAngle + " inBlock=" + inBlock);
			dmBotLog.Debug("EvaluateHit_Player forcedDummy=" + forcedDummy + " forcedWeaponMode=" + forcedWeaponMode);
			#endif
		}

		EvaluateHit_Common(weapon, target, forcedDummy, forcedWeaponMode);
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
