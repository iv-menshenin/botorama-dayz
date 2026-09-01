//! dmBotorama — base constants (world module).

//! Default survivor model class used when spawning a bot.
static const string DM_DEFAULT_MODEL = "dmAI_SurvivorM_Denis";

//! Distance (meters) in front of the player at which a bot spawns.
static const float DM_SPAWN_DISTANCE = 5.0;

//! Approximate eye/head height above the feet (meters).
static const float DM_EYE_HEIGHT = 1.4;

//! Look turn smoothing speed (per second). Higher = faster head turn.
static const float DM_LOOK_TURN_SPEED = 3.0;

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
static const float DM_PATH_WAYPOINT_REACH = 0.15;

//! Raycast distance (meters) straight ahead at eye level to detect a closed door.
static const float DM_DOOR_OPEN_DIST = 2.0;

//! Throttle interval (seconds) for the proactive door check in MoveTo.
static const float DM_DOOR_CHECK_INTERVAL = 0.5;

//! Distance (meters) from the door the bot backs up to before opening it, so the
//! swinging door doesn't push it (see dmBotIntent_OpenDoor).
static const float DM_DOOR_STEP_BACK_DIST = 0.75;

//! Max time (seconds) the bot backs away before opening the door anyway.
static const float DM_DOOR_STEP_BACK_TIMEOUT = 3.0;

//! Max time (seconds) to wait for the door to fully open before giving up.
static const float DM_DOOR_OPEN_TIMEOUT = 3.0;

//! Stepback (before door opening) speed.
static const float DM_DOOR_OPEN_STEPBACK_SPEED = 2.0;

//! Distance (meters) to the ladder entry point at which the bot attaches to the
//! ladder (starts climbing).
static const float DM_LADDER_ATTACH_DIST = 2.0;

//! Grace period (seconds) after starting the ladder command before UseLadder
//! trusts IsClimbingLadder() — gives the ladder command time to become active.
static const float DM_LADDER_ATTACH_GRACE = 0.5;

//! Grace period (seconds) after starting a vault/climb before MoveTo checks
//! IsClimbing() — gives the climb command time to become active.
static const float DM_VAULT_GRACE = 1.0;

//! Time (seconds) the bot steps back/sideways per stuck-recovery attempt before
//! re-routing.
static const float DM_MOVE_RECOVER_TIME = 1.0;

//! Max stuck-recovery attempts (step back/sideways + re-route) before MoveTo aborts.
static const int DM_MOVE_MAX_RECOVER = 2;

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

//! Follow (escort): distance beyond which the bot enters the Follow state (via
//! dmBotCondition_FollowFar). Players may lead farther than other entities.
static const float DM_FOLLOW_THRESHOLD_PLAYER = 5.0;
static const float DM_FOLLOW_THRESHOLD_OTHER = 1.0;

//! Follow: distance to the target at which the bot counts as "in place" (anchor —
//! stops this far short of the target instead of colliding with it).
static const float DM_FOLLOW_REACH = 1.0;

//! Follow: lateral offset (meters) of the escort anchor from the target — the
//! shoulder for a player/bot (±, by m_SideSign), or this far short of an item
//! along the approach.
static const float DM_FOLLOW_SIDE_DISTANCE = 1.0;

//! Follow: velocity extrapolation multiplier (× target velocity) for the escort
//! anchor of a moving player/bot — the anchor leads this far ahead of the target.
static const float DM_FOLLOW_VEL_MULTIPLIER = 0.75;

//! Follow: randomized lateral offset (meters) of the escort anchor for a moving
//! target, rolled once per Follow entry (m_SideDistance) between MIN and MAX.
static const float DM_FOLLOW_SIDE_DISTANCE_MIN = 1.5;
static const float DM_FOLLOW_SIDE_DISTANCE_MAX = 3.0;

//! Follow: distance bands (meters) that scale the escort's preferred speed by
//! distance to the target — sprint beyond SPRINT, jog beyond JOG, walk otherwise.
static const float DM_FOLLOW_SPRINT_DISTANCE = 15.0;
static const float DM_FOLLOW_JOG_DISTANCE = 10.0;

//! Follow: exit-window "stationary" radius (meters). The exit timer resets when
//! the target moves farther than this from its reference point; the escort EXITs
//! once the target stays put for DM_FOLLOW_EXIT_TIME with the bot within reach.
static const float DM_FOLLOW_EXIT_DISTANCE = 1.0;
static const float DM_FOLLOW_EXIT_TIME = 3.0;

