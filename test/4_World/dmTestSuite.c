//! dmTestSuite — каркас самопроверяющихся сценариев (параллельные тесты).
//!
//! dmTestSuite_TestCase — родитель сценария (спавн-хелперы врагов, общие утилиты);
//! dmTestSuite_TestRunner — запускает сценарий и поллит его по таймеру. Реестр
//! s_Active держит все активные запуски (ленивая чистка — SweepFinished).

//! Родитель всех сценариев теста: держит бота/игрока и общие спавн-хелперы.
class dmTestSuite_TestCase
{
	ref dmAISurvivor m_Bot;
	PlayerBase m_Player;
	ref dmTestSuite_TestRunner m_Runner;   // runner, который запустил тест (ставит Start)

	float m_SpawnDistance = 0.0;
	float m_TestRange = 0.0;

	//! Дистанция от игрока, где спавнится БОТ-тестировщик; 0 = DM_SPAWN_DISTANCE.
	float GetSpawnDistance()
	{
		if (m_SpawnDistance > 0.0) return m_SpawnDistance;
		return DM_SPAWN_DISTANCE;
	}

	//! Дистанция от бота до края тестовой зоны (там спавнятся ВРАГИ); 0 = DM_TEST_RANGE_DISTANCE.
	float GetTestRange()
	{
		if (m_TestRange > 0.0) return m_TestRange;
		return DM_TEST_RANGE_DISTANCE;
	}

	//! Переопределить спавн-дистанцию (0 = дефолт).
	void SetSpawnDistance(float v)
	{
		m_SpawnDistance = v;
	}

	//! Переопределить дистанцию тестовой зоны (0 = дефолт).
	void SetTestRange(float v)
	{
		m_TestRange = v;
	}

	//! Ставит m_Bot/m_Player. Переопределения ОБЯЗАНЫ звать super.Setup(bot, player).
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

	//! Спавн вражеской болванки-игрока на GetTestRange() от бота (по направлению взгляда игрока), на земле, враждебной.
	EntityAI SpawnEnemy()
	{
		vector origin = m_Bot.GetPosition();
		vector dir = m_Player.GetDirection();
		dir[1] = 0.0;
		dir.Normalize();
		vector pos = origin + dir * GetTestRange();
		return SpawnHostile(dmSurvivor.GetRandom(), pos);
	}

	//! Спавн болванки-игрока на `distance` метров от бота (по взгляду бота; distance<0 — сзади).
	EntityAI SpawnEnemyNearBot(float distance)
	{
		vector origin = m_Bot.GetPosition();
		vector dir = m_Bot.GetDirection();
		dir[1] = 0.0;
		dir.Normalize();
		vector pos = origin + dir * distance;
		return SpawnHostile(dmSurvivor.GetRandom(), pos);
	}

	//! Спавн зомби на GetTestRange() от бота (по взгляду игрока), на земле, враждебного.
	EntityAI SpawnZombie()
	{
		vector origin = m_Bot.GetPosition();
		vector dir = m_Player.GetDirection();
		dir[1] = 0.0;
		dir.Normalize();
		vector pos = origin + dir * GetTestRange();
		return SpawnHostile("ZmbM_PatrolNormal_Autumn", pos);
	}

	//! Спавн зомби на `distance` метров от бота (по взгляду бота; distance<0 — сзади).
	EntityAI SpawnZombieNearBot(float distance)
	{
		vector origin = m_Bot.GetPosition();
		vector dir = m_Bot.GetPawn().GetDirection();
		dir[1] = 0.0;
		dir.Normalize();
		vector pos = origin + dir * distance;
		return SpawnHostile("ZmbM_PatrolNormal_Autumn", pos);
	}

	//! (private) Создать объект класса `cls` в `pos` и зарегистрировать враждебным; вернуть или null.
	private EntityAI SpawnHostile(string cls, vector pos)
	{
		pos = SnapToGroundExactly(pos);
		EntityAI e = EntityAI.Cast(GetGame().CreateObject(cls, pos, false));
		if (e)
		{
			dmTarget t = m_Bot.RegisterHostile(e, 1.0);
			t.m_LastPosition = pos;
		}
		return e;
	}

