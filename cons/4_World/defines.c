//! dmBotorama — compile-time logging switches.
//!
//! These are preprocessor defines. They are NOT turned on in any .c file here:
//! the DayZ preprocessor runs per-file, so a #define in one file is not visible
//! in another. To enable logging, add the define to this mod's CfgMods block in
//! config.cpp:
//!
//!   class CfgMods
//!   {
//!       class dmBotorama
//!       {
//!           ...
//!           defines[] = { "DM_BOT_PROFILE", "DM_BOT_DEBUG_FSM" };
//!       };
//!   };
//!
//! Logging is split into fine-grained, per-domain switches so you can enable only
//! the area you are debugging. Each call site is gated with its own domain define.
//!
//! DEBUG — discrete events (spawn, transitions, sync):
//!   DM_BOT_DEBUG_SPAWN — spawn/despawn lifecycle.
//!   DM_BOT_DEBUG_BRAIN — body control (orientation, failed look).
//!   DM_BOT_DEBUG_FSM   — FSM transitions and state entry/exit.
//!   DM_BOT_DEBUG_PAWN  — pawn/animation (variable binding, look sync).
//!
//! TRACE — per-frame, verbose:
//!   DM_BOT_TRACE_LOOK — head look steering (LookAtPoint, UpdateLook).
//!
//! PROFILING:
//!   DM_BOT_PROFILE — accumulate per-frame timings; dump via "/prof dump".
//!
//! Gating is done at the CALL SITE, not inside dmBotLog:
//!
//!   #ifdef DM_BOT_DEBUG_SPAWN
//!   dmBotLog.Debug("...");
//!   #endif
//!
//! Enfusion does not optimize an empty function call away (unlike C++), and the
//! string building in the arguments is the expensive part, so wrapping the call
//! itself in #ifdef compiles out both the call AND the concatenation when the
//! define is off. The dmBotLog.Debug/Trace methods themselves always Print.
//!
//! Note: dmBotLog.Error is NOT gated — error conditions always print.
