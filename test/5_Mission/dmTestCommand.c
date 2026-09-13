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
//!   /test bot bandaging — перевязка: бот останавливает кровотечение (см. dmBotMedicalTest.c).
//!   /test bot splinting — шина: бот лечит перелом (GetBrokenLegs()==BROKEN_LEGS_SPLINT).
//!   /test bot painkiller — обезбол после шины (MDF_PAINKILLERS).
//!   /test bot painkillerbandage — обезбол после перевязки при HP<75% (MDF_PAINKILLERS).
//!   /test bot charcoal — уголь при отравлении (MDF_CHARCOAL).
//!   /test bot tetracycline — антибиотик при гриппе (MDF_ANTIBIOTICS).
//!   /test bot vitamins — витамины при холоде (MDF_IMMUNITYBOOST).
//!   /test bot death    — Health=0: бот умирает и удаляется из мира.
//!   /test bot target   — реактивная угроза: зомби → RegisterDamageThreat → hostile → null.
//!   /test bot shoot {N} — стрельба: заряженный АКМ + угроза → Shooting → патроны
//!                                убывают; {N} — дистанция спавна бота от игрока в метрах.
//!   /test bot aim {N}   — лестница точности: бот с пустым АКМ + 10 магазинов по
//!                                5 патронов стреляет одиночными по мишени-болванке на
//!                                50,100,... до N (или дистанции взгляда) метров; метрика —
//!                                выстрелов до убийства; проверяются переходы Idle<->Shooting
//!                                и перезарядка. {N} — максимальная дистанция в метрах.
//!   /test bot enemy {N}  — заспавнить вооружённого бота в N метрах перед игроком
//!                                (лицом к игроку), одетого в горку, с B95 + патронами
//!                                .308; игрок-отправитель — враг бота.
//!   /test bot emote {id} — эмоция: бот играет жест по EmoteConstants ID (напр.
//!                                44=salute, 12=dance, 40=point).
//!   /test bot fight    — бой: бот с Machete отбивает 3 волны зомби (по одному
//!                                спереди и сзади); проверяет состояние Fighting и
//!                                пере-таргетинг между волнами.
//!   /test bot eat       — еда: бот с обнулённой энергией и яблоком сам ест.
//!   /test bot drink     — питьё: бот с обнулённой водой и полной бутылкой сам пьёт.
//!   /test bot eatcan    — консерва: бот с обнулённой энергией и закрытой PeachesCan
//!                                открывает банку (PeachesCan_Opened) и съедает.
//!   /test bot weapon load — зарядка оружия: B95 (пачка .308) и M4 (магазин STANAG).
//!   /test bot weapon selection — выбор оружия: игрок→огнестрел, зомби→мили.
//!   /test bot suppressor — глушители: сила шума выстрела по типу глушителя
//!                                (без 3000м, самодельный 150м, автоматный 100м,
//!                                пистолетный 75м) через dmBotGunshotNoiseStrength().
//!   /test bot trajectory {N} — баллистика: мосинка + 1 патрон, идеальный прицел,
//!                                цель на N м (по умолчанию 500), один выстрел;
//!                                дельта времени полёта читается из лога [Ballistics]
//!                                (FIRE → HIT/IMPACT).
//!   /test bot leadshoot {N} — диагностика упреждения: мосинка + оптика, цель
//!                                (полный бот) бежит спринтом 200 м поперёк прицела
//!                                на N м; прицел ведётся в текущую позицию (упреждения
//!                                нет) — ожидается промах; метрика — попадания.
//!   /test bot flytime {N} — диагностика времени полёта: два бота по очереди стреляют
//!                                в маркер на земле (N м) и в болванку (N+2 м); дельта
//!                                FIRE→IMPACT читается из лога [Ballistics] (все аргументы
//!                                FirearmEffects). Выявляет «мгновенное» попадание.
//!   /test bot voice        — автотест центра воспроизведения реплик: категоризатор/
//!                                выбиратор/кулдаун молчания (3 фазы, без аудио).
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
			return HandleCancel(player, parts);

		//! /test bot patrol | overload | shock | stamina | brokenleg | bandaging | splinting | painkiller | painkillerbandage | charcoal | tetracycline | vitamins | death | target | shoot | aim | enemy | emote | fight | eat | drink | eatcan | weapon load | weapon selection | suppressor | trajectory {N} | leadshoot {N} | flytime {N} | voice
		if (parts.Count() < 3)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи сценарий: /test bot patrol | overload | shock | stamina | brokenleg | bandaging | splinting | painkiller | painkillerbandage | charcoal | tetracycline | vitamins | death | target | shoot {N} | aim {N} | enemy {N} | emote {id} | fight | eat | drink | eatcan | weapon load | weapon selection | suppressor | trajectory {N} | leadshoot {N} | flytime {N} | voice");
			return false;
		}

		if (parts[1] != DM_CHAT_CMD)
			return false;

		if (parts[2] == DM_CHAT_FSM_PATROL)
			return HandlePatrol(player);

		if (parts[2] == DM_CHAT_TEST_OVERLOAD)
			return HandleOverload(player, parts);

		if (parts[2] == DM_CHAT_TEST_SHOCK)
			return HandleTestCase(player, new dmBotTest_Shock());
		if (parts[2] == DM_CHAT_TEST_STAMINA)
			return HandleTestCase(player, new dmBotTest_Stamina());
		if (parts[2] == DM_CHAT_TEST_BROKENLEG)
			return HandleTestCase(player, new dmBotTest_BrokenLeg());
		if (parts[2] == DM_CHAT_TEST_BANDAGING)
			return HandleTestCase(player, new dmBotTest_Bandaging());
		if (parts[2] == DM_CHAT_TEST_SPLINTING)
			return HandleTestCase(player, new dmBotTest_Splinting());
		if (parts[2] == DM_CHAT_TEST_PAINKILLER)
			return HandleTestCase(player, new dmBotTest_Painkiller());
		if (parts[2] == DM_CHAT_TEST_PAINKILLER_BANDAGE)
			return HandleTestCase(player, new dmBotTest_PainkillerBandage());
		if (parts[2] == DM_CHAT_TEST_CHARCOAL)
			return HandleTestCase(player, new dmBotTest_Charcoal());
		if (parts[2] == DM_CHAT_TEST_TETRACYCLINE)
			return HandleTestCase(player, new dmBotTest_Tetracycline());
		if (parts[2] == DM_CHAT_TEST_VITAMINS)
			return HandleTestCase(player, new dmBotTest_Vitamins());
		if (parts[2] == DM_CHAT_TEST_DEATH)
			return HandleTestCase(player, new dmBotTest_Death());
		if (parts[2] == DM_CHAT_TEST_TARGET)
			return HandleTestCase(player, new dmBotTest_Target());
		if (parts[2] == DM_CHAT_TEST_SHOOT)
			return HandleShootTest(player, parts);
		if (parts[2] == DM_CHAT_TEST_AIM)
			return HandleAimTest(player, parts);
		if (parts[2] == DM_CHAT_TEST_EMOTE)
			return HandleEmoteTest(player, parts);
		if (parts[2] == DM_CHAT_TEST_FIGHT)
			return HandleTestCase(player, new dmBotTest_Fight());
		if (parts[2] == DM_CHAT_TEST_EAT)
			return HandleTestCase(player, new dmBotTest_Eat());
		if (parts[2] == DM_CHAT_TEST_DRINK)
			return HandleTestCase(player, new dmBotTest_Drink());
		if (parts[2] == DM_CHAT_TEST_EATCAN)
			return HandleTestCase(player, new dmBotTest_EatCan());
		if (parts[2] == DM_CHAT_TEST_WEAPON)
			return HandleWeaponTest(player, parts);
		if (parts[2] == DM_CHAT_TEST_LOOTING)
			return HandleLootingTest(player, parts);
		if (parts[2] == DM_CHAT_TEST_SUPPRESSOR)
			return HandleTestCase(player, new dmBotTest_Suppressor());
		if (parts[2] == DM_CHAT_TEST_TRAJECTORY)
			return HandleTrajectoryTest(player, parts);
		if (parts[2] == DM_CHAT_TEST_LEADSHOOT)
			return HandleLeadShootTest(player, parts);
		if (parts[2] == DM_CHAT_TEST_FLYTIME)
			return HandleFlytimeTest(player, parts);

		if (parts[2] == DM_CHAT_TEST_ENEMY)
			return HandleEnemy(player, parts);

		if (parts[2] == DM_CHAT_TEST_VOICE)
			return HandleTestCase(player, new dmBotTest_Voice());

		dmCommandManager.ChatToPlayer(player, "Неизвестный сценарий: " + parts[2]);
		return false;
	}

	//! Launch a self-verifying scenario through dmTestSuite_TestRunner.
	private bool HandleTestCase(PlayerBase player, dmTestSuite_TestCase test)
	{
		dmTestSuite_TestRunner.Start(test, player);
		return true;
	}

	//! /test bot weapon load | selection — weapon loading/selection scenarios.
	private bool HandleWeaponTest(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 4)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи: /test bot weapon load | selection");
			return false;
		}

		if (parts[3] == DM_CHAT_TEST_WEAPON_LOAD)
			return HandleTestCase(player, new dmBotTest_WeaponLoad());
		if (parts[3] == DM_CHAT_TEST_WEAPON_SELECTION)
			return HandleTestCase(player, new dmBotTest_WeaponSelection());

		dmCommandManager.ChatToPlayer(player, "Неизвестный сценарий weapon: " + parts[3]);
		return false;
	}

	//! /test bot looting get|change — сценарии лута.
	private bool HandleLootingTest(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 4)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи: /test bot looting get | change");
			return false;
		}
		if (parts[3] == DM_CHAT_TEST_LOOTING_GET)
			return HandleTestCase(player, new dmBotTest_LootingGet());
		if (parts[3] == DM_CHAT_TEST_LOOTING_CHANGE)
			return HandleTestCase(player, new dmBotTest_LootingChange());
		dmCommandManager.ChatToPlayer(player, "Неизвестный сценарий looting: " + parts[3]);
		return false;
	}

	//! /test bot shoot {N} — firing test; N (meters) is the optional spawn distance
	//! of the BOT from the player (0/default = DM_SPAWN_DISTANCE).
	private bool HandleShootTest(PlayerBase player, array<string> parts)
	{
		dmBotTest_Shoot test = new dmBotTest_Shoot();
		if (parts.Count() >= 4)
		{
			int dist = parts[3].ToInt();
			if (dist > 0)
				test.SetSpawnDistance(dist);
		}
		return HandleTestCase(player, test);
	}

	//! /test bot aim {N} — aim-accuracy ladder; N (meters) is the optional maximum
	//! target distance (0/default = DM_AIM_TEST_MAX_DIST).
	private bool HandleAimTest(PlayerBase player, array<string> parts)
	{
		dmBotTest_Aim test = new dmBotTest_Aim();
		if (parts.Count() >= 4)
		{
			int dist = parts[3].ToInt();
			if (dist > 0)
				test.SetMaxDistance(dist);
		}
		return HandleTestCase(player, test);
	}

	//! /test bot trajectory {N} — ballistic flight-time test; N (meters) is the
	//! optional target distance (0/default = DM_TRAJECTORY_TEST_DISTANCE).
	private bool HandleTrajectoryTest(PlayerBase player, array<string> parts)
	{
		dmBotTest_Trajectory test = new dmBotTest_Trajectory();
		if (parts.Count() >= 4)
		{
			int dist = parts[3].ToInt();
			if (dist > 0)
				test.SetTargetDistance(dist);
		}
		return HandleTestCase(player, test);
	}

	//! /test bot leadshoot {N} — running-target lead observation; N (meters) is the
	//! optional crossing distance (0/default = DM_LEADSHOOT_TEST_DISTANCE).
	private bool HandleLeadShootTest(PlayerBase player, array<string> parts)
	{
		dmBotTest_LeadShoot test = new dmBotTest_LeadShoot();
		if (parts.Count() >= 4)
		{
			int dist = parts[3].ToInt();
			if (dist > 0)
				test.SetTargetDistance(dist);
		}
		return HandleTestCase(player, test);
	}

	//! /test bot flytime {N} — bullet flight-time diagnostic; N (meters) is the
	//! optional aim distance on the ground (0/default = DM_FLYTIME_TEST_DISTANCE).
	private bool HandleFlytimeTest(PlayerBase player, array<string> parts)
	{
		dmBotTest_Flytime test = new dmBotTest_Flytime();
		if (parts.Count() >= 4)
		{
			int dist = parts[3].ToInt();
			if (dist > 0)
				test.SetTargetDistance(dist);
		}
		return HandleTestCase(player, test);
	}

	//! /test bot emote {id} — бот играет эмоцию по EmoteConstants ID (напр. 44=salute).
	private bool HandleEmoteTest(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 4)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи ID эмоции: /test bot emote {id} (напр. 44=salute, 12=dance, 40=point)");
			return false;
		}
		int id = parts[3].ToInt();
		if (id <= 0)
		{
			dmCommandManager.ChatToPlayer(player, "Некорректный ID эмоции: " + parts[3]);
			return false;
		}
		dmBotTest_Emote test = new dmBotTest_Emote();
		test.SetEmoteID(id);
		return HandleTestCase(player, test);
	}

	//! /test cancel [all|last] — отменить все тесты игрока или только последний.
	private bool HandleCancel(PlayerBase player, array<string> parts)
	{
		if (parts.Count() >= 3 && parts[2] == DM_CHAT_TEST_CANCEL_ALL)
		{
			dmTestSuite_TestRunner.CancelAll(player);
			return true;
		}
		// "/test cancel" и "/test cancel last" — последний тест игрока.
		dmTestSuite_TestRunner.CancelLast(player);
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

		vector playerPos = player.GetPosition();
		vector spawnPos = playerPos + fwd * DM_SPAWN_DISTANCE;

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

	//! /test bot enemy {N} — spawn a bot N meters in front of the player (facing
	//! back at the player), dress it in Gorka gear, arm it with a B95 + .308 ammo,
	//! give it the Shooting test preset and mark the player hostile.
	private bool HandleEnemy(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 4)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи дистанцию: /test bot enemy {N}");
			return false;
		}

		int dist = parts[3].ToInt();
		if (dist <= 0)
		{
			dmCommandManager.ChatToPlayer(player, "Некорректная дистанция: " + parts[3]);
			return false;
		}

		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();

		vector playerPos = player.GetPosition();
		vector spawnPos = SnapToGroundExactly(playerPos + fwd * (float)dist);

		//! Бот спавнится ПЕРЕД игроком — развернуть его лицом к игроку.
		vector back = -fwd;
		float yaw = back.VectorToAngles()[0];

		ref dmAISurvivor bot = new dmAISurvivor();
		PlayerBase pawn = bot.Spawn(spawnPos, Vector(yaw, 0.0, 0.0));
		if (!pawn)
		{
			dmCommandManager.ChatToPlayer(player, "Не удалось заспавнить бота.");
			return false;
		}

		dmCommandContext.BindBot(player, bot);

		//! Одежда (слоты одежды/обуви/перчаток).
		pawn.GetInventory().CreateInInventory("GorkaPants_Autumn");
		pawn.GetInventory().CreateInInventory("GorkaEJacket_Autumn");
		pawn.GetInventory().CreateInInventory("TacticalGloves_Black");
		pawn.GetInventory().CreateInInventory("CombatBoots_Black");

		//! B95 в руках + досыл патрона в патронник/внутренний магазин.
		Weapon_Base b95 = Weapon_Base.Cast(pawn.GetHumanInventory().CreateInHands("B95"));
		if (b95)
			b95.SpawnAmmo("Ammo_308Win", WeaponWithAmmoFlags.CHAMBER);

		//! Запас ~40 патронов (2 пачки .308 в карго одетой одежды).
		pawn.GetInventory().CreateInInventory("Ammo_308Win");
		pawn.GetInventory().CreateInInventory("Ammo_308Win");

		bot.SetFSM(dmBotTestPreset_Hunting.Create(bot));
		bot.RegisterHostile(player, 1.0, 50.0);

		dmCommandManager.ChatToPlayer(player, "Враг заспавнен в " + dist + " м от тебя (B95 + .308), ты для него враждебен.");
		return true;
	}
}
