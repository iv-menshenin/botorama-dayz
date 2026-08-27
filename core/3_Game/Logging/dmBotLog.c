//! dmBotLog — lightweight logging helpers.
//!
//! Debug/Trace always Print here. The expensive part is the string building at
//! the CALL SITE, and Enfusion does not optimize an empty function call away
//! like C++ would, so each call site is gated with its domain define
//! (DM_BOT_DEBUG_* / DM_BOT_TRACE_*, see botorama/cons/4_World/defines.c).
//! That compiles out both the call AND the string concatenation when the define
//! is off.
//!
//! Every line is prefixed with a wall-clock HH:MM:SS timestamp so log entries can
//! be correlated with the server/client RPT logs.

class dmBotLog
{
	//! Wall-clock timestamp "HH:MM:SS".
	static string TimeStamp()
	{
		int hour;
		int minute;
		int second;
		GetHourMinuteSecond(hour, minute, second);
		return hour.ToStringLen(2) + ":" + minute.ToStringLen(2) + ":" + second.ToStringLen(2);
	}

	static void Debug(string msg)
	{
		Print(TimeStamp() + " [dmBot] " + msg);
	}

	static void Trace(string msg)
	{
		Print(TimeStamp() + " [dmBot][trace] " + msg);
	}

	//! Always printed (not gated): error conditions worth surfacing in logs.
	static void Error(string msg)
	{
		Print(TimeStamp() + " [dmBot][error] " + msg);
	}

	//! Log the mod version once. Always printed (not gated by any DM_BOT_DEBUG_*),
	//! so the loaded mod version is visible in both server and client logs.
	static void LogVersion()
	{
		Print(TimeStamp() + " [dmBot] Botorama initialized: " + DM_BOTORAMA_VERSION);
	}
}
