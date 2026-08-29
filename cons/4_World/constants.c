//! dmBotorama — base constants (world module).

//! Default survivor model class used when spawning a bot.
static const string DM_DEFAULT_MODEL = "dmAI_SurvivorM_Denis";

//! Distance (meters) in front of the player at which a bot spawns.
static const float DM_SPAWN_DISTANCE = 5.0;

//! Approximate eye/head height above the feet (meters).
static const float DM_EYE_HEIGHT = 1.4;

//! Look turn smoothing speed (per second). Higher = faster head turn.
static const float DM_LOOK_TURN_SPEED = 10.0;

//! Maximum head look yaw offset from the body (degrees).
static const float DM_LOOK_MAX_YAW = 90.0;

//! Maximum head look pitch offset (degrees).
static const float DM_LOOK_MAX_PITCH = 85.0;

//! Distance (meters) at which a patrol point counts as reached.
static const float DM_PATROL_REACH_DISTANCE = 1.0;

//! Time (seconds) the bot must dwell at a reached patrol point before advancing.
static const float DM_PATROL_DWELL_TIME = 5.0;

//! Default FSM preemption evaluation interval (seconds).
static const float DM_FSM_PREEMPT_INTERVAL = 0.25;

//! Fixed brain tick interval (seconds). The brain (dmAISurvivor.OnUpdate) is
//! driven by MissionServer.OnUpdate, which runs on the main loop at a variable
//! rate higher than the simulation; TickAll accumulates time and only runs the
//! brain at this fixed rate (~30 Hz) so it doesn't over-tick relative to the
//! pawn's simulation (see fstructure.md "Профилирование").
static const float DM_BOT_TICK_INTERVAL = 0.033;

//! Reduced-frequency interval (seconds) for the AI-bot body-modifier tick.
//! Vanilla ticks modifiers every frame for the selected player; for an AI bot we
//! tick them here (~4 Hz) — their own intervals are >= 0.35 s, so this is enough.
static const float DM_BOT_MODIFIER_TICK_INTERVAL = 0.25;

//! Movement speed index for "jog/run" (0=idle, 1=walk, 2=jog, 3=sprint). Used to
//! cap the bot's speed when the body can't sprint (stamina depleted / broken legs).
static const float DM_SPEED_IDX_JOG = 2.0;

//! Max body slide-turn rate while moving (degrees per second).
static const float DM_MOVE_TURN_RATE = 180.0;

//! Seconds without meaningful progress toward the target before MoveTo aborts.
static const float DM_MOVE_STUCK_TIME = 3.0;

//! Min distance decrease (meters) that counts as "progress" (resets the stuck timer).
static const float DM_MOVE_PROGRESS_EPS = 0.1;

//! Radius (meters) to snap the pathfinding target onto the navmesh.
static const float DM_PATH_SAMPLE_RADIUS = 2.0;

//! Reach radius (meters) for an intermediate path waypoint.
static const float DM_PATH_WAYPOINT_REACH = 0.5;

//! Max path recalculations on stuck before MoveTo aborts.
static const int DM_MOVE_MAX_RECALC = 1;

//! Stance transition timeout (seconds) for erect<->crouch.
static const float DM_STANCE_TIMEOUT_CROUCH = 0.3;

//! Stance transition timeout (seconds) for crouch<->prone.
static const float DM_STANCE_TIMEOUT_PRONE = 0.75;

//! Time (seconds) the bot stays prone at cover before exiting Stealth.
static const float DM_STEALTH_PRONE_DWELL_TIME = 300.0;

//! Movement speed thresholds (m/s) used by dmAISurvivor.CalcSpeed to map a
//! required velocity to a speed index (1=walk, 2=jog, 3=sprint).
static const float DM_SPEED_WALK = 1.4;
static const float DM_SPEED_JOG = 3.6;

//! Speed ramp rate (speed units per second) for smooth acceleration/deceleration.
static const float DM_MOVE_ACCEL_RATE = 6.0;

//! Turn angle (degrees) beyond which the bot slows down while moving (turn-slow).
static const float DM_MOVE_TURN_SLOW_THRESHOLD = 30.0;

//! Speed cap (0..3) applied while turning sharply.
static const float DM_MOVE_TURN_SLOW_SPEED = 1.0;

//! Max age (seconds) of an intent without a deadline before the arbitration
//! removes it (safety net — no intent lives forever).
static const float DM_INTENT_MAX_AGE = 300.0;

//! Directory for loadout files (relative to the DayZ profile root). A loadout
//! named "hunter" is stored at "$profile:dmBotorama/loadouts/hunter.json".
static const string DM_LOADOUT_DIR = "$profile:dmBotorama/loadouts";

//! Follow (escort): perpendicular side offset (meters) — the bot walks alongside
//! the player, not behind.
static const float DM_FOLLOW_SIDE_DISTANCE = 1.8;

//! Follow: distance to the alongside point at which the bot counts as "in place".
static const float DM_FOLLOW_REACH_DISTANCE = 1.0;

//! Follow: how far the alongside point may drift before re-aiming the MoveTo.
static const float DM_FOLLOW_RETARGET_DISTANCE = 2.0;

//! Follow: head-scan interval (seconds) and yaw range (±degrees) while escorting.
static const float DM_FOLLOW_LOOK_INTERVAL = 3.0;
static const float DM_FOLLOW_LOOK_RANGE = 90.0;
