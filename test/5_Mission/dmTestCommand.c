//! dmTestCommand — команды "/test ...": скриптовые тестовые сценарии.
//!
//!   /test bot patrol          — заспавнить бота с патрульным маршрутом из трёх
//!                                точек, который заставляет его поворачивать:
//!                                назад 50 м, вправо 90° на 50 м, вправо 135° на 100 м.
//!   /test bot overload prepare {N} — заспавнить N ботов в радиусе 15 м от игрока;
//!                                каждому дать 25 случайных точек патруля в радиусе
//!                                100 м и случайную скорость; каждый 10-й бот держит
//!                                взгляд на игроке (10 минут). Боты пока без FSM.
//!   /test bot overload run    — собрать и запустить FSM (patrol + idle) на всех
//!                                подготовленных ботах: с этого момента они
//!                                начинают патрулировать.
//!
//! Самопроверяемые тесты честной симуляции тела (см. dmBotTest.c): при запуске
//! в чат приходит описание + ожидаемый результат, а по таймеру скрипт фиксирует
//! состояние бота и сравнивает его с ожидаемым (PASS/FAIL):
//!
//!   /test bot shock    — нокаут: бот без сознания, не двигается, приходит в себя.
//!   /test bot stamina  — тяжёлый рюкзак (NailBox) + бег 300 м: вес режет кап, стамина тратится.
//!   /test bot brokenleg — перелом: бот хромает, не спринтует.
//!   /test bot death    — Health=0: бот умирает и удаляется из мира.
//!   /test bot target   — реактивная угроза: зомби → RegisterDamageThreat → hostile → null.
//!   /test cancel       — прервать работающий тест и удалить его бота.

class dmTestCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_CMD_TEST;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		//! /test cancel — abort the running test
		if (parts.Count() >= 2 && parts[1] == DM_CHAT_TEST_CANCEL)
			return HandleCancel(player);

		//! /test bot patrol | overload | shock | stamina | brokenleg | death | target
		if (parts.Count() < 3)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи сценарий: /test bot patrol | overload | shock | stamina | brokenleg | death | target");
			return false;
		}

		if (parts[1] != DM_CHAT_CMD)
			return false;

		if (parts[2] == DM_CHAT_FSM_PATROL)
			return HandlePatrol(player);

		if (parts[2] == DM_CHAT_TEST_OVERLOAD)
			return HandleOverload(player, parts);

		if (parts[2] == DM_CHAT_TEST_SHOCK)
			return HandleBodyTest(player, new dmBotTest_Shock());
		if (parts[2] == DM_CHAT_TEST_STAMINA)
			return HandleBodyTest(player, new dmBotTest_Stamina());
		if (parts[2] == DM_CHAT_TEST_BROKENLEG)
			return HandleBodyTest(player, new dmBotTest_BrokenLeg());
		if (parts[2] == DM_CHAT_TEST_DEATH)
			return HandleBodyTest(player, new dmBotTest_Death());
		if (parts[2] == DM_CHAT_TEST_TARGET)
			return HandleBodyTest(player, new dmBotTest_Target());

		dmCommandManager.ChatToPlayer(player, "Неизвестный сценарий: " + parts[2]);
		return false;
	}

	//! Launch a self-verifying body-simulation scenario through dmBotTestRunner.
	private bool HandleBodyTest(PlayerBase player, dmBotTestCase test)
	{
		dmBotTestRunner.GetInstance().Start(test, player);
		return true;
	}

	//! /test cancel — abort the running test and clean up after it.
	private bool HandleCancel(PlayerBase player)
	{
		dmBotTestRunner.GetInstance().Cancel(player);
		return true;
	}

	//! /test bot overload prepare {N} | run
	private bool HandleOverload(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 4)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи: /test bot overload prepare|run");
			return false;
		}

		if (parts[3] == DM_CHAT_TEST_PREPARE)
			return HandleOverloadPrepare(player, parts);
		if (parts[3] == DM_CHAT_TEST_RUN)
			return HandleOverloadRun(player);

		dmCommandManager.ChatToPlayer(player, "Неизвестная команда overload: " + parts[3]);
		return false;
	}

	//! Prepare N bots: random spawn within 15m, 25 random patrol points within 100m,
	//! random preferred speed, 1/10 chance to keep looking at the player (10 min).
	private bool HandleOverloadPrepare(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 5)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи число ботов: /test bot overload prepare {N}");
			return false;
		}

		int count = parts[4].ToInt();
		if (count <= 0)
		{
			dmCommandManager.ChatToPlayer(player, "Некорректное число ботов: " + parts[4]);
			return false;
		}

		dmCommandContext.s_OverloadBots.Clear();

		vector center = player.GetPosition();

		int i;
		for (i = 0; i < count; i++)
		{
			vector spawnPos = center + dmCommandContext.RandomHorizontalOffset(DM_TEST_OVERLOAD_SPAWN_RADIUS);

			ref dmAISurvivor bot = new dmAISurvivor();
			PlayerBase pawn = bot.Spawn(spawnPos, Vector(Math.RandomFloat(0.0, 360.0), 0.0, 0.0));
			if (!pawn)
				continue;

			int j;
			for (j = 0; j < DM_TEST_OVERLOAD_POINTS; j++)
			{
				vector point = center + dmCommandContext.RandomHorizontalOffset(DM_TEST_OVERLOAD_PATROL_RADIUS);
				bot.AddPatrolPoint(point);
			}

			bot.SetPreferredSpeed(Math.RandomIntInclusive(1, 3));

			if (Math.RandomIntInclusive(0, 9) == 0)
			{
				dmBotIntent_HoldLook look = new dmBotIntent_HoldLook();
				look.m_Entity = player;
				look.m_Turn = dmBotLookTurn.FULL;
				look.m_Priority = dmBotIntentPriority.CRITICAL;
				look.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
				look.m_Deadline = DM_TEST_OVERLOAD_LOOK_DEADLINE;
				bot.AddCommandIntent(look);
			}

			dmCommandContext.s_OverloadBots.Insert(bot);
		}

		dmCommandManager.ChatToPlayer(player, "Подготовлено ботов: " + dmCommandContext.s_OverloadBots.Count());
		return true;
	}

	//! Build and start an FSM (patrol + idle) on every prepared overload bot.
	private bool HandleOverloadRun(PlayerBase player)
	{
		int count = dmCommandContext.s_OverloadBots.Count();

		int i;
		for (i = 0; i < count; i++)
		{
			dmAISurvivor bot = dmCommandContext.s_OverloadBots[i];

			dmBotFSM fsm = new dmBotFSM(bot);
			dmBotState patrol = new dmBotState_Patrol();
			dmBotState idle = new dmBotState_Idle();
			fsm.AddState(patrol, "patrol");
			fsm.AddState(idle, "idle");
			patrol.AddTransition(idle, 1.0);
			idle.AddTransition(patrol, 1.0);
			fsm.SetDefaultState("patrol");
			fsm.Start();
			bot.SetFSM(fsm);
		}

		dmCommandManager.ChatToPlayer(player, "Запущено ботов: " + count);
		return true;
	}

	//! Test scenario: spawn a bot in front of the player and give it a patrol route
	//! that forces turns — 50m behind, then right 90° for 50m, then right 135° for 100m.
	private bool HandlePatrol(PlayerBase player)
	{
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();

		vector spawnPos = player.GetPosition() + fwd * DM_SPAWN_DISTANCE;

		ref dmAISurvivor bot = new dmAISurvivor();
		PlayerBase pawn = bot.Spawn(spawnPos, player.GetOrientation());
		if (!pawn)
		{
			dmCommandManager.ChatToPlayer(player, "Не удалось заспавнить бота.");
			return false;
		}

		dmCommandContext.BindBot(player, bot);

		//! Patrol route (right turns = negative rotation around the up axis).
		vector behind = fwd * -1.0;
		vector p1 = spawnPos + behind * 50.0;
		vector d2 = dmCommandContext.RotateHorizontal(behind, -90.0);
		vector p2 = p1 + d2 * 50.0;
		vector d3 = dmCommandContext.RotateHorizontal(d2, -135.0);
		vector p3 = p2 + d3 * 100.0;

		bot.AddPatrolPoint(p1);
		bot.AddPatrolPoint(p2);
		bot.AddPatrolPoint(p3);

		dmBotFSM fsm = new dmBotFSM(bot);
		dmBotState patrol = new dmBotState_Patrol();
		dmBotState idle = new dmBotState_Idle();
		fsm.AddState(patrol, "patrol");
		fsm.AddState(idle, "idle");
		patrol.AddTransition(idle, 1.0);
		idle.AddTransition(patrol, 1.0);
		fsm.SetDefaultState("patrol");
		fsm.Start();
		bot.SetFSM(fsm);

		dmCommandManager.ChatToPlayer(player, "Тест-сценарий patrol запущен (3 точки, повороты)");
		return true;
	}
}
