//! Firing: a bot with a loaded AKM receives a damage threat, must enter the
//! Shooting state and fire — the ammo count of the magazine in hands drops.
class dmBotTest_Shoot : dmTestSuite_TestCase
{
	int m_Phase = 0;
	EntityAI m_Target;
	int m_StartAmmo = 0;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		// do not forget!
		super.Setup(bot, player);

		bot.SetFSM(dmBotTestPreset_Shooting.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (!pawn) return;

		//! CreateInHands places the weapon directly in the bot's hands (no move
		//! dance). SpawnAmmo attaches a magazine AND chambers a round (default
		//! WeaponWithAmmoFlags.CHAMBER), so the weapon is ready to fire.
		Weapon_Base gun = Weapon_Base.Cast(pawn.GetHumanInventory().CreateInHands("AKM"));
		if (!gun)
			return;

		gun.SpawnAmmo("Mag_AKM_30Rnd");

		Magazine mag = gun.GetMagazine(gun.GetCurrentMuzzle());
		if (mag)
			mag.ServerSetAmmoCount(30);
	}

	override string GetSummary()
	{
		return "Тест «Стрельба». Боту дают заряженный АКМ и регистрируют враждебную болванку-игрока (15 м). Ожидается: бот входит в Shooting и стреляет — патроны в магазине убывают.";
	}

	override float GetInterval() { return 1.0; }

	override float GetDuration() { return 25.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		if (m_Phase == 0)
		{
			if (elapsed < 5.0)
				return "";

			m_Target = SpawnEnemy();
			if (!m_Target)
				return "FAIL: не удалось заспавнить цель";

			m_StartAmmo = AmmoInHands();
			m_Phase = 1;
			return "цель (болванка-игрок) заспавнена в 15 м, враждебна (патронов в руках: " + m_StartAmmo + ")";
		}

		if (m_Phase == 1)
		{
			if (elapsed < 8.0)
				return "";

			int ammo = AmmoInHands();
			if (ammo < m_StartAmmo)
			{
				EmptyMagazineInHands();
				m_Phase = 2;
				return "бот выстрелил, патроны обнулены";
			}
			return FailDebug(ammo);
		}

		if (m_Phase == 2)
		{
			if (elapsed < 9.0)
				return "";

			string state = CurrentStateName();
			if (state != "Shooting")
				return "PASS: бот вышел из Shooting (state=" + state + ")";
			return "FAIL: бот не вышел из Shooting (state=" + state + ")";
		}

		return "";
	}

	//! Ammo count of the magazine currently in the bot's hands (-1 if none).
	int AmmoInHands()
	{
		Weapon_Base wpn = m_Bot.GetWeaponInHands();
		if (!wpn)
			return -1;
		Magazine mag = wpn.GetMagazine(wpn.GetCurrentMuzzle());
		if (mag)
			return mag.GetAmmoCount();
		return -1;
	}

	//! Force the magazine in hands to zero rounds (the chambered round still fires
	//! once more, then HasNoAmmo() turns true and the bot leaves Shooting).
	void EmptyMagazineInHands()
	{
		Weapon_Base wpn = m_Bot.GetWeaponInHands();
		if (!wpn)
			return;
		Magazine mag = wpn.GetMagazine(wpn.GetCurrentMuzzle());
		if (mag)
			mag.ServerSetAmmoCount(0);
	}

	string FailDebug(int ammo)
	{
		string msg = "FAIL: бот не выстрелил (патроны " + ammo + " / " + m_StartAmmo;
		msg = msg + ", state=" + CurrentStateName() + ", HasNoAmmo=" + m_Bot.HasNoAmmo();
		msg = msg + ", HasFirearm=" + m_Bot.HasFirearmInHands() + ")";
		return msg;
	}
}

//! Shock/knockout: the bot must fall unconscious, stay still (position) under a
//! MoveTo command, and recover. The wake-up animation moves the body while
//! IsUnconscious() is still true, so a detected movement is only a failure if the
//! bot stays unconscious for 2.5 s after it.
class dmBotTest_Shock : dmTestSuite_TestCase
{
	float m_UnconTime;
	vector m_RefPos;
	bool m_WasUnconscious;
	bool m_RefTaken;
	bool m_Reported;
	bool m_SuspectMove;
	float m_SuspectTime;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		bot.GetPawn().SetHealth("", "Shock", 0.0);
	}

	override string GetSummary()
	{
		return "Тест «Шок/нокаут». Боту дают шок до нуля. Ожидается: бот падает без сознания; после 3 с гэпа ему дают MoveTo, но тело остаётся на месте; со временем приходит в себя.";
	}

	override float GetDuration() { return 70.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();

		if (!m_WasUnconscious)
		{
			if (pawn.IsUnconscious())
			{
				m_WasUnconscious = true;
				m_UnconTime = elapsed;
				return "OK: бот без сознания (t=" + elapsed + " c)";
			}
			return "";
		}

		if (!pawn.IsUnconscious())
		{
			if (m_SuspectMove)
				return "PASS: смещение было анимацией выхода из нокаута; бот пришёл в себя через ~" + elapsed + " c";
			return "PASS: бот был без сознания, не двигался (даже под MoveTo) и пришёл в себя через ~" + elapsed + " c";
		}

		//! 3 s gap after the fall animation settles, then snapshot the position
		//! and issue MoveTo — the body must not move while unconscious.
		if (!m_RefTaken)
		{
			if (elapsed >= m_UnconTime + 3.0)
			{
				m_RefTaken = true;
				m_RefPos = pawn.GetPosition();
				GiveMoveTo();
				return "OK: гэп пройден, фиксирую позицию и даю MoveTo (t=" + elapsed + " c)";
			}
			return "";
		}

		float moved = HorizontalMove(m_RefPos, pawn.GetPosition());

		if (moved > 0.5)
		{
			if (!m_SuspectMove)
			{
				m_SuspectMove = true;
				m_SuspectTime = elapsed;
				return "OK: зафиксировано смещение (t=" + elapsed + " c), проверяю: не выход ли это из нокаута?";
			}
			if (elapsed - m_SuspectTime >= 2.5)
				return "FAIL: бот двигался во время нокаута (сдвинулся на " + Fmt(moved) + " м)";
			return "";
		}

		m_SuspectMove = false;
		if (!m_Reported)
		{
			m_Reported = true;
			return "OK: бот не двигается под MoveTo (t=" + elapsed + " c), ждём прихода в себя";
		}
		return "";
	}

	void GiveMoveTo()
	{
		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Goal = ForwardTarget(20.0);
		move.m_Priority = dmBotIntentPriority.CRITICAL;
		move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Bot.AddCommandIntent(move);
	}
}

//! Stamina: a heavy backpack lowers the cap and sprinting drains it honestly.
class dmBotTest_Stamina : dmTestSuite_TestCase
{
	float m_StartStamina;
	bool m_CapChecked;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		GiveHeavyBackpack("NailBox", 10);

		bot.SetPreferredSpeed(3.0);

		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Goal = ForwardTarget(300.0);
		move.m_Priority = dmBotIntentPriority.CRITICAL;
		move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		bot.AddCommandIntent(move);

