//! dmBotIntent_MedicalAction — применить одно лечебное действие (бинт / шина /
//! таблетка) с анимацией. EXCLUSIVE+CRITICAL на канале MOVE — бот стоит во время
//! лечения. Жизненный цикл: prepare (в руки) -> animate (граф-команда) -> wait
//! (поллинг завершения + таймаут-фолбэк) -> apply (эффект + расход) -> Finish().
//!
//! Обезбол/витамины как БОНУСЫ здесь НЕ создаются — это отдельные шаги очереди
//! состояния (интент не создаёт другие интенты).

//! Тип лечебного действия (m_Kind).
enum dmMedicalActionKind
{
	ACTION_BANDAGE,  // перевязка (full-body CMD_ACTIONFB_BANDAGE)
	ACTION_SPLINT,   // шина (full-body CMD_ACTIONFB_CRAFTING)
	ACTION_PILL      // таблетка/витамины (additive CMD_ACTIONMOD_EAT_*)
}

class dmBotIntent_MedicalAction : dmBotIntent
{
	int m_Kind;                       // dmMedicalActionKind
	ItemBase m_Item;                  // предмет (null только для шины — спавн)
	int m_AnimCmd;                    // для PILL: CMD_ACTIONMOD_EAT_TABLET/EAT_PILL
	int m_Phase;                      // 0 prepare, 1 animate, 2 wait, 3 apply
	float m_PhaseTimer;
	HumanCommandActionCallback m_ActionCB;

