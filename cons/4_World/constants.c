//! dmBotorama — base constants (world module).

//! Mod version (increment on every change so you can verify the loaded build).
static const string DM_BOTORAMA_VERSION = "2.3";

//! Default survivor model class used when spawning a bot.
static const string DM_DEFAULT_MODEL = "dmAI_SurvivorM_Denis";

//! Distance (meters) in front of the player at which a bot spawns.
static const float DM_SPAWN_DISTANCE = 1.0;

//! Approximate eye/head height above the feet (meters).
static const float DM_EYE_HEIGHT = 1.4;

//! Look turn smoothing speed (per second). Higher = faster head turn.
static const float DM_LOOK_TURN_SPEED = 10.0;

//! Maximum head look yaw offset from the body (degrees).
static const float DM_LOOK_MAX_YAW = 90.0;

//! Maximum head look pitch offset (degrees).
static const float DM_LOOK_MAX_PITCH = 85.0;
