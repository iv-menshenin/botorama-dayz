//! dmBotorama — base constants (world module).

//! Default survivor model class used when spawning a bot.
static const string DM_DEFAULT_MODEL = "dmAI_SurvivorM_Denis";

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

//! Reach radius (meters) at which a crew entry point (Transport.CrewEntryWS)
//! counts as reached and the bot starts boarding the vehicle.
static const float DM_GETIN_REACH = 1.0;

//! Car door: animation phase at which a door counts as "open".
static const float DM_CAR_DOOR_OPEN_PHASE   = 0.9;

//! Car door: animation phase at which a door counts as "closed".
static const float DM_CAR_DOOR_CLOSED_PHASE = 0.1;

//! Car door: safety timeout (seconds) for the open/close poll — the poll normally
//! finishes earlier via GetAnimationPhase.
static const float DM_CAR_DOOR_TIMEOUT      = 0.75;

//! Car door: safety timeout (seconds) for the get-in/get-out ANIMATION itself
//! (the door phase polls use DM_CAR_DOOR_TIMEOUT). The poll normally finishes
//! earlier via GetCommand_Vehicle()/IsGettingIn().
static const float DM_CAR_DOOR_ANIM_TIMEOUT = 3.0;

//! Grace period (seconds) after starting GetOutVehicle before the intent checks
//! completion — gives the get-out command a moment to become active.
static const float DM_GETOUT_GRACE = 0.5;

//! Idle sit-by-fire: search radius (meters) for a burning fireplace.
static const float DM_SIT_BY_FIRE_RADIUS = 3.0;

//! Idle sit-by-fire: sit distance (meters) from the fireplace — close for a
//! campfire/barrel, farther for an indoor stove (so the bot doesn't block the player).
static const float DM_SIT_BY_FIRE_DIST_CLOSE = 2.5;
static const float DM_SIT_BY_FIRE_DIST_FAR = 3.0;

//! Idle sit-by-fire: reach radius (meters) for the sit point (MoveTo target).
static const float DM_SIT_BY_FIRE_REACH = 0.3;

//! Idle sit-by-fire: how long (seconds) the bot stays seated.
static const float DM_SIT_BY_FIRE_TIME = 15.0;

//! Idle sit-by-fire: cooldown (seconds) after finishing a sit before the bot may
//! sit again — prevents an immediate sit/stand/sit loop while the fire still burns.
static const float DM_SIT_BY_FIRE_COOLDOWN = 60.0;

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

//! Turn speed multiplier
static const float DM_MOVE_TURN_SPEED = 1.0;

//! Max body slide-turn rate while moving (degrees per second).
static const float DM_MOVE_TURN_RATE = 180.0;

//! Turn response (fraction of the remaining body-turn error applied per frame)
//! for the moving slide-turn. <1.0 turns the body smoothly over several frames
//! instead of snapping the full error in one frame.
static const float DM_MOVE_TURN_RESPONSE = 0.4;

//! Seconds without meaningful progress toward the target before MoveTo aborts.
static const float DM_MOVE_STUCK_TIME = 0.5;

//! Min distance decrease (meters) that counts as "progress" (resets the stuck timer).
static const float DM_MOVE_PROGRESS_EPS = 0.25;

//! MoveTo: cap (seconds) for m_TooCloseTime — bounds the reach-radius expansion
//! while the bot oscillates close to a waypoint (prevents stopping ever-farther).
static const float DM_MOVE_TOO_CLOSE_MAX = 1.0;

//! Min distance (meters) to the goal at which the stuck detector still resolves
//! (closer than this the bot is at the goal, not stuck).
static const float DM_MOVE_STUCK_MIN_DIST = 0.5;

//! Proactive vision probe interval (seconds) — how often MoveTo probes ahead.
static const float DM_MOVE_VISION_INTERVAL = 0.25;