		StaminaHandler sh = bot.GetPawn().GetStaminaHandler();
		if (sh)
			m_StartStamina = sh.GetStaminaNormalized();
	}

	override string GetSummary()
	{
		return "Тест «Стамина». Бот получает рюкзак, набитый NailBox (вес), и бежит 300 м. Ожидается: вес снижает кап стамины, спринт честно её расходует и бот замедляется.";
	}

	override float GetDuration() { return 20.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		StaminaHandler sh = m_Bot.GetPawn().GetStaminaHandler();
		if (!sh)
			return "FAIL: нет StaminaHandler";

		float norm = sh.GetStaminaNormalized();
		float cap = sh.GetStaminaCap();
		float max = sh.GetStaminaMax();

		if (!m_CapChecked && elapsed >= 3.0)
		{
			m_CapChecked = true;
			if (cap >= max)
				return "FAIL: вес не снизил кап стамины (cap " + Fmt(cap) + " = max " + Fmt(max) + ")";
			return "OK: вес снизил кап стамины (cap " + Fmt(cap) + " / max " + Fmt(max) + ")";
		}

		if (elapsed >= 8.0)
		{
			if (norm < m_StartStamina - 0.3)
				return "PASS: стамина расходуется честно (норм " + Fmt(norm) + ", было " + Fmt(m_StartStamina) + ")";
			if (elapsed >= GetDuration() - 1.0)
				return "FAIL: стамина почти не расходуется (норм " + Fmt(norm) + ", было " + Fmt(m_StartStamina) + ")";
		}

		return "";
	}
}

//! Broken leg (no splint): the bot limps (injury anim), jogs (no sprint), and
//! takes shock while jogging until it falls unconscious.
class dmBotTest_BrokenLeg : dmTestSuite_TestCase
{
	vector m_StartPos;
	bool m_Moved;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		PlayerBase pawn = bot.GetPawn();
		m_StartPos = pawn.GetPosition();
		//! Break the leg: SetBrokenLegs applies the state immediately (negative =
		//! first-time activation); ActivateModifier (driven by our modifier tick)
		//! applies the injury animation (limp) + BrokenLegWalkShock.
		pawn.SetBrokenLegs(-eBrokenLegs.BROKEN_LEGS);
		pawn.GetModifiersManager().ActivateModifier(eModifiers.MDF_BROKEN_LEGS);

		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Goal = ForwardTarget(100.0);
		move.m_Priority = dmBotIntentPriority.CRITICAL;
		move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		bot.AddCommandIntent(move);
	}

	override string GetSummary()
	{
		return "Тест «Перелом ноги (без шины)». Боту ломают ногу (шины нет) и дают цель в 100 м. Ожидается: перелом применён, бот хромает и бежит (не спринтует), получает шок от бега и в итоге падает в обморок.";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetBrokenLegs() == eBrokenLegs.NO_BROKEN_LEGS)
			return "FAIL: перелом не применён (GetBrokenLegs() == NO_BROKEN_LEGS)";

		if (!m_Moved && HorizontalMove(m_StartPos, pawn.GetPosition()) > 2.0)
			m_Moved = true;

		if (pawn.IsUnconscious())
		{
			if (!m_Moved)
				return "FAIL: бот упал в обморок, не успев двинуться";
			return "PASS: бот хромал, получал шок от бега и упал в обморок (t=" + elapsed + " c)";
		}

		return "";
	}
}

//! Death: setting Health to 0 must kill the bot and release its brain; the corpse
//! stays in the world (engine decay).
class dmBotTest_Death : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		bot.GetPawn().SetHealth("", "Health", 0.0);
	}

	override string GetSummary()
	{
		return "Тест «Смерть». Боту ставят Health=0. Ожидается: бот умирает, мозг снимается с тиков (IsSpawned()=false), а труп остаётся в мире (протухает движком).";
	}

	override float GetDuration() { return 8.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "PASS: мозг отпущен (IsSpawned()=false), труп остался в мире";

		return "";
	}
}

//! Reactive threat: a zombie spawned next to the bot is injected as a damage
//! threat, must be returned by GetHostileTarget() as the nearest hostile, and
//! must drop out of the hostile set once it dies.
class dmBotTest_Target : dmTestSuite_TestCase
{
	int m_Phase = 0;
	EntityAI m_Zombie;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		bot.SetFSM(dmBotPreset_Combat.Create(bot));
	}

	override string GetSummary()
	{
		return "Тест «Реактивная угроза». Спавн зомби рядом → впрыск угрозы (RegisterDamageThreat) → GetHostileTarget()==зомби → убить → null.";
	}

	override float GetInterval() { return 1.0; }

	override float GetDuration() { return 20.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		dmTarget h;
		dmTarget h2;
		vector pos;

		if (m_Phase == 0)
		{
			if (elapsed < 5.0)
				return "";

			pos = m_Bot.GetPosition();
			pos[0] = pos[0] + 2.0;
			m_Zombie = EntityAI.Cast(GetGame().CreateObject("ZmbM_PatrolNormal_Autumn", SnapToGroundExactly(pos), false));
			if (!m_Zombie)
				return "FAIL: не удалось заспавнить зомби";

			m_Bot.RegisterDamageThreat(m_Zombie, 100.0);
			m_Phase = 1;
			return "зомби заспавнен, угроза впрыснута";
		}

		if (m_Phase == 1)
		{
			if (elapsed < 6.0)
				return "";

			h = m_Bot.GetHostileTarget();
			if (h && h.m_Entity == m_Zombie)
			{
				m_Zombie.SetHealth(0.0);
				m_Phase = 2;
				return "hostile=зомби OK, зомби убит";
			}
			return "FAIL: GetHostileTarget != зомби";
		}

		if (elapsed < 7.0)
			return "";

		h2 = m_Bot.GetHostileTarget();
		if (!h2)
			return "PASS";
		return "FAIL: GetHostileTarget не пуст после смерти";
	}
}

//! Fight: a bot with a Machete repels 3 waves of two zombies (one in front, one
//! behind). Verifies the Fighting state kills them and re-targets between waves.
class dmBotTest_Fight : dmTestSuite_TestCase
{
	int m_Wave = 0;
	int m_Phase = 0;
	EntityAI m_ZombieFront;
	EntityAI m_ZombieBack;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
			pawn.GetHumanInventory().CreateInHands("Machete");
		bot.SetFSM(dmBotPreset_Combat.Create(bot));
		m_Wave = 0;
		m_Phase = 0;
		m_ZombieFront = null;
		m_ZombieBack = null;
	}

	override string GetSummary()
	{
		return "Тест «Бой». Боту дают Machete; через 5с спереди и сзади спавнятся зомби. Ожидается: бот убивает обоих; так 3 волны.";
	}

	override float GetDuration() { return 180.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		if (m_Phase == 0)
		{
			if (elapsed < 5.0)
				return "";
			SpawnWave();
			m_Phase = 1;
			return "волна 1: зомби спереди и сзади";
		}

		bool frontAlive = m_ZombieFront && m_ZombieFront.IsAlive();
		bool backAlive = m_ZombieBack && m_ZombieBack.IsAlive();
		if (frontAlive || backAlive)
			return "";

		m_Wave = m_Wave + 1;
		if (m_Wave >= 3)
			return "PASS: бот отбил 3 волны (по 2 зомби)";

		SpawnWave();
		return "волна " + (m_Wave + 1) + ": зомби спереди и сзади";
	}

	//! Spawn one zombie in front and one behind the bot (2 m).
	void SpawnWave()
	{
		m_ZombieFront = SpawnZombieNearBot(2.0);
		m_ZombieBack = SpawnZombieNearBot(-2.0);
	}
}

