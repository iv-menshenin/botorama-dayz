//! dmBotIntent_Untie — free the bot from restraints (rope/handcuffs) with the
//! "struggle" animation. EXCLUSIVE + CRITICAL on the EMOTION channel so it does not
//! fight the concurrent escape MOVE intent over the movement channel. Lifecycle:
//! OnStart plays the full-body looping struggle command
//! (CMD_ACTIONFB_RESTRAINEDSTRUGGLE), then a fixed DM_UNTIE_DURATION timer runs
//! before the bot is released — SetRestrained(false) + the restraint item in hands
//! is removed and the original item spawned on the ground (mirrors Expansion
//! eAI_Unrestrain; vanilla TransformRestrainItem goes through SERVER-mode hand
//! replacement that does not place items for a server AI without a client).
class dmBotIntent_Untie : dmBotIntent
{
	float m_Timer;
	HumanCommandActionCallback m_ActionCB;

	void dmBotIntent_Untie()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.EMOTION;
	}

	override string GetIntentName()
	{
		return "Untie";
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			Fail();
			return;
		}

		//! Full-body looping struggle animation (valid erect/crouch/prone per the
		//! vanilla debug registration). If it doesn't start, abort.
		m_ActionCB = pawn.StartCommand_Action(DayZPlayerConstants.CMD_ACTIONFB_RESTRAINEDSTRUGGLE, dmBotActionAnimCB, DayZPlayerConstants.STANCEMASK_ALL);
		if (!m_ActionCB)
		{
			Fail();
			return;
		}

		m_Timer = DM_UNTIE_DURATION;
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		m_Timer -= pDt;
		if (m_Timer > 0.0)
			return;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			Fail();
			return;
		}

		StopStruggle(pawn);

		pawn.SetRestrained(false);

		//! Remove the restraint item from the bot's hands and spawn the original
		//! item on the ground (mirrors Expansion eAI_Unrestrain).
		EntityAI item = pawn.GetItemInHands();
		if (item)
		{
			string newItemName = item.ConfigGetString("OnRestrainChange");
			if (newItemName != "")
			{
				item.DeleteSafe();
				pawn.OnItemInHandsChanged();
				GetGame().CreateObjectEx(newItemName, pawn.GetPosition(), ECE_PLACE_ON_SURFACE);
			}
			else
			{
				item.DeleteSafe();
				pawn.OnItemInHandsChanged();
			}
		}

		Finish();
	}

	override void OnCancel(dmAISurvivor bot)
	{
		super.OnCancel(bot);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
			StopStruggle(pawn);
	}

	void StopStruggle(dmAISurvivorBase pawn)
	{
		if (!m_ActionCB)
			return;

		if (pawn.GetCommand_Action() == m_ActionCB)
			m_ActionCB.Cancel();
		m_ActionCB = null;
	}
}
