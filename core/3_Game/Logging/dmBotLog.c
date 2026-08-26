//! dmBotLog — lightweight logging helpers.
//!
//! Debug/Trace always Print here. The expensive part is the string building at
//! the CALL SITE, and Enfusion does not optimize an empty function call away
//! like C++ would, so each call site is gated with #ifdef DM_BOT_DEBUG /
//! DM_BOT_TRACE (see botorama/cons/4_World/defines.c). That compiles out both
//! the call AND the string concatenation when the define is off.

class dmBotLog
{
	static void Debug(string msg)
	{
		Print("[dmBot] " + msg);
	}

	static void Trace(string msg)
	{
		Print("[dmBot][trace] " + msg);
	}

	//! Always printed (not gated): error conditions worth surfacing in logs.
	static void Error(string msg)
	{
		Print("[dmBot][error] " + msg);
	}

	//! Log the mod version once. Always printed (not gated by DM_BOT_DEBUG),
	//! so the loaded mod version is visible in both server and client logs.
	static void LogVersion()
	{
		Print("[dmBot] Botorama initialized: " + DM_BOTORAMA_VERSION);
	}
}