//! Tree avoidance: trunk-probe lookahead distance (meters) — how far ahead the
//! low thick trunk ray reaches (longer than the 1 m climb/eye probe so the bot has
//! room to veer around the trunk instead of walking into it).
static const float DM_TREE_LOOKAHEAD = 2.5;
//! Tree avoidance: trunk-ray height (meters) above the ground — low enough to hit
//! the trunk, high enough to clear rocks/roots.
static const float DM_TREE_RAY_HEIGHT = 0.4;
//! Tree avoidance: trunk-ray radius (meters) — a thick ray so a thin trunk is not
//! missed when the trajectory is offset from the trunk axis.
static const float DM_TREE_RAY_RADIUS = 0.25;
//! Tree avoidance: tree-candidate flag freshness (seconds) — how long a detected
//! tree stays "fresh" before the veer trigger ignores it.
static const float DM_TREE_FLAG_TIMEOUT = 0.5;
//! Tree avoidance: veer direction (degrees) off the movement direction — a forward
//! diagonal, not a pure 90° sidestep, so the bot still advances through a dense forest.
static const float DM_TREE_VEER_DIR = 45.0;
//! Tree avoidance: veer duration (seconds) by pace — the faster the bot moves, the
//! shorter the veer so it doesn't overshoot the trunk.
static const float DM_TREE_VEER_TIME_WALK = 0.7;
static const float DM_TREE_VEER_TIME_JOG = 0.35;
static const float DM_TREE_VEER_TIME_SPRINT = 0.2;

//! Collision oracle (test): |moveAngle| (degrees) below this counts as "commanding
//! forward" — a larger angle means the bot is deliberately strafing/backing, not
//! sliding off a round trunk.
static const float DM_TREE_COLLISION_MOVE_ANGLE = 20.0;
//! Collision oracle: forward velocity component (m/s) above this means the bot is
//! actually moving forward (not just standing against the trunk).
static const float DM_TREE_COLLISION_FORWARD = 0.5;
//! Collision oracle: lateral velocity component (m/s) above this counts as the bot
//! sliding sideways — the signature of a tangential slide off a round tree trunk.
static const float DM_TREE_COLLISION_LATERAL = 0.4;
//! Collision oracle: seconds after a veer ends during which the oracle ignores the
//! lateral velocity component (the veer's diagonal impulse decays here).
static const float DM_TREE_ORACLE_GRACE = 0.5;

//! Cooldown (seconds) for the climb-candidate flag from the vision probe.
static const float DM_CLIMB_FLAG_COOLDOWN = 1.0;

//! Timeout (seconds) for the door-candidate flag from the vision probe.
static const float DM_DOOR_FLAG_TIMEOUT = 2.0;

//! Vertical tolerance (meters) for the ground probe — a point counts as on the
//! navmesh when its height is within this of the navmesh surface.
static const float DM_MOVE_GROUND_PROBE_Y = 4.0;

//! Radius (meters) around the ground-probe point to search for the navmesh
//! (SampleNavmeshPosition). A point counts as "on the ground" when the navmesh is
//! within this of it.
static const float DM_MOVE_GROUND_PROBE_RADIUS = 1.0;

//! Height (meters) of the near-ground "toe" ray in ProbeAhead — catches low
//! partitions/fences that the higher climb/eye rays pass over.
static const float DM_MOVE_PROBE_TOE_Y = 0.15;

//! Radius (meters) to snap the pathfinding target onto the navmesh.
static const float DM_PATH_SAMPLE_RADIUS = 2.0;

//! Reach radius (meters) for an intermediate path waypoint.
static const float DM_PATH_WAYPOINT_REACH = 0.15;

//! Порог разницы высот (м) между последним вейпоинтом и целью, выше которого маршрут
//! считается «недостижимым» (разрыв navmesh → ищем лестницу).
static const float DM_NAV_GAP = 1.5;

//! Максимальная глубина рекурсии FindRoute (длина цепочки лестниц через разрывы navmesh).
//! Страховка от бесконечной рекурсии (помимо visited-set по типу+индексу лестницы).
static const int DM_NAV_MAX_DEPTH = 6;

//! Path rounding: turn angle (degrees) below which a corner is left unchanged.
static const float DM_PATH_ROUND_ANGLE_LOW   = 75.0;
//! Path rounding: turn angle (degrees) above which a corner is stair-stepped (90°).
static const float DM_PATH_ROUND_ANGLE_HIGH  = 120.0;
//! Path rounding: step length (meters) for overshoot / 90° stair-steps.
static const float DM_PATH_ROUND_STEP        = 0.25;
//! Path rounding: lookahead distance (meters) from the bot for corner rounding.
static const float DM_PATH_ROUND_LOOKAHEAD   = 25.0;

