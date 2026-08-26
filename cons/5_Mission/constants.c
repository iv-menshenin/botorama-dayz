//! dmBotorama — mission-side constants.

//! Command word (first word after the leading "/"), e.g. "/bot spawn test".
static const string DM_CHAT_CMD = "bot";

//! Sub-command "spawn".
static const string DM_CHAT_SPAWN = "spawn";

//! Sub-command "test".
static const string DM_CHAT_TEST = "test";

//! Sub-command "intent".
static const string DM_CHAT_INTENT = "intent";

//! Intent sub-commands.
static const string DM_CHAT_LOOKAT = "lookAt";
static const string DM_CHAT_LOOKATME = "lookAtMe";
static const string DM_CHAT_GOTO = "goto";
static const string DM_CHAT_CLEAR = "clear";

//! Distance (meters) for the "where is the player looking" raycast.
static const float DM_LOOK_RAYCAST_DISTANCE = 200.0;

//! Deadline (seconds) for the test "look" intents.
static const float DM_TEST_LOOK_DEADLINE = 30.0;
