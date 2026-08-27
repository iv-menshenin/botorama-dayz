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