//! Raycast distance (meters) straight ahead at eye level to detect a closed door.
static const float DM_DOOR_OPEN_DIST = 2.0;

//! Throttle interval (seconds) for the proactive door check in MoveTo.
static const float DM_DOOR_CHECK_INTERVAL = 0.5;

//! Distance (meters) from the door the bot backs up to before opening it, so the
//! swinging door doesn't push it (see dmBotIntent_OpenDoor).
static const float DM_DOOR_STEP_BACK_DIST = 0.5;

//! Max time (seconds) the bot backs away before opening the door anyway.
static const float DM_DOOR_STEP_BACK_TIMEOUT = 0.1;

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

//! Time (seconds) without vertical progress on the ladder before the bot is
//! considered stuck (then it reverses the climb direction once, then gives up).
static const float DM_LADDER_STUCK_TIME = 3.0;

//! Min vertical change (meters) on the ladder that counts as "progress" (resets
//! the stuck timer).
static const float DM_LADDER_PROGRESS_EPS = 0.1;

//! Min height difference (meters) between the goal and the bot for a ladder to
//! even be considered (less = same floor, no ladder needed).
static const float DM_LADDER_FLOOR_GAP = 1.5;

//! Max horizontal distance (meters) from the bot to the ladder entry point at
//! which the bot is still considered "at the ladder" and may start climbing.
static const float DM_LADDER_ENTRY_REACH = 2.5;

//! Max time (seconds) in the ladder-approach phase before the bot gives up
//! (the entry point is unreachable).
static const float DM_LADDER_APPROACH_TIME = 5.0;

//! Grace period (seconds) after starting a vault/climb before MoveTo checks
//! IsClimbing() — gives the climb command time to become active.
static const float DM_VAULT_GRACE = 1.0;

//! Time (seconds) the bot steps back/sideways per stuck-recovery attempt before
//! re-routing.
static const float DM_MOVE_RECOVER_TIME = 1.0;

//! Max stuck-recovery attempts (step back/sideways + re-route) before MoveTo aborts.
static const int DM_MOVE_MAX_RECOVER = 2;

//! Time (seconds) the bot sidesteps perpendicular per lateral-detour attempt
//! before re-routing (a longer sidestep than the short recovery step).
static const float DM_MOVE_DETOUR_TIME = 2.0;

//! Max lateral-detour attempts before MoveTo gives up and aborts/re-paths.
static const int DM_MOVE_MAX_DETOUR = 3;

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

//! Dead-zone (degrees) for the body-relative movement angle: |angle| below this
//! is zeroed to kill the ±1-2° strafe jitter (micro-tremor) at sprint.
static const float DM_MOVE_ANGLE_DEADZONE = 3.0;

//! Max age (seconds) of an intent without a deadline before the arbitration
//! removes it (safety net — no intent lives forever).
static const float DM_INTENT_MAX_AGE = 300.0;

//! Directory for loadout files (relative to the DayZ profile root). A loadout
//! named "hunter" is stored at "$profile:dmBotorama/loadouts/hunter.json".
static const string DM_LOADOUT_DIR = "$profile:dmBotorama/loadouts";

//! Directory where the user drops generated map configs (building interiors, POIs).
static const string DM_MAP_CONFIG_DIR = "$profile:dmBotorama/map";
//! Building-interior map file (schema dmBuildingInteriorConfig). The user places a
//! copy of the generated data/map/buildings_interior.json here.
static const string DM_MAP_BUILDINGS_FILE = "$profile:dmBotorama/map/buildings_interior.json";
//! World POI file (schema dmWorldPoiConfig). The user places a copy of the
//! generated data/map/world_poi.json here.
static const string DM_MAP_WORLD_POI_FILE = "$profile:dmBotorama/map/world_poi.json";
//! Spawn distribution file (schema dmSpawnConfig). Created with defaults on first run.
static const string DM_MAP_SPAWN_FILE = "$profile:dmBotorama/map/spawn.json";
//! Spawn manager tick interval (seconds) — how often Tick checks/replenishes population.
static const float DM_SPAWN_TICK_INTERVAL = 5.0;
//! Random offset (meters, ±) of a bot's spawn point from its settlement center.
static const float DM_SPAWN_CITY_OFFSET = 15.0;
//! Delay (ms) before a House registers itself, so the building has a valid position
//! by the time the registry reads it (in the constructor it is still (0,0,0)).
static const int DM_HOUSE_REGISTER_DELAY_MS = 1000;

