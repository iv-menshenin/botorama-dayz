//! dmCommandContext — shared state and domain helpers for the test command modules.

class dmCommandContext
{
	//! Player Id -> bound bot.
	static ref map<string, ref dmAISurvivor> s_TestBotByPlayer = new map<string, ref dmAISurvivor>;

	//! FSM draft state names (/fsm new/add/apply).
	static ref array<string> s_DraftStates = new array<string>;

	//! Cover position for a draft "stealth" state.
	static vector s_DraftStealthCover;

	//! Bots spawned by the overload test (prepared, not yet run).
	static ref array<ref dmAISurvivor> s_OverloadBots = new array<ref dmAISurvivor>;

	//! The bot bound to the player (or null).
	static dmAISurvivor FindBotForPlayer(PlayerBase player)
	{
		string key = player.GetIdentity().GetId();

		dmAISurvivor bot;
		if (s_TestBotByPlayer.Find(key, bot)) return bot;
		return null;
	}

	//! Bind a bot to the player.
	static void BindBot(PlayerBase player, dmAISurvivor bot)
	{
		#ifdef DM_BOT_DEBUG_PATHFINDER
		bot.m_DebugPlayer = player;
		#endif
		string key = player.GetIdentity().GetId();
		s_TestBotByPlayer.Set(key, bot);
	}

	//! Create a state for the FSM draft by name.
	static dmBotState MakeState(string name)
	{
		if (name == DM_CHAT_FSM_IDLE)
			return new dmBotState_Idle();
		if (name == DM_CHAT_FSM_PATROL)
			return new dmBotState_Patrol();
		if (name == DM_CHAT_FSM_STEALTH)
		{
			dmBotState_Stealth stealth = new dmBotState_Stealth();
			stealth.m_CoverPosition = s_DraftStealthCover;
			return stealth;
		}
		return null;
	}

	//! Build an FSM from the draft (auto-connect all pairs) and apply it to the bot.
	static void ApplyDraftFSM(dmAISurvivor bot)
	{
		dmBotFSM fsm = new dmBotFSM(bot);
		ref array<ref dmBotState> states = new array<ref dmBotState>();
		int i;
		for (i = 0; i < s_DraftStates.Count(); i++)
		{
			dmBotState state = MakeState(s_DraftStates[i]);
			if (state)
			{
				fsm.AddState(state, s_DraftStates[i]);
				states.Insert(state);
			}
		}

		int j;
		for (i = 0; i < states.Count(); i++)
		{
			for (j = 0; j < states.Count(); j++)
			{
				if (i != j)
					states[i].AddTransition(states[j], 1.0);
			}
		}

		if (states.Count() > 0)
			fsm.SetDefaultState(states[0].GetName());

		fsm.Start();
		bot.SetFSM(fsm);
	}

	//! Rotate a horizontal direction around the up axis (counter-clockwise positive).
	static vector RotateHorizontal(vector dir, float degrees)
	{
		float rad = degrees * Math.DEG2RAD;
		float c = Math.Cos(rad);
		float s = Math.Sin(rad);
		return Vector(dir[0] * c - dir[2] * s, 0.0, dir[0] * s + dir[2] * c);
	}

	//! Random horizontal offset within a radius (random direction + distance).
	static vector RandomHorizontalOffset(float radius)
	{
		float angle = Math.RandomFloat(0.0, 360.0);
		float dist = Math.RandomFloat(0.0, radius);
		float rad = angle * Math.DEG2RAD;
		return Vector(Math.Cos(rad) * dist, 0.0, Math.Sin(rad) * dist);
	}

	//! Give the bot a short MoveTo command one meter toward the player. Used right
	//! after spawn as a "kick": a stationary AI character isn't re-evaluated for
	//! ground collision/fall by the engine, so this real movement wakes its physics
	//! and lets a bot spawned in the air fall to the ground.
	static void GiveMoveKick(dmAISurvivor bot, PlayerBase player)
	{
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		vector target = bot.GetPosition() - fwd * 1.0; // 1 m toward the player

		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Goal = target;
		move.m_Priority = dmBotIntentPriority.CRITICAL;
		move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		bot.AddCommandIntent(move);
	}
}
