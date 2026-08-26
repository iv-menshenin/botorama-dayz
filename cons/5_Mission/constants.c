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

//! Distance (meters) for the "where is the player looking" raycast.
static const float DM_LOOK_RAYCAST_DISTANCE = 200.0;

//! Deadline (seconds) for the test "look" intents.
static const float DM_TEST_LOOK_DEADLINE = 30.0;
