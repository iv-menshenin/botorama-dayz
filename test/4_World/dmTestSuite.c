
class dmTestSuite_TestCase
{
	ref dmAISurvivor m_Bot;
	PlayerBase m_Player;

	float m_SpawnDistance = 0.0;
	float m_TestRange = 0.0;

	//! Spawn distance from the player (meters); 0 = DM_SPAWN_DISTANCE default.
	float GetSpawnDistance()
	{
		if (m_SpawnDistance > 0.0) return m_SpawnDistance;
		return DM_SPAWN_DISTANCE;
	}

	float GetTestRange()
	{
		if (m_TestRange > 0.0) return m_TestRange;
		return DM_TEST_RANGE_DISTANCE;
	}

    void SetSpawnDistance(float v)
    {
        m_SpawnDistance = v;
    }
    
    void SetTestRange(float v)
    {
        m_TestRange = v;
    }

	//! Apply the scenario-specific condition to the freshly spawned bot.
	void Setup(dmAISurvivor bot, PlayerBase player)
    {
        m_Bot = bot;
        m_Player = player;
    }

	//! One-line summary sent to chat on start (what is done + expected result).
	string GetSummary() { return ""; }

	//! Polled every GetInterval() seconds. See the file header for the contract.
	string OnCheck(float elapsed) { return ""; }

	//! Poll interval (seconds).
	float GetInterval() { return 1.0; }

	//! Total runtime before an automatic timeout-FAIL.
	float GetDuration() { return 15.0; }

	//! Equip a backpack and fill it with item.
	void GiveHeavyBackpack(string itemClass, int count = 1)
	{
		EntityAI bag = m_Bot.GetPawn().GetInventory().CreateInInventory("TortillaBag");
		if (!bag)
			return;

		GameInventory bagInv = bag.GetInventory();
		int i;
		for (i = 0; i < count; i++)
			bagInv.CreateInInventory( itemClass );
	}

	EntityAI SpawnEmeny()
	{
		vector pos = m_Player.GetPosition();
		vector dir = m_Player.GetDirection();
		dir[1] = 0.0;
		dir.Normalize();
		pos = SnapToGroundExactly(pos + dir * GetTestRange());

		EntityAI enemy = EntityAI.Cast(GetGame().CreateObject("dmAI_SurvivorM_Denis", SnapToGroundExactly(pos), false));
		if ( enemy )
			m_Bot.RegisterHostile(enemy, 1.0);
		return enemy;
	}

	//! Trim a float to 3 decimals for readable chat output.
	string Fmt(float v)
	{
		return (Math.Round(v * 1000.0) / 1000.0).ToString();
	}
}

class dmTestSuite_TestRunner
{
	ref dmTestSuite_TestCase m_Test;
	ref dmAISurvivor m_Bot;
    PlayerBase m_Player;

	float m_Elapsed;
	bool m_Running;

    void dmTestSuite_TestRunner(PlayerBase player, dmTestSuite_TestCase test)
    {
        m_Player = player;
        m_Test = test;
    }

	bool IsRunning()
	{
		return m_Running;
	}

    void ChatToPlayer(string msg)
    {
        dmCommandManager.ChatToPlayer(m_Player, msg);
    }

	//! Spawn the bot immediately, show the summary, then wait DM_TEST_QUIET_SECONDS
	//! before running Setup() + polling (via Begin).
	static dmTestSuite_TestRunner Start(dmTestSuite_TestCase test, PlayerBase player)
	{
        dmTestSuite_TestRunner runner = new dmTestSuite_TestRunner(player, test);

		vector spawnPos = ForwardTarget( player, test.GetSpawnDistance() );
		dmAISurvivor bot = new dmAISurvivor();
		PlayerBase pawn = bot.Spawn(spawnPos, Vector(0, 0, 0));
		if (!pawn)
		{
			runner.ChatToPlayer("Не удалось заспавнить бота для теста.");
			return null;
		}
		dmCommandContext.BindBot(player, bot);

		//! Kick: real movement wakes the bot's physics (a stationary AI character
		//! isn't re-evaluated for ground collision/fall, so it would float).
		GiveMoveKick(bot, player);

		runner.m_Bot = bot;
		runner.m_Elapsed = 0.0;
		runner.m_Running = true;

		runner.ChatToPlayer(test.GetSummary());
		runner.ChatToPlayer("Старт через " + DM_TEST_QUIET_SECONDS + " c...");

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(runner.Begin, (int)(DM_TEST_QUIET_SECONDS * 1000), false);

        return runner;
	}

	static vector ForwardTarget(PlayerBase player, float distance)
	{
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		return SnapToGroundExactly(player.GetPosition() + fwd * distance);
	}

	static void GiveMoveKick(dmAISurvivor bot, PlayerBase player)
	{
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		vector target = SnapToGroundExactly(bot.GetPosition() - fwd * 1.0); // 1 m toward the player

		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Goal = target;
		move.m_Priority = dmBotIntentPriority.CRITICAL;
		move.m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		bot.AddPersonalityIntent(move);
	}

	//! After the quiet delay: clear all intents (from the spawn kick), run Setup()
	//! and start polling.
	void Begin()
	{
		if (!m_Running || !m_Test || !m_Bot)
			return;

		m_Bot.ClearFSMIntents();
		m_Bot.ClearCommandIntents();
		m_Bot.ClearPersonalityIntents();

		m_Test.Setup(m_Bot, m_Player);

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.OnTick, (int)(m_Test.GetInterval() * 1000), true);
	}

	//! Poll the current test; report PASS/FAIL and stop when a verdict arrives.
	void OnTick()
	{
		if (!m_Running || !m_Test)
			return;

		m_Elapsed += m_Test.GetInterval();

		string result = m_Test.OnCheck(m_Elapsed);
		if (result != "")
		{
			ChatToPlayer(result);

			if (result.Length() >= 4 && (result.Substring(0, 4) == "PASS" || result.Substring(0, 4) == "FAIL"))
				Stop();
		}
		else if (m_Elapsed >= m_Test.GetDuration())
		{
			ChatToPlayer("FAIL: таймаут — ожидаемый результат не наступил за " + m_Test.GetDuration() + " c");
			Stop();
		}
	}

	void Stop()
	{
		if (m_Running)
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(this.OnTick);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(this.Begin);
		}
		m_Running = false;
		m_Test = null;
	}

	//! Abort the running test (during quiet delay or the run): stop timers and
	//! despawn the test bot.
	void Cancel(PlayerBase player)
	{
		if (!m_Running)
		{
			ChatToPlayer("Нет запущенного теста.");
			return;
		}

		Stop();

		if (m_Bot && m_Bot.IsSpawned())
			m_Bot.Despawn();
		m_Bot = null;

		ChatToPlayer("Тест отменён, бот удалён.");
	}
}

vector SnapToGroundRelative(vector pos)
{
    float pos_x = pos[0];
    float pos_z = pos[2];
    float pos_y = g_Game.SurfaceY(pos_x, pos_z);
    vector tmp_pos = Vector(pos_x, pos_y, pos_z);
    tmp_pos[1] = tmp_pos[1] + pos[1];

    return tmp_pos;
}

vector SnapToGroundExactly(vector pos)
{
    float pos_x = pos[0];
    float pos_z = pos[2];
    float pos_y = g_Game.SurfaceY(pos_x, pos_z);
    vector tmp_pos = Vector(pos_x, pos_y, pos_z);

    return tmp_pos;
}