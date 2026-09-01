//! dmBotorama — mission-side constants.

//! Command word (first word after the leading "/"), e.g. "/bot spawn test".
static const string DM_CHAT_CMD = "bot";

//! Sub-command "spawn" / kind "test".
static const string DM_CHAT_SPAWN = "spawn";
static const string DM_CHAT_TEST = "test";

//! Sub-command "intent" and its actions.
static const string DM_CHAT_INTENT = "intent";
static const string DM_CHAT_LOOKAT = "lookAt";
static const string DM_CHAT_LOOKATME = "lookAtMe";
static const string DM_CHAT_GOTO = "goto";
static const string DM_CHAT_CLEAR = "clear";

//! Sub-command "patrol" and its actions.
static const string DM_CHAT_PATROL = "patrol";
static const string DM_CHAT_ADD = "add";

//! FSM command family ("/fsm ...").
static const string DM_CHAT_FSM = "fsm";
static const string DM_CHAT_FSM_NEW = "new";
static const string DM_CHAT_FSM_APPLY = "apply";
static const string DM_CHAT_FSM_IDLE = "idle";
static const string DM_CHAT_FSM_PATROL = "patrol";
static const string DM_CHAT_FSM_STEALTH = "stealth";

//! Test command family ("/test ...").
static const string DM_CHAT_CMD_TEST = "test";

//! Test scenario "overload" and its sub-commands.
static const string DM_CHAT_TEST_OVERLOAD = "overload";
static const string DM_CHAT_TEST_PREPARE = "prepare";
static const string DM_CHAT_TEST_RUN = "run";

//! Overload test: spawn radius / patrol radius / patrol point count.
static const float DM_TEST_OVERLOAD_SPAWN_RADIUS = 15.0;
static const float DM_TEST_OVERLOAD_PATROL_RADIUS = 100.0;
static const int DM_TEST_OVERLOAD_POINTS = 25;

//! Overload test: look-at-player intent lifetime (10 minutes).
static const float DM_TEST_OVERLOAD_LOOK_DEADLINE = 600.0;

//! Intent "stance" action and stance names.
static const string DM_CHAT_STANCE = "stance";
static const string DM_CHAT_STANCE_ERECT = "erect";
static const string DM_CHAT_STANCE_CROUCH = "crouch";
static const string DM_CHAT_STANCE_PRONE = "prone";

//! Sub-command "speed" and its values (preferred movement speed).
static const string DM_CHAT_SPEED = "speed";
static const string DM_CHAT_SPEED_WALK = "walk";
static const string DM_CHAT_SPEED_JOG = "jog";
static const string DM_CHAT_SPEED_SPRINT = "sprint";

//! Lifetime (seconds) for the "/bot intent crouch" shortcut intent.
static const float DM_TEST_STANCE_DEADLINE = 300.0;

//! Default deadline (seconds) for /bot intent commands without an explicit one.
static const float DM_TEST_COMMAND_DEADLINE = 60.0;

//! Distance (meters) for the "where is the player looking" raycast.
static const float DM_LOOK_RAYCAST_DISTANCE = 200.0;

//! Deadline (seconds) for the test "look" intents.
static const float DM_TEST_LOOK_DEADLINE = 30.0;

//! Profiler command family ("/prof ...").
static const string DM_CHAT_PROF = "prof";
static const string DM_CHAT_PROF_DUMP = "dump";
static const string DM_CHAT_PROF_CLEAR = "clear";
static const string DM_CHAT_PROF_START = "start";
static const string DM_CHAT_PROF_STOP = "stop";

//! Sub-command "status" — full body/brain state report.
static const string DM_CHAT_STATUS = "status";

//! Sub-command "loadout" — apply a loadout to the bound bot ("/bot loadout {name}").
static const string DM_CHAT_LOADOUT = "loadout";

//! Sub-command "follow" — escort the player ("/bot follow" / "/bot follow stop").
static const string DM_CHAT_FOLLOW = "follow";
static const string DM_CHAT_STOP = "stop";

//! Sub-command "give" — hand an item to the bound bot ("/bot give {item}").
static const string DM_CHAT_GIVE = "give";

//! Sub-command "melee" — order the bound bot to strike its hostile target.
static const string DM_CHAT_MELEE = "melee";

//! Sub-command "combat" — switch the bound bot to the combat preset.
static const string DM_CHAT_COMBAT = "combat";

//! Sub-command "vision" — report the bot's perception ("/bot vision").
static const string DM_CHAT_VISION = "vision";

//! Sub-command "deadmans" — report the vanilla corpse-decay state (CorpseData).
static const string DM_CHAT_DEADMANS = "deadmans";

//! "set" sub-commands ("/bot setX <float>"): each takes a REQUIRED float value.
static const string DM_CHAT_SETHEALTH = "sethealth";
static const string DM_CHAT_SETBLOOD = "setblood";
static const string DM_CHAT_SETSHOCK = "setshock";
static const string DM_CHAT_SETSTAMINA = "setstamina";
static const string DM_CHAT_SETHEATBUFFER = "setheatbuffer";
static const string DM_CHAT_SETTOXICITY = "settoxicity";
static const string DM_CHAT_SETENERGY = "setenergy";
static const string DM_CHAT_SETWATER = "setwater";

//! Test scenario sub-commands ("/test bot <scenario>").
static const string DM_CHAT_TEST_SHOCK = "shock";
static const string DM_CHAT_TEST_STAMINA = "stamina";
static const string DM_CHAT_TEST_BROKENLEG = "brokenleg";
static const string DM_CHAT_TEST_DEATH = "death";
static const string DM_CHAT_TEST_TARGET = "target";
static const string DM_CHAT_TEST_SHOOT = "shoot";
static const string DM_CHAT_TEST_AIM = "aim";
static const string DM_CHAT_TEST_EMOTE = "emote";

//! Aim-observation test: pause (seconds) before/after the shots and shot interval.
static const float DM_TEST_AIM_HOLD = 10.0;
static const float DM_TEST_AIM_SHOT_INTERVAL = 2.0;

//! Test command "cancel" — aborts the running test and cleans it up.
static const string DM_CHAT_TEST_CANCEL = "cancel";

//! Quiet delay (seconds) after showing a test summary before the test actually runs.
static const float DM_TEST_QUIET_SECONDS = 5.0;

//! Aim-accuracy test: default max distance (meters) and per-pass step.
static const float DM_AIM_TEST_MAX_DIST = 500.0;
static const float DM_AIM_TEST_STEP = 50.0;

//! Aim-accuracy test: pause (seconds) between passes.
static const float DM_AIM_TEST_PAUSE = 10.0;

//! Aim-accuracy test: timeout (seconds) for a single pass to kill the target.
static const float DM_AIM_TEST_PASS_TIMEOUT = 60.0;

//! Aim-accuracy test: backpack magazines (count and rounds each).
static const int DM_AIM_TEST_MAG_COUNT = 10;
static const int DM_AIM_TEST_MAG_ROUNDS = 5;
