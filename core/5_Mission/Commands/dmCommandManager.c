//! dmCommandManager — the chat command engine.
//!
//! Command modules register themselves by name; a chat message is tokenized, the
//! first word picks the module, the player is resolved, IsAllowUser is checked, and
//! then the module handles the rest. Generic helpers (player lookup, look raycast,
//! chat reply) live here as statics.
class dmCommandManager
{
	static ref dmCommandManager s_Instance;

	private ref map<string, ref dmCommandModule> m_Modules;

	void dmCommandManager()
	{
		m_Modules = new map<string, ref dmCommandModule>();
	}

	//! Singleton. Created lazily (test/5_Mission registers modules into it).
	static dmCommandManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new dmCommandManager();
		return s_Instance;
	}

	void Register(dmCommandModule module)
	{
		m_Modules.Insert(module.GetName(), module);
	}

	//! Resolve a command from a chat message and delegate to its module.
	bool Execute(string playerName, string message)
	{
		array<string> parts = new array<string>();
		message.Trim().Split(" ", parts);
		if (parts.Count() < 1)
			return false;

		string cmd = parts[0];
		if (cmd.Length() > 0 && cmd.Substring(0, 1) == "/")
			cmd = cmd.Substring(1, cmd.Length() - 1);

		dmCommandModule module = m_Modules.Get(cmd);
		if (!module)
			return false;

		PlayerBase player = FindPlayerByName(playerName);
		if (!player)
			return false;

		if (!module.IsAllowUser(player))
			return false;

		return module.Handle(player, parts);
	}

	//! Resolve a player by (case-insensitive) name.
	static PlayerBase FindPlayerByName(string name)
	{
		name.ToLower();

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);

		foreach (Man man : players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity())
			{
				string playerName = player.GetIdentity().GetName();
				playerName.ToLower();
				if (playerName == name)
					return player;
			}
		}

		return null;
	}

	//! World point the player is looking at (raycast along the camera look direction).
	//! The camera direction is approximated on the server by the head bone's forward
	//! vector (includes pitch), which is much closer to the crosshair than the
	//! horizontal body heading (GetHeadingVector).
	static void GetPlayerLookPoint(PlayerBase player, out vector point)
	{
		vector beg;
		vector dir;

		int headBone = player.GetBoneIndexByName("Head");
		if (headBone != -1)
		{
			vector headTransform[4];
			player.GetBoneTransformWS(headBone, headTransform);
			beg = player.GetBonePositionWS(headBone);
			dir = headTransform[1];
		}
		else
		{
			beg = player.GetPosition() + Vector(0, DM_EYE_HEIGHT, 0);
			dir = MiscGameplayFunctions.GetHeadingVector(player);
		}

		vector end = beg + dir * DM_LOOK_RAYCAST_DISTANCE;
		vector contactPos;
		vector contactDir;
		int contactComponent;
		if (DayZPhysics.RaycastRV(beg, end, contactPos, contactDir, contactComponent, null, null, player, false, false, ObjIntersectView))
			point = contactPos;
		else
			point = end;
	}

	//! Send a chat reply to a player.
	static void ChatToPlayer(PlayerBase player, string msg)
	{
		if (player)
			GetGame().ChatMP(player, msg, "colorAction");
	}
}