//! Follow (escort): distance beyond which the bot enters the Follow state (via
//! dmBotCondition_FollowFar). Players may lead farther than other entities.
static const float DM_FOLLOW_THRESHOLD_PLAYER = 5.0;
static const float DM_FOLLOW_THRESHOLD_OTHER = 1.0;

//! Follow: distance to the target at which the bot counts as "in place" (anchor —
//! stops this far short of the target instead of colliding with it).
static const float DM_FOLLOW_REACH = 1.0;

//! Follow (FollowTo): reach radius (meters) to the escort ANCHOR (the shoulder
//! point is already DM_FOLLOW_SIDE_DISTANCE from the target).
static const float DM_FOLLOW_ANCHOR_REACH = 0.5;

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

//! Follow: seconds the new FollowTo/MoveTo decision must persist before the state
//! switches (hysteresis). Dampens the flickering m_HasLOS from the FOV-cone gate
//! while the head scans, so intents aren't thrashed and the stuck monitor survives.
static const float DM_FOLLOW_SWITCH_DWELL = 1.0;

//! Follow (FollowTo): distance to the anchor (meters) beyond which the escort sprints.
static const float DM_FOLLOW_SPRINT_GAP = 7.5;

//! Follow (FollowTo): distance to the anchor (meters) beyond which the escort jogs
//! (below it — walks).
static const float DM_FOLLOW_JOG_GAP = 5.0;

//! Follow (FollowTo): path re-computation interval (seconds), capped at 1 Hz.
static const float DM_FOLLOW_PATH_INTERVAL = 1.0;

//! Follow (FollowTo): target-velocity smoothing factor (0..1; higher = snappier).
static const float DM_FOLLOW_VEL_SMOOTH = 0.3;

//! Follow: seconds a target counts as "recently seen" for the FollowTo/MoveTo
//! decision — the FOV-cone LOS gate flickers while the head scans, so this keeps
//! FollowTo engaged for a few seconds after the head turns away.
static const float DM_FOLLOW_VISIBLE_RECENT = 5.0;

//! Follow (FollowTo): minimum 2D anchor drift (meters) that triggers a re-path
//! while following. A stationary anchor must not re-path every second.
static const float DM_FOLLOW_REPATH_DIST = 2.0;

//! Danger (red zone): avoid radius (meters) around a burning fireplace.
static const float DM_BOT_DANGER_AVOID_RADIUS = 0.6;

//! Danger (red zone): lifetime (seconds) of a remembered dangerous position.
static const float DM_BOT_DANGER_TIMEOUT     = 300.0;

//! Danger: extra margin (meters) added to a red-zone radius when offsetting the sub-goal.
static const float DM_BOT_DANGER_MARGIN      = 0.4;

//! Danger escape: distance (meters) the bot backs away from a burning fireplace.
static const float DM_DANGER_ESCAPE_DIST = 1.5;

//! Danger: throttle interval (seconds) for the proactive campfire scan in MoveTo.
static const float DM_DANGER_CHECK_INTERVAL  = 0.5;

//! MoveTo: periodic re-path interval (seconds) for non-continuous intents.
static const float DM_MOVE_REPATH_INTERVAL   = 5.0;

//! Scan: random interval (seconds) between idle direction changes.
static const float DM_SCAN_INTERVAL_MIN = 10.0;
static const float DM_SCAN_INTERVAL_MAX = 30.0;

//! Scan: head turn angle range (±degrees from the body).
static const float DM_SCAN_ANGLE_MIN = 15.0;
static const float DM_SCAN_ANGLE_MAX = 75.0;

//! Scan: how long the head holds a new direction before recentering (seconds).
static const float DM_SCAN_HOLD_MIN = 3.0;
static const float DM_SCAN_HOLD_MAX = 10.0;

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
static const float DM_PERCEPTION_FOV = 160.0;

