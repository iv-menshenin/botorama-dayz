//! dmBotIntent_SitByFireplace — walk to a burning fireplace and sit next to it.
//!
//! MoveTo subclass: it walks to a sit point near the fire (offset m_SitDist from
//! the fireplace), then faces the fire and plays a cyclic sit emote. It holds the
//! sit for DM_SIT_BY_FIRE_TIME, then stops the emote and finishes. EXCLUSIVE so a
//! parallel MOVE intent can't shove the bot while sitting.
class dmBotIntent_SitByFireplace : dmBotIntent_MoveTo
{
	FireplaceBase m_Fireplace;
	float m_SitDist = DM_SIT_BY_FIRE_DIST_CLOSE;
	int m_EmoteID = EmoteConstants.ID_EMOTE_CAMPFIRE;
	bool m_Sitting = false;
	float m_SitTimer = 0.0;

	void dmBotIntent_SitByFireplace()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "SitByFireplace";
	}

	override void OnStart(dmAISurvivor bot)
	{
		vector firePos = m_Fireplace.GetPosition();
		vector botPos = bot.GetPosition();
		vector dir = botPos - firePos;
		dir[1] = 0.0;
		if (dir.Length() < 0.01)
			dir = Vector(0.0, 0.0, 1.0);
		else
			dir.Normalize();
		m_Goal = firePos + dir * m_SitDist;
		m_ReachDistance = DM_SIT_BY_FIRE_REACH;
		super.OnStart(bot);
	}

	override void OnReachedGoal(dmAISurvivor bot, vector pos)
	{
		bot.SetMove(0.0, 0.0);
		bot.FacePoint(m_Fireplace.GetPosition());

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn || !pawn.PlayEmote(m_EmoteID))
		{
			Fail();
			return;
		}

		m_Sitting = true;
		m_SitTimer = DM_SIT_BY_FIRE_TIME;
		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] SitByFireplace: sit emote=" + m_EmoteID);
		#endif
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		if (m_Sitting)
		{
			m_SitTimer -= pDt;
			if (m_SitTimer <= 0.0)
			{
				StopSitting(bot);
				Finish();
			}
			return;
		}
		super.OnUpdate(bot, pDt);
	}

	override void OnCancel(dmAISurvivor bot)
	{
		StopSitting(bot);
		super.OnCancel(bot);
	}

	//! Stop the cyclic sit emote so the bot stands back up. For a server AI there
	//! is no client, so ServerRequestEmoteCancel() (which pushes a sync-juncture to
	//! a remote that doesn't exist) is a no-op; we interrupt the gesture action
	//! command directly instead.
	void StopSitting(dmAISurvivor bot)
	{
		if (!m_Sitting)
			return;
		m_Sitting = false;

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return;

		HumanCommandActionCallback action = pawn.GetCommand_Action();
		if (action)
			action.Cancel();
		HumanCommandActionCallback modifier = pawn.GetCommandModifier_Action();
		if (modifier)
			modifier.Cancel();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] SitByFireplace: stop sit");
		#endif
	}
}
