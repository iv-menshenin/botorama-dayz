//! dmCommandModule — base class for a chat command module.
//!
//! A module handles one command family (e.g. "/bot", "/fsm", "/test"). The command
//! manager resolves the player and delegates the tokenized message here. parts[0] is
//! the command name, parts[1..] are the arguments.
class dmCommandModule
{
	//! Command prefix (e.g. "bot", "fsm", "test"). Used as the registry key.
	string GetName()
	{
		return "";
	}

	//! Whether the player is allowed to use this command. Stub: always true
	//! (future: a real permission system).
	bool IsAllowUser(PlayerBase player)
	{
		return true;
	}

	//! Handle the command. Return true if the command was consumed.
	bool Handle(PlayerBase player, array<string> parts)
	{
		return false;
	}
}
