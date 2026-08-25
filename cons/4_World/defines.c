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