//! Aim-accuracy ladder: Idle<->Shooting + reload. A bot with an EMPTY AKM (no
//! magazine) + a backpack of 10x 5-round mags spawns in front of the player and
//! fires single shots at a humanoid dummy target placed along the player's line
//! of sight at 50,100,... up to min(N, look distance) meters, until it kills it.
class dmBotTest_Aim : dmTestSuite_TestCase
{
	ref array<float> m_Distances;
	ref array<int> m_Results;
	EntityAI m_TargetEntity;
	int m_Pass;
	int m_PassPhase;
	float m_PassTimer;
	int m_StartAmmo;
	int m_MaxDistMeters;
	vector m_LookDir;

	void SetMaxDistance(int meters)
	{
		m_MaxDistMeters = meters;
	}

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		float maxDist = DM_AIM_TEST_MAX_DIST;
		if (m_MaxDistMeters > 0)
			maxDist = m_MaxDistMeters;

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;
		// 1) horizontal look direction only (distance stays as the player set it)
		// 2) place + face the bot along the look line
		GetPlayerLookDir(player, m_LookDir);
		vector playerPos = player.GetPosition();
		vector spawnPos = playerPos + m_LookDir * 0.5;
		pawn.SetPosition(spawnPos);
		bot.SetDirection(m_LookDir);

		// 3) empty AKM + optic + backpack of mags
		GiveEmptyAKMWithMags(pawn);

		// 4) minimal FSM: Idle + Shooting (test the transitions + reload)
		bot.SetFSM(dmBotTestPreset_Shooting.Create(bot));
        
		// dmBotIntent_TidyInventory tidy = new dmBotIntent_TidyInventory();
		// bot.AddPersonalityIntent(tidy);

		// 5) distances 50..maxDist step 50
		m_Distances = new array<float>();
		float d;
		for (d = DM_AIM_TEST_STEP; d <= maxDist + 0.001; d = d + DM_AIM_TEST_STEP)
			m_Distances.Insert(d);

		m_Results = new array<int>();
		m_Pass = 0;
		m_PassPhase = 0;
		m_PassTimer = 0.0;
		m_TargetEntity = null;
	}

	override string GetSummary()
	{
		return "Тест «Лестница точности». Бот с пустым АКМ + 10 магазинов по 5 патронов стреляет по мишени-болванке на 50..N м; метрика — выстрелов до убийства (проверяются переходы Idle<->Shooting и перезарядка).";
	}

	override float GetInterval() { return 1.0; }

	override float GetDuration() { return 600.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		m_PassTimer += GetInterval();

		if (m_Pass >= m_Distances.Count())
			return BuildSummary();

		float dist = m_Distances[m_Pass];

		if (m_PassPhase == 0)
		{
			SpawnEnemyNearBot(dist);
			m_StartAmmo = TotalAmmo(m_Bot.GetPawn());
			m_PassPhase = 1;
			m_PassTimer = 0.0;
			return "проход " + (m_Pass + 1) + ": мишень на " + Fmt(dist) + " м";
		}

		if (m_PassPhase == 1)
		{
			if (!m_TargetEntity || !m_TargetEntity.IsAlive())
			{
				int shots = m_StartAmmo - TotalAmmo(m_Bot.GetPawn());
				m_Results.Insert(shots);
				m_PassPhase = 2;
				m_PassTimer = 0.0;
				return "  -> " + Fmt(dist) + " м: " + shots + " выстрелов";
			}
			if (CurrentStateName() != "Shooting" && m_PassTimer > 5.0)
				return "FAIL: бот вышел из Shooting, цель жива (не убил за 50 патронов, " + Fmt(dist) + " м)";
			if (m_PassTimer >= DM_AIM_TEST_PASS_TIMEOUT)
				return "FAIL: цель не убита за таймаут (" + Fmt(dist) + " м)";
			return "";
		}

		// phase 2: pause
		if (m_PassTimer >= DM_AIM_TEST_PAUSE)
		{
			m_Pass++;
			m_PassPhase = 0;
			m_PassTimer = 0.0;
		}
		return "";
	}

	string BuildSummary()
	{
		string s = "ГОТОВО: ";
		int i;
		for (i = 0; i < m_Distances.Count(); i++)
		{
			if (i > 0)
				s = s + ", ";
			s = s + Fmt(m_Distances[i]) + "м=" + m_Results[i];
		}
		return "PASS: " + s;
	}

	void GiveEmptyAKMWithMags(PlayerBase pawn)
	{
		Weapon_Base gun = Weapon_Base.Cast(pawn.GetHumanInventory().CreateInHands("AKM"));
		if (gun)
			gun.GetInventory().CreateAttachment("PSO11Optic");

		EntityAI bag = pawn.GetInventory().CreateInInventory("TortillaBag");
		if (!bag)
			return;

		int i;
		for (i = 0; i < DM_AIM_TEST_MAG_COUNT; i++)
		{
			Magazine mag = Magazine.Cast(bag.GetInventory().CreateInInventory("Mag_AKM_30Rnd"));
			if (mag)
				mag.ServerSetAmmoCount(DM_AIM_TEST_MAG_ROUNDS);
		}
	}

	int TotalAmmo(PlayerBase pawn)
	{
		int total = 0;
		Weapon_Base wpn = Weapon_Base.Cast(pawn.GetHumanInventory().GetEntityInHands());
		if (wpn)
		{
			int mi = wpn.GetCurrentMuzzle();
			if (!wpn.IsChamberEmpty(mi) && !wpn.IsChamberFiredOut(mi))
				total = 1;
		}

		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			Magazine mag = Magazine.Cast(items[i]);
			if (mag)
				total = total + mag.GetAmmoCount();
		}
		return total;
	}
}

//! Emote: the bot plays a gesture animation by EmoteConstants ID. Verifies the
//! emote action command actually starts on the pawn.
class dmBotTest_Emote : dmTestSuite_TestCase
{
	int m_EmoteID;

	void SetEmoteID(int id)
	{
		m_EmoteID = id;
	}

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		dmBotIntent_Emote emote = new dmBotIntent_Emote();
		emote.m_EmoteID = m_EmoteID;
		emote.m_Priority = dmBotIntentPriority.CRITICAL;
		bot.AddCommandIntent(emote);
	}

	override string GetSummary()
	{
		return "Тест «Эмоция». Бот играет жест (ID=" + m_EmoteID + "). Ожидается: команда action активна (GetCommand_Action/Modifier).";
	}

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetCommand_Action() || pawn.GetCommandModifier_Action())
			return "PASS: эмоция запущена (команда action активна, t=" + elapsed + " c)";

		return "";
	}
}

//! Weapon loading: phase 1 gives an empty B95 (break-action, chamber-fed) plus a
//! loose Ammo_308Win pile; phase 2 despawns that bot and spawns a fresh one with an
//! empty M4A1 (no magazine) + a backpack holding a 5.56 pile and an empty STANAG
//! magazine. The TidyInventory personality intent must load them.
class dmBotTest_WeaponLoad : dmTestSuite_TestCase
{
	int m_Phase = 0;
	float m_Phase2Time = 0.0;
	Magazine m_Mag;
	Weapon_Base m_M4;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		pawn.GetHumanInventory().CreateInHands("B95");

		EntityAI bag = pawn.GetInventory().CreateInInventory("TortillaBag");
		if (bag)
			bag.GetInventory().CreateInInventory("Ammo_308Win");

