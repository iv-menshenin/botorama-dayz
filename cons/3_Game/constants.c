//! dmBotorama — game module constants (loaded first, visible to all modules).

//! Mod version (increment on every change so you can verify the loaded build).
//! Lives in the game module because dmBotLog (also game module) prints it, and
//! the game module compiles before the world module.
static const string DM_BOTORAMA_VERSION = "2.9";