	//! Точка на `distance` метров впереди игрока (по горизонтальному взгляду), на земле.
	vector ForwardTarget(float distance)
	{
		vector fwd = m_Player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		vector origin = m_Player.GetPosition();
		vector target = origin + fwd * distance;
		return SnapToGroundExactly(target);
	}

	//! Имя текущего состояния FSM бота ("none", если FSM/состояние недоступны).
	string CurrentStateName()
	{
		dmBotFSM fsm = m_Bot.GetFSM();
		if (fsm && fsm.GetCurrentState())
			return fsm.GetCurrentState().GetName();
		return "none";
	}

	//! Горизонтальное расстояние между двумя точками (игнорирует высоту).
	float HorizontalMove(vector start, vector current)
	{
		vector d = current - start;
		d[1] = 0.0;
		return d.Length();
	}

	//! Trim a float to 3 decimals for readable chat output.
	string Fmt(float v)
	{
		return (Math.Round(v * 1000.0) / 1000.0).ToString();
	}
}

//! Запускает один сценарий (параллельно с другими) и поллит его по таймеру.
class dmTestSuite_TestRunner
{
	ref dmTestSuite_TestCase m_Test;
	ref dmAISurvivor m_Bot;
	PlayerBase m_Player;

	float m_Elapsed;
	bool m_Running;

	static ref array<ref dmTestSuite_TestRunner> s_Active = new array<ref dmTestSuite_TestRunner>;

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

		test.m_Runner = runner;
		SweepFinished();
		s_Active.Insert(runner);

		return runner;
	}

	static vector ForwardTarget(PlayerBase player, float distance)
	{
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		vector origin = player.GetPosition();
		vector target = origin + fwd * distance;
		return SnapToGroundExactly(target);
	}

	static void GiveMoveKick(dmAISurvivor bot, PlayerBase player)
	{
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		vector botPos = bot.GetPosition();
		vector target = SnapToGroundExactly(botPos - fwd * 1.0); // 1 m toward the player

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

	//! Удалить из реестра все завершённые (m_Running == false) runner'ы.
	static void SweepFinished()
	{
		int i;
		for (i = s_Active.Count() - 1; i >= 0; i--)
		{
			if (!s_Active[i].m_Running)
				s_Active.Remove(i);
		}
	}

	//! Отменить все запущенные тесты игрока `player`.
	static void CancelAll(PlayerBase player)
	{
		SweepFinished();
		int i;
		for (i = s_Active.Count() - 1; i >= 0; i--)
		{
			if (s_Active[i].m_Running && s_Active[i].m_Player == player)
				s_Active[i].Cancel(player);
		}
		SweepFinished();
	}

	//! Отменить последний запущенный тест игрока `player` (или ничего, если нет).
	static void CancelLast(PlayerBase player)
	{
		SweepFinished();
		int i;
		for (i = s_Active.Count() - 1; i >= 0; i--)
		{
			if (s_Active[i].m_Running && s_Active[i].m_Player == player)
			{
				s_Active[i].Cancel(player);
				break;
			}
		}
		SweepFinished();
	}

	//! Заменить бота теста (мультифазные тесты спавнят свежую пешку) — обновить и у теста, и у runner.
	void ReplaceBot(dmAISurvivor bot)
	{
		m_Bot = bot;
		if (m_Test)
			m_Test.m_Bot = bot;
	}
}

//! Прибить к земле, СОХРАНИВ относительную высоту `pos[1]` (SurfaceY + pos[1]).
vector SnapToGroundRelative(vector pos)
{
	float pos_x = pos[0];
	float pos_z = pos[2];
	float pos_y = g_Game.SurfaceY(pos_x, pos_z);
	vector tmp_pos = Vector(pos_x, pos_y, pos_z);
	tmp_pos[1] = tmp_pos[1] + pos[1];

	return tmp_pos;
}

//! Прибить СТРОГО к поверхности (SurfaceY, игнорируя pos[1]) — для спавна врагов/точек, чтобы не висели в воздухе.
vector SnapToGroundExactly(vector pos)
{
	float pos_x = pos[0];
	float pos_z = pos[2];
	float pos_y = g_Game.SurfaceY(pos_x, pos_z);
	vector tmp_pos = Vector(pos_x, pos_y, pos_z);

	return tmp_pos;
}
