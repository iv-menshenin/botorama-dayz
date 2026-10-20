//! dmTestBootstrap — registers the test command modules into the command manager.
//!
//! Lives in test/5_Mission because the modules (dmBotCommand/dmFSMCommand/dmTestCommand)
//! are defined here, and the module load order is cons -> core -> test.
//!
//! Also owns the server-side test hooks (OnEvent -> chat command dispatch) and
//! kicks the e2e bridge, which self-schedules its tick via CallLater. They are
//! kept in this single modded class MissionServer block because a second modded
//! class MissionServer in another file of the same module silently drops the
//! first file's overrides (DayZ 1.30 file-order sensitivity). The bridge does NOT
//! ride OnUpdate here: in 1.30, two modded class MissionServer blocks across
//! modules both overriding OnUpdate silently drop one of them (core wins, test
//! loses), so the bridge schedules its own repeating CallLater instead.

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		dmBotLog.LogVersion();

		dmCommandManager mgr = dmCommandManager.GetInstance();
		mgr.Register(new dmBotCommand());
		mgr.Register(new dmFSMCommand());
		mgr.Register(new dmTestCommand());
		mgr.Register(new dmProfCommand());
		mgr.Register(new dmTpCommand());
		mgr.Register(new dmShowCommand());
		mgr.Register(new dmLaunchCommand());
		mgr.Register(new dmPoiCommand());
		mgr.Register(new dmSpawnCityCommand());

		//! Kick the e2e bridge: it self-schedules its repeating CallLater tick
		//! (OnUpdate is unusable here — 1.30 cross-module override conflict).
		dmE2EBridge.Get();
	}

	override void OnEvent(EventType eventTypeId, Param params)
	{
		if (eventTypeId == ChatMessageEventTypeID)
		{
			ChatMessageEventParams chat = ChatMessageEventParams.Cast(params);
			if (chat)
			{
				string message = chat.param3;
				if (message.Length() > 0 && message.Substring(0, 1) == DM_COMMAND_PREFIX)
				{
					if (dmCommandManager.GetInstance().Execute(chat.param2, message))
						return;
				}
			}
		}

		super.OnEvent(eventTypeId, params);
	}
}
