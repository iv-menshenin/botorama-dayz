//! dmBotorama — compile-time logging switches.
//!
//! These are preprocessor defines. They are NOT turned on in any .c file here:
//! the DayZ preprocessor runs per-file, so a #define in one file is not visible
//! in another. To enable logging, add the define to this mod's CfgMods block:
//!
//!   class CfgMods
//!   {
//!       class dmBotorama
//!       {
//!           ...
//!           defines[] = { "DM_BOT_DEBUG" };    // basic logging
//!           // defines[] = { "DM_BOT_TRACE" }; // verbose logging
//!       };
//!   };
//!
//! Available defines:
//!   DM_BOT_DEBUG — basic debug logging (spawn/despawn, chat commands).
//!   DM_BOT_TRACE — verbose trace logging.
//!   DM_BOT_PROFILE — accumulate per-frame timings (dmBotProfiler); dump via "/prof dump".
//!
//! Gating is done at the CALL SITE, not inside dmBotLog:
//!
//!   #ifdef DM_BOT_DEBUG
//!   dmBotLog.Debug("...");
//!   #endif
//!
//! Enfusion does not optimize an empty function call away (unlike C++), and the
//! string building in the arguments is the expensive part, so wrapping the call
//! itself in #ifdef compiles out both the call AND the concatenation when the
//! define is off. The dmBotLog.Debug/Trace methods themselves always Print.