//! Perception: discovery scan interval (seconds) — how often the bot opens new
//! targets from the registry (radius check only; no FOV/LOS here).
static const float DM_PERCEPTION_BOX_INTERVAL = 1.0;

//! Perception: per-target LOS refresh intervals (seconds). The LOS pass runs every
//! tick and re-checks each target at its own cadence: creatures and friendly targets
//! are cheap to keep fresh, high-threat targets re-check fastest, low-threat slower.
//! High-threat is deliberately 0.2 (5 Hz raycast) rather than 0.1: at 10 Hz the LOS
//! raycast of a hostile dominated the profile (~91% CPU after Opt-1). The 200 ms
//! slower refresh means a bot may briefly still "see" a target that just hid, but
//! that is human-sized and the FOV-cone gate clears m_HasLOS instantly when the
//! target leaves the cone.
static const float DM_PERCEPTION_REFRESH_CREATURE = 0.25;
static const float DM_PERCEPTION_REFRESH_FRIENDLY = 0.3;
static const float DM_PERCEPTION_REFRESH_LOW_THREAT = 0.2;
static const float DM_PERCEPTION_REFRESH_HIGH_THREAT = 0.2;

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

//! Restrain (связывание): угроза, назначаемая restrainer'у при связывании (сознание).
static const float DM_RESTRAIN_THREAT_RESTRAINER = 0.1;
//! Restrain (связывание): угроза, назначаемая всем людям в радиусе при связывании из отключки.
static const float DM_RESTRAIN_THREAT_UNCONSCIOUS = 0.35;
//! Restrain (связывание): радиус (метры), в котором люди считаются причастными при отключке.
static const float DM_RESTRAIN_THREAT_RADIUS = 8.0;

//! Restrain (развязывание): длительность (секунды) анимации борьбы до освобождения.
static const float DM_UNTIE_DURATION = 3.0;

//! Target evaluation: attractiveness (0..1) — how interesting a target is.
static const float DM_TARGET_ATTRACT_PLAYER = 0.1;
static const float DM_TARGET_ATTRACT_ZOMBIE = 0.2;
static const float DM_TARGET_ATTRACT_ANIMAL = 0.5;

//! Target memory: seconds without contact before a remembered target is forgotten.
static const float DM_TARGET_FORGET_TIME = 300.0;

//! Aggro через прицеливание: бот считает игрока враждебным, если тот целится
//! в него (IsRaised + огнестрел в руках + прицел в силуэт бота). Угловое окно —
//! угловой размер бота (ширина/высота) с запасом DM_AGGRO_AIM_ENLARGE.
static const float DM_AGGRO_AIM_RATE          = 0.25;   // threat/с, накапливается потиково пока прицел в силуэте
static const float DM_AGGRO_AIM_TARGET_WIDTH  = 0.5;    // ширина силуэта бота (плечи), м
static const float DM_AGGRO_AIM_TARGET_HEIGHT = 1.8;    // высота силуэта бота (рост), м
static const float DM_AGGRO_AIM_ENLARGE       = 1.07;   // запас +7% к угловому размеру

//! Поправка кости головы: на сервере голова не следует стволу (эталон —
//! Expansion.Expansion_GetAimDirection: +5° яу, +12.5° питч).
static const float DM_AGGRO_AIM_HEAD_YAW   = 5.0;    // поправка яу (градусы)
static const float DM_AGGRO_AIM_HEAD_PITCH = 12.5;   // поправка питча (градусы) — главная

//! Пол окна прицеливания (градусы): серверный прицел — аппроксимация + sway,
//! окно не должно схлопываться в 1-2° на дистанции.
static const float DM_AGGRO_AIM_MIN_HALF_W = 5.0;    // пол по яу
static const float DM_AGGRO_AIM_MIN_HALF_H = 10.0;   // пол по питчу