//! Follow: seconds the target may stay out of sight before the bot refreshes the
//! target's last known position ("magic" re-aim of the catch-up MoveTo).
static const float DM_FOLLOW_LOST_SIGHT_TIME = 60.0;

//! Follow (FollowTo): distance to the anchor (meters) beyond which the escort sprints.
static const float DM_FOLLOW_SPRINT_GAP = 7.5;

//! Follow (FollowTo): distance to the anchor (meters) beyond which the escort jogs
//! (below it — walks).
static const float DM_FOLLOW_JOG_GAP = 5.0;

//! Follow (FollowTo): path re-computation interval (seconds), capped at 1 Hz.
static const float DM_FOLLOW_PATH_INTERVAL = 1.0;

//! Follow (FollowTo): target-velocity smoothing factor (0..1; higher = snappier).
static const float DM_FOLLOW_VEL_SMOOTH = 0.3;

//! Scan: random interval (seconds) between idle direction changes.
static const float DM_SCAN_INTERVAL_MIN = 10.0;
static const float DM_SCAN_INTERVAL_MAX = 30.0;

//! Scan: head turn angle range (±degrees from the body).
static const float DM_SCAN_ANGLE_MIN = 5.0;
static const float DM_SCAN_ANGLE_MAX = 35.0;

//! Scan: how long the head holds a new direction before recentering (seconds).
static const float DM_SCAN_HOLD_MIN = 5.0;
static const float DM_SCAN_HOLD_MAX = 15.0;

//! Scan: body turn angle range (±degrees), used when body turning is allowed.
static const float DM_SCAN_BODY_ANGLE_MIN = 15.0;
static const float DM_SCAN_BODY_ANGLE_MAX = 120.0;

//! Perception: scan radius (meters) around the bot for zombies.
static const float DM_PERCEPTION_ZOMBIE_RADIUS = 75.0;

//! Perception: scan radius (meters) around the bot for animals.
static const float DM_PERCEPTION_ANIMAL_RADIUS = 200.0;

//! Perception: scan radius (meters) around the bot for players/bots.
static const float DM_PERCEPTION_PLAYER_RADIUS = 1000.0;

//! Perception: horizontal field of view (degrees). A target within ±half this angle
//! from the look direction counts as "in front".
static const float DM_PERCEPTION_FOV = 120.0;

//! Perception: discovery scan interval (seconds) — how often the bot opens new
//! targets from the registry (radius check only; no FOV/LOS here).
static const float DM_PERCEPTION_BOX_INTERVAL = 1.0;

//! Perception: per-target LOS refresh intervals (seconds). The LOS pass runs every
//! tick and re-checks each target at its own cadence: creatures and friendly targets
//! are cheap to keep fresh, high-threat targets re-check fastest, low-threat slower.
static const float DM_PERCEPTION_REFRESH_CREATURE = 0.25;
static const float DM_PERCEPTION_REFRESH_FRIENDLY = 0.3;
static const float DM_PERCEPTION_REFRESH_LOW_THREAT = 0.2;
static const float DM_PERCEPTION_REFRESH_HIGH_THREAT = 0.1;

//! Target evaluation: threat (0..1) — how dangerous a sighted entity is.
//! The player is scored low so bots don't auto-attack them by default.
static const float DM_TARGET_THREAT_PLAYER = 0.1;
static const float DM_TARGET_THREAT_ZOMBIE = 0.3; // обнаруженный зомби не враждебен; враждебность только через урон
static const float DM_TARGET_THREAT_ANIMAL = 0.2;

//! Threat assigned after taking damage (0..1) — угроза после нанесённого урона.
//! See dmAISurvivor.RegisterDamageThreat: an attacker is treated as hostile the
//! instant it lands a hit, even outside the vision FOV. High vs low is picked by
//! the hit's health damage against DM_DAMAGE_THREAT_HP_THRESHOLD.
static const float DM_DAMAGE_THREAT_HIGH = 0.9;
static const float DM_DAMAGE_THREAT_LOW = 0.8;

//! Damage-threat threshold (HP damage) below which a hit registers low threat
//! (DM_DAMAGE_THREAT_LOW) instead of high (DM_DAMAGE_THREAT_HIGH).
static const float DM_DAMAGE_THREAT_HP_THRESHOLD = 30.0;

//! Attack: m_Threat above this threshold marks a target as hostile. Matches the
//! boundary between the fast and slow LOS-refresh cadences in GetRefreshTime.
static const float DM_ATTACK_THREAT_THRESHOLD = 0.5;