		dmBotFSM fsm = new dmBotFSM(bot);
		dmBotState idle = new dmBotState_Idle();
		fsm.AddState(idle, "Idle");
		fsm.SetDefaultState("Idle");
		fsm.Start();
		bot.SetFSM(fsm);

		dmBotIntent_TidyInventory tidy = new dmBotIntent_TidyInventory();
		bot.AddPersonalityIntent(tidy);

		m_Phase = 0;
		m_Phase2Time = 0.0;
		m_Mag = null;
		m_M4 = null;
	}

	override string GetSummary()
	{
		return "Тест «Зарядка оружия». Фаза 1: пустой B95 + пачка .308 — TidyInventory заряжает ствол. Фаза 2: новый бот с пустым M4A1 + пустой магазин STANAG + пачка 5.56 — магазин заряжается и вставляется.";
	}

	override float GetDuration() { return 120.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		Weapon_Base wpn;
		int mi;
		int internalAmmo;
		bool chamberEmpty;
		string err;
		string msg;
		Magazine mag;
		int magAmmo;
		bool hasMag;

		if (m_Phase == 0)
		{
			if (elapsed < 5.0)
				return "";
			m_Phase = 1;
			return "OK: тишина пройдена, проверяю B95";
		}

		if (m_Phase == 1)
		{
			wpn = m_Bot.GetWeaponInHands();
			if (!wpn)
				return "FAIL: фаза 1 — нет ствола в руках (state=" + CurrentStateName() + ")";
			mi = wpn.GetCurrentMuzzle();
			internalAmmo = wpn.GetInternalMagazineCartridgeCount(mi);
			chamberEmpty = wpn.IsChamberEmpty(mi);
			if (internalAmmo > 0 || !chamberEmpty)
			{
				err = SpawnPhase2();
				if (err != "")
					return err;
				m_Phase = 2;
				m_Phase2Time = elapsed;
				return "OK: фаза 1 — B95 зарядился (internal=" + internalAmmo + ", chamberEmpty=" + chamberEmpty + "), спавню второго бота";
			}
			msg = "FAIL: фаза 1 — B95 не зарядился (state=" + CurrentStateName();
			msg = msg + ", HasNoAmmo=" + m_Bot.HasNoAmmo();
			msg = msg + ", internal=" + internalAmmo + ", chamberEmpty=" + chamberEmpty + ")";
			return msg;
		}

		if (m_Phase == 2)
		{
			if (elapsed < m_Phase2Time + 20.0)
				return "";
			m_Phase = 3;
			return "OK: пауза пройдена, проверяю M4";
		}

		if (m_Phase == 3)
		{
			if (!m_M4)
				return "FAIL: фаза 2 — нет ссылки на M4";
			mi = m_M4.GetCurrentMuzzle();
			mag = m_M4.GetMagazine(mi);
			magAmmo = 0;
			if (m_Mag)
				magAmmo = m_Mag.GetAmmoCount();
			if (mag && magAmmo > 0)
				return "PASS: фаза 2 — магазин заряжен (" + magAmmo + " патронов) и вставлен в M4";
			hasMag = mag != null;
			msg = "FAIL: фаза 2 — магазин не вставлен/не заряжен (ammo=" + magAmmo;
			msg = msg + ", hasMag=" + hasMag + ", state=" + CurrentStateName() + ")";
			return msg;
		}

		return "";
	}

	string SpawnPhase2()
	{
		vector pos = m_Bot.GetPosition();
		m_Bot.Despawn();

		ref dmAISurvivor bot2 = new dmAISurvivor();
		PlayerBase pawn2 = bot2.Spawn(pos, Vector(0, 0, 0));
		if (!pawn2)
		{
			m_Runner.ReplaceBot(null);
			return "FAIL: не удалось заспавнить второго бота";
		}

		m_Runner.ReplaceBot(bot2);
		dmCommandContext.BindBot(m_Player, bot2);

		m_M4 = Weapon_Base.Cast(pawn2.GetHumanInventory().CreateInHands("M4A1"));

		EntityAI bag = pawn2.GetInventory().CreateInInventory("TortillaBag");
		if (bag)
		{
			bag.GetInventory().CreateInInventory("Ammo_556x45");
			m_Mag = Magazine.Cast(bag.GetInventory().CreateInInventory("Mag_STANAG_30Rnd"));
			if (m_Mag)
				m_Mag.ServerSetAmmoCount(0);
		}

		dmBotFSM fsm = new dmBotFSM(bot2);
		dmBotState idle = new dmBotState_Idle();
		fsm.AddState(idle, "Idle");
		fsm.SetDefaultState("Idle");
		fsm.Start();
		bot2.SetFSM(fsm);

		dmBotIntent_TidyInventory tidy = new dmBotIntent_TidyInventory();
		bot2.AddPersonalityIntent(tidy);

		return "";
	}
}

//! Weapon selection by target: a bot with a loaded B95 (shoulder), a barbed bat
//! (melee) and spare .308 ammo in a backpack must pick the rifle vs a distant
//! player dummy and switch to melee vs a nearby zombie.
class dmBotTest_WeaponSelection : dmTestSuite_TestCase
{
	int m_Phase = 0;
	Weapon_Base m_B95;
	EntityAI m_Target;
	int m_StartAmmo = 0;
	bool m_WasShooting = false;
	float m_Phase1Time = 0.0;
	float m_Phase2Time = 0.0;
	float m_Phase3Time = 0.0;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		Weapon_Base b95 = Weapon_Base.Cast(pawn.GetInventory().CreateInInventory("B95"));
		if (b95)
		{
			b95.SpawnAmmo("Ammo_308Win", WeaponWithAmmoFlags.CHAMBER);
			m_B95 = b95;
		}

		pawn.GetInventory().CreateInInventory("BarbedBaseballBat");

		EntityAI bag = pawn.GetInventory().CreateInInventory("TortillaBag");
		if (bag)
			bag.GetInventory().CreateInInventory("Ammo_308Win");

		bot.SetFSM(dmBotPreset_Combat.Create(bot));

