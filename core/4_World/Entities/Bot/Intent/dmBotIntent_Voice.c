//! dmBotIntent_Voice — произнести голосовую реплику (VOICE-канал, EXCLUSIVE).
//! Играет реплику один раз, когда выигрывает канал, затем завершается.
class dmBotIntent_Voice : dmBotIntent
{
	int m_LineId;

	void dmBotIntent_Voice()
	{
		m_Manage = dmBotIntentsChannel.VOICE;
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.DESIRABLE;
	}

	override string GetIntentName()
	{
		return "Voice";
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
			pawn.SpeakLine(m_LineId);
		Finish();
	}
}
