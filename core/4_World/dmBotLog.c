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
}
