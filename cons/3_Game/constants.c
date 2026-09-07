//! dmBotorama — game module constants (loaded first, visible to all modules).

//! Mod version (increment on every change so you can verify the loaded build).
//! Lives in the game module because dmBotLog (also game module) prints it, and
//! the game module compiles before the world module.
static const string DM_BOTORAMA_VERSION = "3.68";

//! Bullet-drop compensation: initial per-bot learnable coefficient (start ~0.8).
static const float DM_DROP_COEF_INIT = 0.8;
//! Bullet-drop compensation: learning rate of the coefficient update per miss.
static const float DM_DROP_LEARN_RATE = 0.6;
//! Bullet-drop compensation: clamp bounds of the learned coefficient.
static const float DM_DROP_COEF_MIN = 0.1;
static const float DM_DROP_COEF_MAX = 1.5;

//! Wind drift: fraction of the wind speed the bullet reaches laterally
//! (drag-limited). Empirically ~0.022 (0.38 m drift at 800 m / 12 m/s wind).
static const float DM_WIND_DRIFT_COEF = 0.022;

//! Ricochet filter: impacts slower than this (m/s) are ricochets, not the
//! bullet's first ground hit — ignore them in the feedback.
static const float DM_DROP_MIN_IMPACT_SPEED = 200.0;

//! Hearing: threat assigned to a target heard but not seen (below attack threshold 0.5).
static const float DM_NOISE_THREAT = 0.4;

//! Hearing: noise ranges (meters) per source, for the distance filter.
static const float DM_NOISE_GUNSHOT_STRENGTH = 3000.0;
static const float DM_NOISE_STEP_STRENGTH = 10.0;
static const float DM_NOISE_BULLETIMPACT_STRENGTH = 15.0;
static const float DM_NOISE_SCREAM_STRENGTH = 30.0;

//! Hearing: gunshot range (meters) when the weapon has a suppressor, by kind.
static const float DM_NOISE_GUNSHOT_SILENCED_PISTOL = 75.0;
static const float DM_NOISE_GUNSHOT_SILENCED_RIFLE = 100.0;
static const float DM_NOISE_GUNSHOT_SILENCED_HOMEMADE = 150.0;

//! Hearing: attractiveness (0..1) assigned to a heard target, by noise kind.
static const float DM_NOISE_ATTRACTIVENESS_NOISE = 0.6;
static const float DM_NOISE_ATTRACTIVENESS_SHOT = 0.9;