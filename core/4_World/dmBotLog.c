//! dmBotLog — lightweight logging helpers.
//!
//! Calls are compiled out unless DM_BOT_DEBUG / DM_BOT_TRACE are defined
//! (see botorama/cons/4_World/defines.c).

class dmBotLog
{
	static void Debug(string msg)
	{
#ifdef DM_BOT_DEBUG
		Print("[dmBot] " + msg);
#endif
	}

	static void Trace(string msg)
	{
#ifdef DM_BOT_TRACE
		Print("[dmBot][trace] " + msg);
#endif
	}

	//! Log the mod version once. Always printed (not gated by DM_BOT_DEBUG),
	//! so the loaded mod version is visible in both server and client logs.
	static void LogVersion()
	{
		Print("[dmBot] Botorama initialized: " + DM_BOTORAMA_VERSION);
	}
}