	void dmBotIntent_MedicalAction()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
		m_Kind = dmMedicalActionKind.ACTION_BANDAGE;
		m_AnimCmd = 0;
		m_Phase = 0;
		m_PhaseTimer = 0.0;
		m_ActionCB = null;
	}

	override string GetIntentName()
	{
		return "MedicalAction";
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);
		m_Phase = 0;
		m_PhaseTimer = 0.0;
		m_ActionCB = null;
		bot.SetMove(0.0, 0.0);

		#ifdef DM_BOT_DEBUG_MEDICAL
		dmBotLog.Debug("[Medical] OnStart: m_Phase=" + m_Phase);
		#endif
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			#ifdef DM_BOT_DEBUG_MEDICAL
			dmBotLog.Debug("[Medical] OnUpdate: pawn is null");
			#endif
			Fail();
			return;
		}

		bot.SetMove(0.0, 0.0);

		if (m_Phase == 0)
			PhasePrepare(pawn);
		else if (m_Phase == 1)
			PhaseAnimate(pawn);
		else if (m_Phase == 2)
			PhaseWait(pawn, pDt);
		else
			PhaseApply(pawn);
	}

	//! Взять предмет в руки; шина при отсутствии — спавн в руки + плата.
	void PhasePrepare(dmAISurvivorBase pawn)
	{
		if (m_Kind == dmMedicalActionKind.ACTION_SPLINT)
		{
			if (m_Item)
			{
				if (!dmLoot.TakeToHands(pawn, m_Item))
				{
					#ifdef DM_BOT_DEBUG_MEDICAL
					dmBotLog.Debug("[Medical] TakeToHands FAILED: m_Phase=" + m_Phase + " m_Item=" + m_Item.GetType() + " ACTION_SPLINT");
					#endif
					Fail();
					return;
				}
			}
			else
			{
				ItemBase created = ItemBase.Cast(pawn.GetHumanInventory().CreateInHands("Splint"));
				if (!created)
				{
					#ifdef DM_BOT_DEBUG_MEDICAL
					dmBotLog.Debug("[Medical] CreateInHands FAILED: m_Phase=" + m_Phase + " m_Item=Splint");
					#endif
					Fail();
					return;
				}
				m_Item = created;
				dmLoot.PayForSplint(pawn);
			}
		}
		else
		{
			if (!m_Item || !dmLoot.TakeToHands(pawn, m_Item))
			{
				#ifdef DM_BOT_DEBUG_MEDICAL
				dmBotLog.Debug("[Medical] TakeToHands FAILED: m_Phase=" + m_Phase + " m_Item=" + m_Item.GetType());
				#endif
				Fail();
				return;
			}
		}

		m_Phase = 1;
		m_PhaseTimer = 0.0;
		#ifdef DM_BOT_DEBUG_MEDICAL
		dmBotLog.Debug("[Medical] PhasePrepare done: m_Phase=" + m_Phase);
		#endif
	}

	//! Проиграть анимацию (гейт: если уже идёт full-body/модификатор — пропустить).
	void PhaseAnimate(dmAISurvivorBase pawn)
	{
		if (pawn.GetCommand_Action() || pawn.GetCommandModifier_Action())
		{
			m_Phase = 2;
			m_PhaseTimer = 0.0;
			#ifdef DM_BOT_DEBUG_MEDICAL
			dmBotLog.Debug("[Medical] PhaseAnimate: m_Phase=" + m_Phase + " RETURN");
			#endif
			return;
		}

		if (m_Kind == dmMedicalActionKind.ACTION_BANDAGE)
			m_ActionCB = pawn.StartCommand_Action(DayZPlayerConstants.CMD_ACTIONFB_BANDAGE, dmBotActionAnimCB, DayZPlayerConstants.STANCEMASK_CROUCH);
		else if (m_Kind == dmMedicalActionKind.ACTION_SPLINT)
			m_ActionCB = pawn.StartCommand_Action(DayZPlayerConstants.CMD_ACTIONFB_CRAFTING, dmBotActionAnimCB, DayZPlayerConstants.STANCEMASK_CROUCH);
		else
			m_ActionCB = pawn.AddCommandModifier_Action(m_AnimCmd, dmBotActionAnimCB);

		m_Phase = 2;
		m_PhaseTimer = 0.0;
		#ifdef DM_BOT_DEBUG_MEDICAL
		dmBotLog.Debug("[Medical] PhaseAnimate: m_Phase=" + m_Phase);
		#endif
	}

	//! Ждать завершения команды (или таймаут-фолбэк — full-body у ИИ не подтверждён).
	void PhaseWait(dmAISurvivorBase pawn, float pDt)
	{
		m_PhaseTimer = m_PhaseTimer + pDt;
		bool animDone = !pawn.GetCommand_Action() && !pawn.GetCommandModifier_Action();
		if (animDone || m_PhaseTimer >= DM_MEDICAL_ANIM_TIMEOUT)
			m_Phase = 3;
	}

	//! Применить эффект + расход, завершить интент.
	void PhaseApply(dmAISurvivorBase pawn)
	{
		if (m_Kind == dmMedicalActionKind.ACTION_BANDAGE)
			ApplyBandage(pawn);
		else if (m_Kind == dmMedicalActionKind.ACTION_SPLINT)
			ApplySplint(pawn);
		else
			ApplyPill(pawn);

		Finish();
	}

	void ApplyBandage(dmAISurvivorBase pawn)
	{
		if (!m_Item)
			return;

		#ifdef DM_BOT_DEBUG_MEDICAL
		dmBotLog.Debug("[Medical] ApplyBandage: m_Phase=" + m_Phase);
		#endif
		pawn.GetBleedingManagerServer().RemoveMostSignificantBleedingSourceEx(m_Item);
		if (m_Item.HasQuantity())
			m_Item.AddQuantity(-1, true);
		else
			m_Item.Delete();
	}

	void ApplySplint(dmAISurvivorBase pawn)
	{

		#ifdef DM_BOT_DEBUG_MEDICAL
		dmBotLog.Debug("[Medical] ApplySplint: m_Phase=" + m_Phase);
		#endif
		pawn.ApplySplint();
		if (pawn.GetBrokenLegs() == eBrokenLegs.BROKEN_LEGS)
		{
			pawn.SetBrokenLegs(eBrokenLegs.BROKEN_LEGS_SPLINT);
			pawn.GetInventory().CreateInInventory("Splint_Applied");
		}
		if (m_Item)
			m_Item.Delete();
	}

	void ApplyPill(dmAISurvivorBase pawn)
	{
		if (!m_Item)
			return;

		#ifdef DM_BOT_DEBUG_MEDICAL
		dmBotLog.Debug("[Medical] ApplyPill: m_Phase=" + m_Phase);
		#endif
		Edible_Base edible = Edible_Base.Cast(m_Item);
		if (edible)
			edible.Consume(1.0, pawn);
	}

	override void OnCancel(dmAISurvivor bot)
	{
		super.OnCancel(bot);
		StopAction(bot);
		bot.SetMove(0.0, 0.0);
	}

	void StopAction(dmAISurvivor bot)
	{
		if (!m_ActionCB)
			return;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
		{
			if (pawn.GetCommandModifier_Action() == m_ActionCB)
				pawn.DeleteCommandModifier_Action(m_ActionCB);
			else if (pawn.GetCommand_Action() == m_ActionCB)
				m_ActionCB.Cancel();   // full-body (бинт/шина) — отменить вручную
		}
		m_ActionCB = null;
	}
}
