//! dmBotIntent_Emote — play a gesture/emote animation by EmoteConstants ID.
//!
//! PARALLEL + EMOTION channel: the emote is started once in OnStart and runs to
//! completion; it doesn't hold a per-tick channel (the action command owns the
//! body on the engine side). Full-body emotes (e.g. salute) still freeze the body
//! on the engine level even though the intent is PARALLEL — concurrent MOVE
//! intents simply have no effect while the action command is active. Completion
//! is detected via GetCommand_Action()/GetCommandModifier_Action() == null (NOT
//! IsEmotePlaying(), which never clears for a server AI). Cyclic emotes (dance)
//! never end on their own — the caller must set m_Deadline, otherwise the
//! auto-deadline DM_INTENT_MAX_AGE drops the intent.
class dmBotIntent_Emote : dmBotIntent
{
	int m_EmoteID = -1;

	void dmBotIntent_Emote()
	{
		m_Manage = dmBotIntentsChannel.EMOTION;
		m_Concurrency = dmBotIntentConcurrency.PARALLEL;
	}

	override string GetIntentName()
	{
		return "Emote";
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn || !pawn.PlayEmote(m_EmoteID))
		{
			Fail();
			return;
		}
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			Fail();
			return;
		}
		//! Finished when the emote action command ended (full-body -> GetCommand_Action
		//! null; additive -> GetCommandModifier_Action null). IsEmotePlaying() is
		//! unusable here — it stays true for a server AI.
		if (!pawn.GetCommand_Action() && !pawn.GetCommandModifier_Action())
			Finish();
	}
}