//! Target evaluation: attractiveness (0..1) — how interesting a target is.
static const float DM_TARGET_ATTRACT_PLAYER = 0.1;
static const float DM_TARGET_ATTRACT_ZOMBIE = 0.2;
static const float DM_TARGET_ATTRACT_ANIMAL = 0.5;

//! Target memory: seconds without contact before a remembered target is forgotten.
static const float DM_TARGET_FORGET_TIME = 300.0;

//! Melee: cooldown (seconds) between the bot's strikes.
static const float DM_MELEE_COOLDOWN = 0.6;

//! Melee: allowed body-facing error (degrees) before a strike is thrown.
static const float DM_MELEE_FACE_ANGLE = 15.0;

//! Melee: damage multiplier applied per strike against zombies.
static const int DM_MELEE_DAMAGE_MULT_ZOMBIE = 2;

//! Melee: fallback strike reach (meters) when the weapon reach can't be read.
static const float DM_MELEE_REACH = 1.5;

//! Fighting: seconds between target re-resolution (re-pick the nearest hostile).
static const float DM_FIGHT_RETARGET_INTERVAL = 2.0;

//! Melee approach: distance (meters) below which the bot steers straight at the
//! target (SetMoveYaw + SetMove) instead of running a navmesh MoveTo.
static const float DM_MELEE_APPROACH_NO_PATH_DIST = 3.0;

//! Melee evasion: strafe speed (0..3) while dodging between strikes.
static const float DM_MELEE_EVADE_SPEED = 2.0;

//! Melee evasion: seconds between strafe direction flips.
static const float DM_MELEE_EVADE_SWITCH_TIME = 0.3;

//! Firearm: cooldown (seconds) between the bot's shots (throttles cadence).
static const float DM_BOT_FIRE_COOLDOWN = 0.3;

//! Fire: interval (seconds) between fire requests in the Shooting state.
static const float DM_BOT_FIRE_INTERVAL = 0.5;

//! Aiming: base hit probability at point-blank range (accuracyMax, close range).
static const float DM_AIM_ACCURACY_MAX = 0.95;

//! Aiming: base hit probability at DM_AIM_MAX_ACCURACY_DIST (accuracyMin, long range).
static const float DM_AIM_ACCURACY_MIN = 0.75;

//! Aiming: distance (meters) at which accuracy degrades to DM_AIM_ACCURACY_MIN.
static const float DM_AIM_MAX_ACCURACY_DIST = 500.0;

//! Aiming: tracking time (seconds) to reach full tracking accuracy.
static const float DM_AIM_MAX_TRACKING_TIME = 3.5;

//! Aiming: target speed (m/s) below which a target counts as "standing" (headshot allowed).
static const float DM_AIM_HEADSHOT_SPEED_EPS = 0.5;

//! Aiming readiness: time (seconds) for the weapon raise animation.
static const float DM_AIM_RAISE_TIME = 0.5;

//! Aiming readiness: time (seconds) to bring the sight onto the target (ADS).
static const float DM_AIM_LOOK_TIME = 0.3;

//! Aiming readiness: extra time (seconds) to acquire the target in a magnified optic (ADS only).
static const float DM_AIM_ACQUIRE_TIME = 0.5;

//! Aiming: distance (meters) beyond which the bot always uses ADS (else HIP first).
static const float DM_AIM_ADS_DISTANCE = 100.0;

//! Aiming: seconds the bot stays in HIP before switching to ADS within DM_AIM_ADS_DISTANCE.
static const float DM_AIM_HIP_GRACE = 10.0;

//! Aiming recoil: base barrel kick (degrees up) per shot.
static const float DM_AIM_RECOIL_DEGREE = 2.0;

//! Aiming recoil: per-weapon multiplier (placeholder, later computed from weapon).
static const float DM_AIM_RECOIL_MODIFIER = 1.0;

//! Aiming recoil: recovery rate (degrees per second) — how fast the barrel lowers.
static const float DM_AIM_RECOIL_RECOVERY = 5.0;

//! Vertical navigation: fall height (meters) below which a drop is safe (matches
//! DayZPlayerImplementFallDamage.HEALTH_HEIGHT_LOW).
static const float DM_BOT_FALL_HEIGHT_LOW = 5.0;

//! Loot: timeout (seconds) after which an ignored item is picked up again.
static const float DM_LOOT_IGNORE_TIMEOUT = 300.0;