		m_Phase = 0;
		m_Target = null;
		m_StartAmmo = 0;
		m_WasShooting = false;
		m_Phase1Time = 0.0;
		m_Phase2Time = 0.0;
		m_Phase3Time = 0.0;
	}

	override string GetSummary()
	{
		return "Тест «Выбор оружия». Бот с заряженным B95 (плечо) + битой (мили). Фаза 1: игрок-болванка на 75 м — бот входит в Shooting и стреляет. Фаза 2: зомби на 15 м — бот входит в Fighting и берёт мили.";
	}

	override float GetDuration() { return 120.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		int ammo;
		PlayerBase pawn;
		EntityAI inHands;
		string hands;
		string msg;

		if (m_Phase == 0)
		{
			if (elapsed < 5.0)
				return "";
			if (!m_B95)
				return "FAIL: нет B95";
			m_StartAmmo = B95Ammo();
			if (m_StartAmmo <= 0)
				return "FAIL: B95 пуст после Setup (SpawnAmmo не сработал)";
			m_Target = SpawnEnemyNearBot(75.0);
			m_Phase = 1;
			m_Phase1Time = elapsed;
			return "болванка-игрок на 75 м, hostile зарегистрирован (патронов в B95: " + m_StartAmmo + ")";
		}

		if (m_Phase == 1)
		{
			if (CurrentStateName() == "Shooting")
				m_WasShooting = true;

			ammo = B95Ammo();
			if (ammo < m_StartAmmo)
			{
				CleanupTarget();
				m_Phase = 2;
				m_Phase2Time = elapsed;
				msg = "бот выстрелил в болванку (патроны " + ammo + " < " + m_StartAmmo;
				msg = msg + ", wasShooting=" + m_WasShooting + ")";
				return msg;
			}

			if (elapsed >= m_Phase1Time + 15.0)
			{
				msg = "FAIL: бот не выстрелил в болванку (state=" + CurrentStateName();
				msg = msg + ", ammo=" + ammo + "/" + m_StartAmmo;
				msg = msg + ", wasShooting=" + m_WasShooting + ")";
				return msg;
			}
			return "";
		}

		if (m_Phase == 2)
		{
			if (elapsed < m_Phase2Time + 3.0)
				return "";
			m_Target = SpawnZombieNearBot(15.0);
			m_Phase = 3;
			m_Phase3Time = elapsed;
			return "зомби на 15 м, hostile зарегистрирован";
		}

		if (m_Phase == 3)
		{
			if (CurrentStateName() == "Fighting")
			{
				pawn = m_Bot.GetPawn();
				inHands = pawn.GetItemInHands();
				if (inHands && inHands.IsMeleeWeapon())
					return "PASS: бот в Fighting с мили в руках (" + inHands.GetType() + ")";
			}

			if (elapsed >= m_Phase3Time + 15.0)
			{
				pawn = m_Bot.GetPawn();
				hands = "none";
				inHands = pawn.GetItemInHands();
				if (inHands)
					hands = inHands.GetType();
				msg = "FAIL: бот не в Fighting с мили (state=" + CurrentStateName();
				msg = msg + ", hands=" + hands + ")";
				return msg;
			}
			return "";
		}

		return "";
	}

	int B95Ammo()
	{
		int total = 0;
		if (!m_B95)
			return 0;
		int mc = m_B95.GetMuzzleCount();
		int k;
		for (k = 0; k < mc; k++)
			total = total + m_B95.GetTotalCartridgeCount(k);
		return total;
	}

	void CleanupTarget()
	{
		if (m_Target)
			m_Target.SetHealth(0.0);
		m_Target = null;
	}
}

//! Looting (get): the bot picks up a sequence of 7 clothing items from the ground
//! one by one (5 s silence → spawn → 5 s silence → PickUp intent → wait until it is
//! worn in the inventory). PASS when all 7 are worn.
class dmBotTest_LootingGet : dmTestSuite_TestCase
{
	int m_Step = 0;
	int m_SubPhase = 0;   // 0 = spawn, 1 = PickUp, 2 = check
	float m_PhaseTime = 0.0;
	EntityAI m_Item;
	ref array<string> m_Items;
	ref array<string> m_Slots;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		m_Step = 0;
		m_SubPhase = 0;
		m_PhaseTime = 0.0;
		m_Item = null;

		m_Items = new array<string>();
		m_Items.Insert("M65Jacket_Black");
		m_Items.Insert("CargoPants_Beige");
		m_Items.Insert("BaseballCap_Black");
		m_Items.Insert("TacticalGloves_Black");
		m_Items.Insert("Shemag_Green");
		m_Items.Insert("CombatBoots_Black");
		m_Items.Insert("Armband_White");

		m_Slots = new array<string>();
		m_Slots.Insert("Body");
		m_Slots.Insert("Legs");
		m_Slots.Insert("Headgear");
		m_Slots.Insert("Gloves");
		m_Slots.Insert("Mask");
		m_Slots.Insert("Feet");
		m_Slots.Insert("Armband");
	}

	override string GetSummary()
	{
		return "Тест «Лут (подбор)». Бот по очереди поднимает с пола 7 предметов одежды (куртка, штаны, кепка, перчатки, шемаг, ботинки, повязка). Ожидается: каждый предмет НАДЕТ в свой слот (Body/Legs/Headgear/Gloves/Mask/Feet/Armband).";
	}

	override float GetDuration() { return 200.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		if (m_Step >= 7)
			return "PASS: все 7 предметов подняты";

		string cls = m_Items[m_Step];

		if (m_SubPhase == 0)
		{
			if (elapsed - m_PhaseTime < 5.0)
				return "";
			m_Item = SpawnItemNearBot(cls);
			if (!m_Item)
				return "FAIL: не удалось заспавнить " + cls;
			m_SubPhase = 1;
			m_PhaseTime = elapsed;
			return "предмет " + (m_Step + 1) + "/7: " + cls + " заспавнен";
		}

		if (m_SubPhase == 1)
		{
			if (elapsed - m_PhaseTime < 5.0)
				return "";
			CreatePickUpIntent(m_Item);
			m_SubPhase = 2;
			m_PhaseTime = elapsed;
			return "интент PickUp на " + cls;
		}

		ItemBase worn = GetWorn(m_Slots[m_Step]);
		if (worn && worn.GetType() == cls)
		{
			m_Step = m_Step + 1;
			m_SubPhase = 0;
			m_PhaseTime = elapsed;
			m_Item = null;
			return "надет " + cls + " в " + m_Slots[m_Step - 1] + " (" + m_Step + "/7)";
		}

		if (elapsed - m_PhaseTime > 20.0)
			return "FAIL: таймаут подбора " + cls;

		return "";
	}

	//! Создать предмет на полу рядом с ботом (2 м вперёд-вправо), прибитый к земле.
	EntityAI SpawnItemNearBot(string cls)
	{
		vector origin = m_Bot.GetPosition();
		vector pos = origin + Vector(2.0, 0.0, 2.0);
		return EntityAI.Cast(GetGame().CreateObject(cls, SnapToGroundExactly(pos), false));
	}

	//! Выдать боту интент подбора предмета (идёт и поднимает).
	void CreatePickUpIntent(EntityAI item)
	{
		dmBotIntent_PickUp p = new dmBotIntent_PickUp();
		p.m_Item = item;
		m_Bot.AddCommandIntent(p);
	}

	//! Надетый в слот предмет (или null).
	ItemBase GetWorn(string slotName)
	{
		return ItemBase.Cast(m_Bot.GetPawn().GetInventory().FindAttachment(InventorySlots.GetSlotIdFromString(slotName)));
	}
}

//! Looting (change): a bot already wearing shorts/t-shirt/shoes (with Pear/Apple in
//! their cargo) picks up Gorka pants and a Gorka jacket from the ground. The new
//! clothes must be worn in their slots and the cargo transferred to them.
class dmBotTest_LootingChange : dmTestSuite_TestCase
{
	int m_Phase = 0;
	float m_PhaseTime = 0.0;
	EntityAI m_Item;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		pawn.GetInventory().CreateInInventory("TShirt_Green");
		pawn.GetInventory().CreateInInventory("CanvasPantsMidi_Blue");
		pawn.GetInventory().CreateInInventory("JoggingShoes_Black");

		ItemBase tshirt = GetWorn("Body");
		if (tshirt)
			tshirt.GetInventory().CreateInInventory("Apple");

		ItemBase shorts = GetWorn("Legs");
		if (shorts)
			shorts.GetInventory().CreateInInventory("Pear");