//! Уворот от прицела (EvadeAim): threat-давление и геометрия укрытия.
static const float DM_EVADE_AIM_RATE            = 0.05;   // threat/с, пока интент активен
static const float DM_EVADE_AIM_SHOT_THREAT     = 0.35;   // threat за выстрел агрессора (попытка убийства)
static const float DM_EVADE_AIM_END_DIST        = 10.0;   // (a) конец: агрессор дальше X
static const float DM_EVADE_AIM_END_AIM_ANGLE   = 45.0;   // (a) угол прицела > X
static const float DM_EVADE_AIM_END_HEAD_ANGLE  = 25.0;   // (b) угол головы > X (после опускания)
//! Геометрия: полукруг (±MAX_ANGLE) вокруг агрессора, радиус от MIN_DIST, растёт
//! на DIST_STEP при исчерпании свипа.
static const float DM_EVADE_AIM_START_ANGLE     = 15.0;
static const float DM_EVADE_AIM_ANGLE_STEP      = 15.0;
static const float DM_EVADE_AIM_MAX_ANGLE       = 90.0;   // полукруг (±90°), не полный круг
static const float DM_EVADE_AIM_MIN_DIST        = 10.0;   // мин. радиус круга
static const float DM_EVADE_AIM_MAX_DIST        = 60.0;   // макс. радиус
static const float DM_EVADE_AIM_DIST_STEP       = 10.0;   // рост радиуса при исчерпании
static const float DM_EVADE_AIM_MAX_SURFACE_DELTA = 1.5;
static const float DM_EVADE_AIM_STALL_TIMEOUT   = 8.0;
//! Страйф при поиске укрытия: поставь false, чтобы бот стоял, когда укрытия нет.
static const bool  DM_EVADE_AIM_STRAFE          = true;
static const float DM_EVADE_AIM_STRAFE_SWITCH   = 0.5;    // смена стороны страйфа, с

//! Melee: cooldown (seconds) between the bot's strikes.
static const float DM_MELEE_COOLDOWN = 0.6;

//! Melee: allowed body-facing error (degrees) before a strike is thrown.
static const float DM_MELEE_FACE_ANGLE = 90.0;

//! Melee: damage multiplier applied per strike against zombies.
static const int DM_MELEE_DAMAGE_MULT_ZOMBIE = 2;

//! Melee: fallback strike reach (meters) when the weapon reach can't be read.
static const float DM_MELEE_REACH = 1.5;

//! Melee spin oracle (diagnostic): accumulated body-yaw (degrees) during a single
//! strike above which the bot is considered to have "spun" (ApplyBodyTurn foot-step
//! turn during the swing). Logged by dmAISurvivorBase.TickMeleeSpinOracle.
static const float DM_MELEE_SPIN_THRESHOLD = 180.0;

//! Melee stall oracle (diagnostic): seconds in reach with cooldown 0 and no strike
//! request before the bot is considered a "dummy" (in reach but not attacking).
static const float DM_MELEE_STALL_THRESHOLD = 3.0;

//! Retrieve dropped weapon (knockout recovery): seconds to complete a full 360°
//! ground scan. The look point advances one full circle over this time.
static const float DM_RETRIEVE_SCAN_TIME = 4.0;

//! Retrieve dropped weapon: distance (meters) of the ground look point ahead of
//! the bot while sweeping.
static const float DM_RETRIEVE_LOOK_DIST = 1.0;

//! Retrieve dropped weapon: half-angle (degrees) of the visibility cone for the
//! raycast to the item — the item is only picked up while within this of the
//! current sweep direction.
static const float DM_RETRIEVE_FOV = 30.0;

//! Retrieve dropped weapon: height lift (meters) of the raycast target point above
//! the item's root position. A flat item's root sits at ground level, so a ray to
//! it would hit the terrain before the item; lifting the point clears the terrain.
static const float DM_RETRIEVE_RAY_LIFT = 0.3;

//! Fighting: seconds between target re-resolution (re-pick the nearest hostile).
static const float DM_FIGHT_RETARGET_INTERVAL = 2.0;

//! Melee approach: distance (meters) below which the bot steers straight at the
//! target (SetMoveYaw + SetMove) instead of running a navmesh MoveTo.
static const float DM_MELEE_APPROACH_NO_PATH_DIST = 3.0;

//! Melee evasion: strafe speed (0..3) while dodging between strikes.
static const float DM_MELEE_EVADE_SPEED = 2.0;

