//! dmBotorama — server-side entry point.
//!
//! Delegates chat messages to the command manager. The command modules register
//! themselves from test/5_Mission (dmTestBootstrap) — core does not reference test.

modded class MissionServer
{
	override void OnEvent(EventType eventTypeId, Param params)
	{
		if (eventTypeId == ChatMessageEventTypeID)
		{
			ChatMessageEventParams chat = ChatMessageEventParams.Cast(params);
			if (chat)
			{
				string message = chat.param3;
				if (message.Length() > 0 && message.Substring(0, 1) == "/")
				{
					if (dmCommandManager.GetInstance().Execute(chat.param2, message))
						return;
				}
			}
		}

		super.OnEvent(eventTypeId, params);
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		//! Heartbeat for all bots (fixed-rate, accumulated inside TickAll).
		dmAISurvivor.TickAll(timeslice);
	}
}