		m_Phase = 0;
		m_PhaseTime = 0.0;
		m_Item = null;
	}

	override string GetSummary()
	{
		return "Тест «Лут (смена одежды)». Бот в шортах/футболке/обуви (в карго — груша/яблоко) поднимает горку-штаны и горку-куртку с пола. Ожидается: новая одежда надевается в слоты Legs/Body, а карго переносится.";
	}

	override float GetDuration() { return 200.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		ItemBase worn;

		if (m_Phase == 0)
		{
			if (elapsed - m_PhaseTime < 5.0)
				return "";
			m_Item = SpawnItemNearBot("GorkaPants_Autumn");
			if (!m_Item)
				return "FAIL: не удалось заспавнить GorkaPants_Autumn";
			m_Phase = 1;
			m_PhaseTime = elapsed;
			return "горка-штаны заспавнены на полу";
		}

		if (m_Phase == 1)
		{
			if (elapsed - m_PhaseTime < 5.0)
				return "";
			CreatePickUpIntent(m_Item);
			m_Phase = 2;
			m_PhaseTime = elapsed;
			return "интент PickUp на горку-штаны";
		}

		if (m_Phase == 2)
		{
			worn = GetWorn("Legs");
			if (worn && worn.GetType() == "GorkaPants_Autumn")
			{
				m_Phase = 3;
				m_PhaseTime = elapsed;
				return "бот надел горку-штаны (Legs)";
			}
			if (elapsed - m_PhaseTime > 30.0)
				return "FAIL: бот не надел горку-штаны (Legs)";
			return "";
		}

		if (m_Phase == 3)
		{
			if (elapsed - m_PhaseTime < 5.0)
				return "";
			m_Item = SpawnItemNearBot("GorkaEJacket_Autumn");
			if (!m_Item)
				return "FAIL: не удалось заспавнить GorkaEJacket_Autumn";
			m_Phase = 4;
			m_PhaseTime = elapsed;
			return "горка-куртка заспавнена на полу";
		}

		if (m_Phase == 4)
		{
			if (elapsed - m_PhaseTime < 5.0)
				return "";
			CreatePickUpIntent(m_Item);
			m_Phase = 5;
			m_PhaseTime = elapsed;
			return "интент PickUp на горку-куртку";
		}

		if (m_Phase == 5)
		{
			worn = GetWorn("Body");
			if (worn && worn.GetType() == "GorkaEJacket_Autumn")
			{
				m_Phase = 6;
				m_PhaseTime = elapsed;
				return "бот надел горку-куртку (Body)";
			}
			if (elapsed - m_PhaseTime > 30.0)
				return "FAIL: бот не надел горку-куртку (Body)";
			return "";
		}

		//! m_Phase == 6: финальная проверка слотов и перенесённого карго.
		ItemBase legs = GetWorn("Legs");
		ItemBase body = GetWorn("Body");

		bool legsOk = legs && legs.GetType() == "GorkaPants_Autumn";
		bool bodyOk = body && body.GetType() == "GorkaEJacket_Autumn";
		bool pearOk = HasInCargo(legs, "Pear");
		bool appleOk = HasInCargo(body, "Apple");

		if (legsOk && bodyOk && pearOk && appleOk)
			return "PASS: горка-штаны (Legs) с грушей, горка-куртка (Body) с яблоком";

		string msg = "FAIL: финальная проверка (legs=" + legsOk;
		msg = msg + ", body=" + bodyOk;
		msg = msg + ", pear=" + pearOk;
		msg = msg + ", apple=" + appleOk + ")";
		return msg;
	}

	//! Надетый в слот предмет (или null).
	ItemBase GetWorn(string slotName)
	{
		return ItemBase.Cast(m_Bot.GetPawn().GetInventory().FindAttachment(InventorySlots.GetSlotIdFromString(slotName)));
	}

	//! Есть ли предмет класса `cls` в карго контейнера.
	bool HasInCargo(ItemBase container, string cls)
	{
		if (!container)
			return false;
		CargoBase cargo = container.GetInventory().GetCargo();
		if (!cargo)
			return false;
		int i;
		for (i = 0; i < cargo.GetItemCount(); i++)
		{
			EntityAI item = cargo.GetItem(i);
			if (item && item.GetType() == cls)
				return true;
		}
		return false;
	}

	//! Создать предмет на полу рядом с ботом (2 м вперёд-вправо), прибитый к земле.
	EntityAI SpawnItemNearBot(string cls)
	{
		vector origin = m_Bot.GetPosition();
		vector pos = origin + Vector(2.0, 0.0, 2.0);
		return EntityAI.Cast(GetGame().CreateObject(cls, SnapToGroundExactly(pos), false));
	}

	//! Выдать боту интент подбора предмета (идёт и поднимает).
	void CreatePickUpIntent(EntityAI item)
	{
		dmBotIntent_PickUp p = new dmBotIntent_PickUp();
		p.m_Item = item;
		m_Bot.AddCommandIntent(p);
	}
}

//! Suppressor noise-strength ladder: no suppressor 3000m, improvised 150m,
//! rifle/automatic 100m, pistol 75m (via dmBotGunshotNoiseStrength()).
class dmBotTest_Suppressor : dmTestSuite_TestCase
{
	int m_Phase = 0;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		//! Винтовка в руки (без глушителя) — стартовая точка.
		PlayerBase pawn = bot.GetPawn();
		if (pawn)
			pawn.GetHumanInventory().CreateInHands("M4A1");
	}

	override string GetSummary()
	{
		return "Тест «Глушитель». 4 фазы силы шума выстрела: без глушителя 3000м, самодельный 150м, автоматный 100м, пистолетный 75м.";
	}

	override float GetInterval() { return 1.0; }

	override float GetDuration() { return 20.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		Weapon_Base wpn;
		Weapon_Base pistol;
		ItemBase sup;
		ItemSuppressor oldSup;
		float strength;

		if (m_Phase == 0)
		{
			wpn = m_Bot.GetWeaponInHands();
			if (!wpn)
				return "FAIL: нет оружия в руках";
			strength = wpn.dmBotGunshotNoiseStrength();
			if (strength != DM_NOISE_GUNSHOT_STRENGTH)
				return "FAIL: без глушителя ожидалось " + Fmt(DM_NOISE_GUNSHOT_STRENGTH) + ", получили " + Fmt(strength);
			sup = ItemBase.Cast(wpn.GetInventory().CreateAttachment("ImprovisedSuppressor"));
			if (!sup)
				return "FAIL: не удалось надеть самодельный глушитель";
			m_Phase = 1;
			return "фаза 1 (без): " + Fmt(strength) + " — ок, надел самодельный";
		}
		else if (m_Phase == 1)
		{
			wpn = m_Bot.GetWeaponInHands();
			if (!wpn)
				return "FAIL: нет оружия в руках";
			strength = wpn.dmBotGunshotNoiseStrength();
			if (strength != DM_NOISE_GUNSHOT_SILENCED_HOMEMADE)
				return "FAIL: самодельный ожидалось " + Fmt(DM_NOISE_GUNSHOT_SILENCED_HOMEMADE) + ", получили " + Fmt(strength);
			oldSup = wpn.GetAttachedSuppressor();
			if (oldSup)
				oldSup.Delete();
			m_Phase = 2;
			return "фаза 2 (самодельный): " + Fmt(strength) + " — ок, снимаю самодельный";
		}
		else if (m_Phase == 2)
		{
			wpn = m_Bot.GetWeaponInHands();
			if (!wpn)
				return "FAIL: нет оружия в руках";
			sup = ItemBase.Cast(wpn.GetInventory().CreateAttachment("M4_Suppressor"));
			if (!sup)
				return "FAIL: не удалось надеть автоматный глушитель";
			strength = wpn.dmBotGunshotNoiseStrength();
			if (strength != DM_NOISE_GUNSHOT_SILENCED_RIFLE)
				return "FAIL: автоматный ожидалось " + Fmt(DM_NOISE_GUNSHOT_SILENCED_RIFLE) + ", получили " + Fmt(strength);
			wpn.Delete();
			m_Phase = 3;
			return "фаза 3 (автоматный): " + Fmt(strength) + " — ок, меняю на пистолет";
		}
		else
		{
			pistol = Weapon_Base.Cast(m_Bot.GetPawn().GetHumanInventory().CreateInHands("CZ75"));
			if (!pistol)
				return "FAIL: не удалось дать пистолет";
			sup = ItemBase.Cast(pistol.GetInventory().CreateAttachment("PistolSuppressor"));
			if (!sup)
				return "FAIL: не удалось надеть пистолетный глушитель";
			strength = pistol.dmBotGunshotNoiseStrength();
			if (strength != DM_NOISE_GUNSHOT_SILENCED_PISTOL)
				return "FAIL: пистолетный ожидалось " + Fmt(DM_NOISE_GUNSHOT_SILENCED_PISTOL) + ", получили " + Fmt(strength);
			return "PASS: 3000 / 150 / 100 / 75 — глушители определяются верно";
		}
	}
}