//! Melee evasion: seconds between strafe direction flips.
static const float DM_MELEE_EVADE_SWITCH_TIME = 0.3;

//! Flank (combat movement): start angle (degrees) of the sweep around a target
//! the bot can't see — the first candidate is this far off the target->bot line.
static const float DM_FLANK_START_ANGLE = 15.0;

//! Flank: sweep step (degrees) advanced per tick while searching for a candidate.
static const float DM_FLANK_ANGLE_STEP = 15.0;

//! Flank: sweep limit (degrees, both sides). Exceeding it fails the flank.
static const float DM_FLANK_MAX_ANGLE = 180.0;

//! Flank: cap (meters) of the flank distance — min(distance, this) is used.
static const float DM_FLANK_MAX_DIST = 180.0;

//! Flank: minimum distance (meters) to the target below which the bot does not
//! flank (too close — better to disengage than circle).
static const float DM_FLANK_MIN_DIST = 5.0;

static const float DM_FLANK_LOW_WEAPON_TIMING = 2.5;

//! Flank: max vertical delta (meters) between a path point and the terrain
//! surface — a bigger gap means the candidate is unreachable/wrong height.
static const float DM_FLANK_MAX_SURFACE_DELTA = 1.5;

//! Flank: whole-attempt timeout (seconds). A stuck/slow MoveTo aborts after this.
static const float DM_FLANK_STALL_TIMEOUT = 8.0;

//! Hunting: min attractiveness (0..1) to search for a target it can't see.
static const float DM_HUNT_MIN_ATTRACTIVENESS = 0.5;

//! Hunting: facing factor range — spread = distance × (min at face, max at back).
static const float DM_HUNT_FACING_MIN = 0.05;
static const float DM_HUNT_FACING_MAX = 0.25;

//! Hunting: clamp of the target-position spread (meters).
static const float DM_HUNT_SPREAD_MIN = 1.0;
static const float DM_HUNT_SPREAD_MAX = 250.0;

//! Weapon selection: distance (meters) below which a pistol is preferred over a
//! rifle; at/above it a rifle is preferred.
static const float DM_WEAPON_SEL_FAR = 50.0;

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

//! Aiming settle: ticks to wait for the barrel to turn onto the dispersed aim
//! direction (rolled per-shot) before the shot fires along the barrel.
static const int DM_AIM_SETTLE_TICKS = 5;

//! Shot: max hitscan distance (meters) for bullet-drop compensation.
static const float DM_AI_SHOT_MAX_DISTANCE = 1200.0;

//! Shot: fallback muzzle velocity (m/s) when the ammo initSpeed can't be read.
static const float DM_AI_DEFAULT_INIT_SPEED = 800.0;

//! Vertical navigation: fall height (meters) below which a drop is safe (matches
//! DayZPlayerImplementFallDamage.HEALTH_HEIGHT_LOW).
static const float DM_BOT_FALL_HEIGHT_LOW = 5.0;

//! Fall-safety: перепад (м) ниже точки, при котором точка считается «обрывом»
//! (уже нельзя vault/climb вниз).
static const float DM_FALL_DANGER_DROP = 2.75;
//! Fall-safety: блокировать «прыжок вниз», если бот выше этого (м) над землёй.
static const float DM_FALL_JUMP_BLOCK_HEIGHT = 3.0;
//! Fall-safety: дистанция (м) контрольных точек вперёд.
static const float DM_FALL_CHECK_AHEAD = 0.5;
//! Fall-safety: если поверхность земли в пределах этого (м) от Y вейпоинта — пропустить лучи.
static const float DM_FALL_SURFACE_EPS = 1.5;
//! Fall-safety: троттлинг проверки (с).
static const float DM_FALL_CHECK_INTERVAL = 1.0;
//! Fall-safe: длительность (с) отхода назад от края перед перестроением маршрута.
static const float DM_STEP_BACK_TIME = 0.5;

//! Loot: timeout (seconds) after which an ignored item is picked up again.
static const float DM_LOOT_IGNORE_TIMEOUT = 300.0;

//! Loot: used-capacity fraction (0..1) above which the inventory counts as full
//! (less than 25% free space). See dmRequirements.IsFull.
static const float DM_LOOT_FULL_THRESHOLD = 0.75;

