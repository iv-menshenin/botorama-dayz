//! dmBotorama — game module constants (loaded first, visible to all modules).

//! Mod version (increment on every change so you can verify the loaded build).
//! Lives in the game module because dmBotLog (also game module) prints it, and
//! the game module compiles before the world module.
static const string DM_BOTORAMA_VERSION = "3.01";

//! Hearing: threat assigned to a target heard but not seen (below attack threshold 0.5).
static const float DM_NOISE_THREAT = 0.4;

//! Hearing: noise ranges (meters) per source, for the distance filter.
static const float DM_NOISE_GUNSHOT_STRENGTH = 100.0;
static const float DM_NOISE_STEP_STRENGTH = 10.0;
static const float DM_NOISE_BULLETIMPACT_STRENGTH = 20.0;
static const float DM_NOISE_SCREAM_STRENGTH = 30.0;