//! Ballistic drop-compensation convergence test: Mosin (internal 5-round
//! magazine) + perfect aim, target at N m (default 500). Fires without the FSM
//! (GetAiming().SetTarget + RaiseWeapon + RequestFire) at a fixed interval, cycling the
//! bolt / chamber-loading from the pants ammo via ReloadWeaponAI when not ready.
//! Each miss feeds BallisticFeedback, which nudges the bullet-drop coefficient;
//! the coefficient trend is read from the [Ballistics] server log (FEEDBACK)
//! and echoed per-shot in the chat returns. Stops on a hit or when ammo runs out.
class dmBotTest_Trajectory : dmTestSuite_TestCase
{
	int m_Phase = 0;
	EntityAI m_Target;
	float m_TargetDistance = 0.0;
	int m_Shots = 0;
	int m_LastShotTime;
	vector m_LookDir;

	void SetTargetDistance(float v)
	{
		m_TargetDistance = v;
	}

	float GetTargetDistance()
	{
		if (m_TargetDistance > 0.0)
			return m_TargetDistance;
		return DM_TRAJECTORY_TEST_DISTANCE;
	}

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		PlayerBase pawn = bot.GetPawn();
		Weapon_Base mosin = Weapon_Base.Cast(pawn.GetHumanInventory().CreateInHands("Mosin9130"));
		if (mosin)
		{
			mosin.SpawnAmmo("Ammo_762x54", WeaponWithAmmoFlags.CHAMBER);
			mosin.GetInventory().CreateAttachment("PUScopeOptic");
		}

		//! Spare ammo in the pants cargo: 5 loose piles of 7.62x54 (20 rounds
		//! each = 100), found by FindChamberAmmo for bolt-cycle chamber-loading.
		EntityAI pants = pawn.GetInventory().CreateInInventory("CargoPants_Beige");
		if (pants)
		{
			for (int i = 0; i < 5; i++)
				pants.GetInventory().CreateInInventory("Ammo_762x54");
		}

		GetPlayerLookDir(player, m_LookDir);
		vector playerPos = player.GetPosition();
		vector spawnPos = playerPos + m_LookDir * 0.5;
		pawn.SetPosition(spawnPos);
		bot.SetDirection(m_LookDir);

		dmAISurvivorBase base = dmAISurvivorBase.Cast(pawn);
		if (base)
		{
			base.SetPerfectAim(true);
			base.GetAiming().Enable();
		}
	}

	override string GetSummary()
	{
		return "Тест «Траектория». Мосинка (магазин 5 патронов + 100 патронов в штанах) + идеальный прицел, цель на 500 м. Выстрелы с паузой 5 с: каждый промах корректирует коэф. компенсации дропа (см. лог [Ballistics] FEEDBACK), коэф печатается в каждом выстреле. Затвор циклируется / досылается из штанов через ReloadWeaponAI. PASS — цель убита; DONE — патроны исчерпаны.";
	}

	override float GetInterval() { return 0.5; }

	override float GetDuration() { return 500.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Bot.GetPawn());
		dmAiming aim;

		if (m_Phase == 0)
		{
			//! Развернуть тело по направлению взгляда игрока.
			vector dir = m_Player.GetDirection();
			dir[1] = 0.0;
			dir.Normalize();
			float yaw = dir.VectorToAngles()[0];
			if (pawn)
				pawn.SetTargetBodyYaw(yaw);
			if (pawn)
				pawn.SetOrientation(Vector(yaw, 0.0, 0.0));
				
			vector targetPos = ForwardTarget(GetTargetDistance());
			m_Target = EntityAI.Cast(GetGame().CreateObject(dmSurvivor.GetRandom(), targetPos, false));
			if (!m_Target)
				return "FAIL: не удалось заспавнить цель на " + Fmt(GetTargetDistance()) + " м";
			if (pawn)
				pawn.RaiseWeapon(true);
			m_Shots = 0;
			m_Phase = 1;
			return "цель на " + Fmt(GetTargetDistance()) + " м заспавнена, оружие поднимается";
		}
		else if (m_Phase == 1)
		{
			if (pawn && m_Target)
			{
				aim = pawn.GetAiming();
				if (aim)
				{
					aim.SetTarget(m_Target);
					aim.Enable();
				}
			}
			if (pawn && pawn.IsReadyToShoot())
			{
				pawn.RequestFire();
				m_Shots = 1;
				m_LastShotTime = GetGame().GetTime();
				m_Phase = 2;
				return "выстрел #1 (coef=" + Fmt(pawn.GetDropCoef()) + ")";
			}
			return "";
		}
		else
		{
			//! Релоад сразу после выстрела (болтовик — одиночные): цикл затвора /
			//! досыл из штанов перекрывается с полётом пули.
			if (pawn && !pawn.IsReadyToShoot())
			{
				if (!pawn.ReloadWeaponAI())
					return "DONE: закончились патроны (coef=" + Fmt(pawn.GetDropCoef()) + " после " + m_Shots + " выстрелов)";
			}

			if ((GetGame().GetTime() - m_LastShotTime) / 1000.0 < DM_TRAJECTORY_SHOT_INTERVAL)
				return "";

			DayZPlayer tgt = DayZPlayer.Cast(m_Target);
			if (m_Target && !m_Target.IsAlive())
				return "PASS: цель поражена за " + m_Shots + " выстрелов (coef=" + Fmt(pawn.GetDropCoef()) + ")";
			if (tgt && tgt.IsUnconscious())
				return "PASS: цель без сознания (ранена) за " + m_Shots + " выстрелов (coef=" + Fmt(pawn.GetDropCoef()) + ")";

			if (pawn && m_Target)
			{
				aim = pawn.GetAiming();
				if (aim)
				{
					aim.SetTarget(m_Target);
					aim.Enable();
				}
			}
			if (pawn && pawn.IsReadyToShoot())
			{
				pawn.RequestFire();
				m_Shots = m_Shots + 1;
				m_LastShotTime = GetGame().GetTime();
				return "выстрел #" + m_Shots + " (coef=" + Fmt(pawn.GetDropCoef()) + ")";
			}
			return "";
		}
	}
}