//! Loot: needs-coordinator tick interval (seconds) — how often inventory -> desires.
static const float DM_NEEDS_TICK_INTERVAL = 5.0;

//! Loot: distance (meters) to an item at which the bot picks it up.
static const float DM_PICKUP_REACH = 1.0;

//! Loot exploration: scan radius (meters) for buildings around the bot.
static const float DM_EXPLORE_SCAN_RADIUS = 50.0;
//! Loot exploration: tick interval (seconds) between building scans.
static const float DM_EXPLORE_TICK_INTERVAL = 60.0;
//! Loot exploration: forget any building farther than this (meters).
static const float DM_EXPLORE_FORGET_ANY = 500.0;
//! Loot exploration: forget an unvisited building farther than this (meters).
static const float DM_EXPLORE_FORGET_UNVISITED = 200.0;

//! Loot exploration: radius (meters) to scan for items to pick up.
static const float DM_EXPLORE_PICKUP_RADIUS = 3.0;
//! Loot exploration: min CalcDesired to pick up an item.
static const float DM_EXPLORE_PICKUP_THRESHOLD = 0.5;
//! Loot exploration: radius (meters) to search for an unvisited building.
static const float DM_EXPLORE_EXPLORE_RADIUS = 100.0;
//! Loot exploration: reach (meters) to a building before marking it visited.
static const float DM_EXPLORE_BUILDING_REACH = 2.0;
//! Loot exploration: cooldown (seconds) between drops when the inventory is full.
static const float DM_EXPLORE_DROP_COOLDOWN = 15.0;

//! TidyInventory: cooldown (seconds) between individual ammo/magazine steps.
static const float DM_TIDY_STEP_INTERVAL = 1.0;

//! TidyInventory: rescan interval (seconds) when there is nothing to do.
static const float DM_TIDY_SCAN_INTERVAL = 3.0;

//! MedicalCare: порог здоровья (0..1), ниже которого после перевязки бот принимает
//! бонус-обезболивающее.
static const float DM_MEDICAL_PAINKILLER_HEALTH_THRESHOLD = 0.75;
//! MedicalCare: число тряпок (Rag), списываемых за спавн шины (если нет бинта).
static const int   DM_MEDICAL_SPLINT_RAG_COST = 4;
//! MedicalCare: порог «холодно» (HeatComfort) — ниже него бот пьёт витамины.
static const float DM_MEDICAL_COLD_HC = -0.15;
//! MedicalCare: таймаут (секунды) ожидания завершения full-body анимации (фолбэк).
static const float DM_MEDICAL_ANIM_TIMEOUT = 4.0;

//! EatDrink (персональный рефлекс еды/питья): порог энергии, ниже которого бот ест.
static const float DM_EAT_DRINK_ENERGY        = 2500.0;
//! EatDrink: порог воды, ниже которого бот пьёт.
static const float DM_EAT_DRINK_WATER         = 2500.0;
//! EatDrink: радиус (метры), в котором враг блокирует еду/питьё.
static const float DM_EAT_DRINK_ENEMY_RADIUS  = 250.0;
//! EatDrink: полный предмет съедается за это время (секунды) — portion = GetQuantityMax()/X*dt.
static const float DM_EAT_DRINK_FULL_TIME     = 10.0;
//! EatDrink: таймаут (секунды) ожидания замены закрытой консервы на X_Opened.
static const float DM_EAT_DRINK_OPEN_TIMEOUT  = 3.0;
//! EatDrink: интервал (секунды) пересканирования, когда есть/пить нечего.
static const float DM_EAT_DRINK_SCAN_INTERVAL = 2.0;

//! Travel (кочёвка): порог воды, ниже которого бот считается жаждущим (GetStatWater().Get()).
static const float DM_TRAVEL_WATER_THRESHOLD = 2500.0;
//! Travel: радиус (метры) поиска ближайшего POI (застримленного здания).
static const float DM_TRAVEL_POI_SEARCH_RADIUS = 300.0;
//! Travel: дистанция (метры) прибытия к POI.
static const float DM_TRAVEL_REACH_DISTANCE = 3.0;
