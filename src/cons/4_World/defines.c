//! dmBotorama — compile-time logging switches.
//!
//! These are preprocessor defines. They are NOT turned on in any .c file here:
//! the DayZ preprocessor runs per-file, so a #define in one file is not visible
//! in another. To enable logging, add the define to the CfgMods block of the
//! module's config.cpp (src/<module>/config.cpp). The mod is split into one PBO
//! per module (cons/reg/core/map/loadout/test), and `defines[]` is duplicated in
//! the CfgMods of every module that contains gated call sites (core/map/loadout/test):
//!
//!   // src/core/config.cpp
//!   class CfgMods
//!   {
//!       class dmBotorama_Core
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
//!   DM_BOT_DEBUG_SPAWN   — spawn/despawn lifecycle.
//!   DM_BOT_DEBUG_BRAIN   — body control (orientation, failed look).
//!   DM_BOT_DEBUG_FSM     — FSM transitions and state entry/exit.
//!   DM_BOT_DEBUG_PAWN    — pawn/animation (variable binding, look sync).
//!   DM_BOT_DEBUG_BODY    — body mechanics (CE profile, corpse registration, blood/
//!                          shock/unconscious bridge, periodic body stats).
//!   DM_BOT_DEBUG_LOADOUT — loadout application (dmLoadoutApplier).
//!   DM_BOT_DEBUG_VISION  — perception scan (candidates, FOV/LOS results).
//!   DM_BOT_DEBUG_EVADE   — уворот от прицела (EvadeAim): триггер, свип укрытия, додж, конец.
//!   DM_BOT_DEBUG_MEDICAL — ИИ-лечение (MedicalCare): вход в состояние, шаги очереди,
//!                          применение эффекта, ошибки.
//!   DM_BOT_DEBUG_VOICE   — голосовые реплики бота (SpeakLine/TickTalking/PlayVoiceLineClient).
//!   DM_BOT_DEBUG_PATHFINDER — pathfinding (moving to goal)
//!   DM_BOT_DEBUG_PERFRAME_MOVING_LOG — per-frame movement speed/angle (ApplyMovement).
//!
//!   DM_WEAPON_DEBUG_FSM - debug weapon FSM events
//!
//!   DM_PERCEPTION_DEBUG - debug bots perception events
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