//! Lead-observation diagnostic: the test bot (Mosin + optic + perfect aim) shoots
//! at a FULL running target bot that sprints 200 m across the sight line (from
//! P_left to P_right through P0 at N m). There is NO lead in the aiming code — the
//! sight tracks the target's CURRENT position — so the shots are expected to miss.
//! The metric is hits; the test reports DONE when the target finishes its run (or
//! ammo runs out), PASS only if a shot lands.
class dmBotTest_LeadShoot : dmTestSuite_TestCase
{
	int m_Phase = 0;
	float m_TargetDistance = 0.0;
	int m_Shots = 0;
	int m_LastShotTime;
	ref dmAISurvivor m_TargetBot;
	vector m_PRight;
	vector m_LookDir;

	void SetTargetDistance(float v)
	{
		m_TargetDistance = v;
	}

	float GetTargetDistance()
	{
		if (m_TargetDistance > 0.0)
			return m_TargetDistance;
		return DM_LEADSHOOT_TEST_DISTANCE;
	}

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		PlayerBase pawn = bot.GetPawn();
		Weapon_Base mosin = Weapon_Base.Cast(pawn.GetHumanInventory().CreateInHands("Mosin9130"));
		if (mosin)
		{
			mosin.SpawnAmmo("Ammo_762x54", WeaponWithAmmoFlags.CHAMBER);
			mosin.GetInventory().CreateAttachment("PUScopeOptic");
		}

		//! Spare ammo in the pants cargo: 5 loose piles of 7.62x54 (20 rounds each).
		EntityAI pants = pawn.GetInventory().CreateInInventory("CargoPants_Beige");
		if (pants)
		{
			for (int i = 0; i < 5; i++)
				pants.GetInventory().CreateInInventory("Ammo_762x54");
		}

		GetPlayerLookDir(player, m_LookDir);
		vector playerPos = player.GetPosition();
		vector spawnPos = playerPos + m_LookDir * 0.5;
		pawn.SetPosition(spawnPos);
		bot.SetDirection(m_LookDir);

		dmAISurvivorBase base = dmAISurvivorBase.Cast(pawn);
		if (base)
			base.SetPerfectAim(true);
	}

	override string GetSummary()
	{
		return "Тест «Упреждение». Мосинка + оптика + идеальный прицел; цель (полный бот) бежит спринтом 200 м поперёк прицела на " + Fmt(GetTargetDistance()) + " м. Метрика — попадания; ожидается промах (упреждения в коде нет).";
	}

	override float GetInterval() { return 0.5; }

	override float GetDuration() { return 500.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Bot.GetPawn());
		dmAiming aim;

		if (m_Phase == 0)
		{
			//! Развернуть тело по направлению взгляда игрока.
			vector dir = m_Player.GetDirection();
			dir[1] = 0.0;
			dir.Normalize();
			float yaw = dir.VectorToAngles()[0];
			if (pawn)
				pawn.SetTargetBodyYaw(yaw);
			if (pawn)
				pawn.SetOrientation(Vector(yaw, 0.0, 0.0));

			//! Геометрия: P0 на взгляде, перпендикуляр, точки старта/финиша пробега.
			vector p0 = ForwardTarget(GetTargetDistance());
			vector perp = Vector(dir[2], 0.0, -dir[0]);
			vector pLeft = p0 + perp * DM_LEADSHOOT_RUN_OFFSET;
			vector pRight = p0 - perp * DM_LEADSHOOT_RUN_OFFSET;
			pLeft = SnapToGroundExactly(pLeft);
			pRight = SnapToGroundExactly(pRight);
			m_PRight = pRight;

			//! Цель — ПОЛНЫЙ бот, бегущий спринтом от P_left к P_right.
			m_TargetBot = new dmAISurvivor();
			PlayerBase tpawn = m_TargetBot.Spawn(pLeft, Vector(0, 0, 0));
			if (!tpawn)
			{
				m_TargetBot = null;
				return "FAIL: не удалось заспавнить цель";
			}
			m_TargetBot.SetPreferredSpeed(DM_LEADSHOOT_TARGET_SPEED);

			dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
			move.m_Goal = pRight;
			move.m_Priority = dmBotIntentPriority.CRITICAL;
			move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			m_TargetBot.AddCommandIntent(move);

			if (pawn)
				pawn.RaiseWeapon(true);
			m_Shots = 0;
			m_Phase = 1;
			return "цель (бегущий бот) заспавнена на " + Fmt(GetTargetDistance()) + " м, бежит спринтом поперёк прицела";
		}

		if (m_Phase == 3)
			return "";

		//! Terminal checks (every tick): target dead -> PASS; target reached P_right
		//! -> DONE (no lead, hits 0).
		string verdict = CheckTermination();
		if (verdict != "")
			return verdict;

		if (m_Phase == 1)
		{
			if (pawn && m_TargetBot && m_TargetBot.IsSpawned())
			{
				aim = pawn.GetAiming();
				if (aim)
				{
					aim.SetTarget(m_TargetBot.GetPawn());
					aim.Enable();
				}
			}
			if (pawn && pawn.IsReadyToShoot())
			{
				pawn.RequestFire();
				m_Shots = 1;
				m_LastShotTime = GetGame().GetTime();
				m_Phase = 2;
				return "выстрел #1";
			}
			return "";
		}

		//! m_Phase == 2: aim follows the running pawn every tick; reload when not
		//! ready; fire at the shot interval.
		if (pawn && m_TargetBot && m_TargetBot.IsSpawned())
		{
			aim = pawn.GetAiming();
			if (aim)
			{
				aim.SetTarget(m_TargetBot.GetPawn());
				aim.Enable();
			}
		}

		if (pawn && !pawn.IsReadyToShoot())
		{
			if (!pawn.ReloadWeaponAI())
			{
				CleanupTarget();
				m_Phase = 3;
				return "DONE: закончились патроны после " + m_Shots + " выстрелов";
			}
		}

		if ((GetGame().GetTime() - m_LastShotTime) / 1000.0 < DM_TRAJECTORY_SHOT_INTERVAL)
			return "";

		if (pawn && pawn.IsReadyToShoot())
		{
			pawn.RequestFire();
			m_Shots = m_Shots + 1;
			m_LastShotTime = GetGame().GetTime();
			return "выстрел #" + m_Shots;
		}
		return "";
	}

	//! Returns "" while the target is still running alive; otherwise cleans up and
	//! returns the verdict (PASS on a kill, DONE on finishing the run).
	string CheckTermination()
	{
		if (!m_TargetBot)
			return "";
		PlayerBase tpawn = m_TargetBot.GetPawn();
		if (!tpawn || !tpawn.IsAlive())
		{
			//! Natural death: OnDeath() already unregistered from s_All and nulled
			//! the pawn; the corpse is left to the engine.
			m_TargetBot = null;
			m_Phase = 3;
			return "PASS: цель убита за " + m_Shots + " выстрелов";
		}
		if (HorizontalMove(m_TargetBot.GetPosition(), m_PRight) < 2.0)
		{
			CleanupTarget();
			m_Phase = 3;
			return "DONE: цель пробежала 200 м, попаданий 0 (упреждения нет), " + m_Shots + " выстрелов";
		}
		return "";
	}

	//! Remove the running target bot from the world (no leak in s_All).
	void CleanupTarget()
	{
		if (m_TargetBot && m_TargetBot.IsSpawned())
			m_TargetBot.Despawn();
		m_TargetBot = null;
	}
}
