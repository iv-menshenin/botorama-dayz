//! dmBotTest — self-verifying scenarios for the honest body simulation.
//!
//! A scenario is a dmBotTestCase subclass. dmBotTestRunner spawns a bot, runs
//! Setup(), prints a one-line summary to chat, then polls the bot's state on a
//! repeating timer. OnCheck() returns:
//!   - ""        -> keep waiting (no message);
//!   - "PASS..." / "FAIL..." -> report and stop;
//!   - anything else -> report progress and keep waiting (e.g. "OK: ...").

class dmBotTestCase
{
	ref dmAISurvivor m_Bot;
	PlayerBase m_Player;
	float m_SpawnDistance = 0.0;

	//! Spawn distance from the player (meters); 0 = DM_SPAWN_DISTANCE default.
	float GetSpawnDistance()
	{
		if (m_SpawnDistance > 0.0)
			return m_SpawnDistance;
		return DM_SPAWN_DISTANCE;
	}

	//! Apply the scenario-specific condition to the freshly spawned bot.
	void Setup(dmAISurvivor bot, PlayerBase player) {}

	//! One-line summary sent to chat on start (what is done + expected result).
	string GetSummary() { return ""; }

	//! Polled every GetInterval() seconds. See the file header for the contract.
	string OnCheck(float elapsed) { return ""; }

	//! Poll interval (seconds).
	float GetInterval() { return 1.0; }

	//! Total runtime before an automatic timeout-FAIL.
	float GetDuration() { return 15.0; }

	//! World point `distance` meters ahead of the player's horizontal look direction.
	static vector ForwardTarget(PlayerBase player, float distance)
	{
		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		return player.GetPosition() + fwd * distance;
	}

	//! Equip a backpack and fill it with heavy NailBoxes (stamina-weight test).
	static void GiveHeavyBackpack(PlayerBase pawn)
	{
		EntityAI bag = pawn.GetInventory().CreateInInventory("TortillaBag");
		if (!bag)
			return;

		GameInventory bagInv = bag.GetInventory();
		int i;
		for (i = 0; i < 10; i++)
			bagInv.CreateInInventory("NailBox");
	}

	//! Horizontal distance moved from a start position (ignores the vertical fall).
	static float HorizontalMove(vector start, vector current)
	{
		vector d = current - start;
		d[1] = 0.0;
		return d.Length();
	}

	//! Trim a float to 3 decimals for readable chat output.
	static string Fmt(float v)
	{
		return (Math.Round(v * 1000.0) / 1000.0).ToString();
	}
}

class dmBotTestRunner
{
	static ref dmBotTestRunner s_Instance;
	ref dmBotTestCase m_Test;
	PlayerBase m_Player;
	ref dmAISurvivor m_Bot;
	float m_Elapsed;
	bool m_Running;

	static dmBotTestRunner GetInstance()
	{
		if (!s_Instance)
			s_Instance = new dmBotTestRunner();
		return s_Instance;
	}

	bool IsRunning()
	{
		return m_Running;
	}

	//! Spawn the bot immediately, show the summary, then wait DM_TEST_QUIET_SECONDS
	//! before running Setup() + polling (via Begin).
	void Start(dmBotTestCase test, PlayerBase player)
	{
		if (m_Running)
		{
			dmCommandManager.ChatToPlayer(player, "Тест уже идёт — дождись его завершения.");
			return;
		}

		vector fwd = player.GetDirection();
		fwd[1] = 0.0;
		fwd.Normalize();
		float spawnDist = test.GetSpawnDistance();
		vector spawnPos = player.GetPosition() + fwd * spawnDist;

		ref dmAISurvivor bot = new dmAISurvivor();
		PlayerBase pawn = bot.Spawn(spawnPos, Vector(0, 0, 0));
		if (!pawn)
		{
			dmCommandManager.ChatToPlayer(player, "Не удалось заспавнить бота для теста.");
			return;
		}
		dmCommandContext.BindBot(player, bot);

		//! Kick: real movement wakes the bot's physics (a stationary AI character
		//! isn't re-evaluated for ground collision/fall, so it would float).
		dmCommandContext.GiveMoveKick(bot, player);

		m_Test = test;
		m_Player = player;
		m_Bot = bot;
		m_Elapsed = 0.0;
		m_Running = true;

		dmCommandManager.ChatToPlayer(player, test.GetSummary());
		dmCommandManager.ChatToPlayer(player, "Старт через " + DM_TEST_QUIET_SECONDS + " c...");

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.Begin, (int)(DM_TEST_QUIET_SECONDS * 1000), false);
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

		m_Test.m_Bot = m_Bot;
		m_Test.m_Player = m_Player;
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
			dmCommandManager.ChatToPlayer(m_Player, result);

			if (result.Length() >= 4 && (result.Substring(0, 4) == "PASS" || result.Substring(0, 4) == "FAIL"))
				Stop();
		}
		else if (m_Elapsed >= m_Test.GetDuration())
		{
			dmCommandManager.ChatToPlayer(m_Player, "FAIL: таймаут — ожидаемый результат не наступил за " + m_Test.GetDuration() + " c");
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
			dmCommandManager.ChatToPlayer(player, "Нет запущенного теста.");
			return;
		}

		Stop();

		if (m_Bot && m_Bot.IsSpawned())
			m_Bot.Despawn();
		m_Bot = null;

		dmCommandManager.ChatToPlayer(player, "Тест отменён, бот удалён.");
	}
}

//! Shock/knockout: the bot must fall unconscious, stay still (position) under a
//! MoveTo command, and recover. The wake-up animation moves the body while
//! IsUnconscious() is still true, so a detected movement is only a failure if the
//! bot stays unconscious for 2.5 s after it.
class dmBotTest_Shock : dmBotTestCase
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
		bot.GetPawn().SetHealth("", "Shock", 0.0); // below UNCONSCIOUS_THRESHOLD -> knockout
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
		move.m_Goal = ForwardTarget(m_Player, 20.0);
		move.m_Priority = dmBotIntentPriority.CRITICAL;
		move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		m_Bot.AddCommandIntent(move);
	}
}

//! Stamina: a heavy backpack lowers the cap and sprinting drains it honestly.
class dmBotTest_Stamina : dmBotTestCase
{
	float m_StartStamina;
	bool m_CapChecked;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		GiveHeavyBackpack(bot.GetPawn());

		bot.SetPreferredSpeed(3.0);

		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Goal = ForwardTarget(player, 300.0);
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
class dmBotTest_BrokenLeg : dmBotTestCase
{
	vector m_StartPos;
	bool m_Moved;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		PlayerBase pawn = bot.GetPawn();
		m_StartPos = pawn.GetPosition();
		//! Break the leg: SetBrokenLegs applies the state immediately (negative =
		//! first-time activation); ActivateModifier (driven by our modifier tick)
		//! applies the injury animation (limp) + BrokenLegWalkShock.
		pawn.SetBrokenLegs(-eBrokenLegs.BROKEN_LEGS);
		pawn.GetModifiersManager().ActivateModifier(eModifiers.MDF_BROKEN_LEGS);

		dmBotIntent_MoveTo move = new dmBotIntent_MoveTo();
		move.m_Goal = ForwardTarget(player, 100.0);
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
class dmBotTest_Death : dmBotTestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
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
class dmBotTest_Target : dmBotTestCase
{
	int m_Phase = 0;
	EntityAI m_Zombie;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
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
			m_Zombie = EntityAI.Cast(GetGame().CreateObject("ZmbM_PatrolNormal_Autumn", pos, false));
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
class dmBotTest_Fight : dmBotTestCase
{
	int m_Wave = 0;              // число отбитых волн (0..3)
	int m_Phase = 0;             // 0 = тишина, 1 = бой текущей волны
	EntityAI m_ZombieFront;
	EntityAI m_ZombieBack;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
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

	//! Spawn one zombie in front and one behind the bot (2 m, snapped to ground).
	void SpawnWave()
	{
		PlayerBase pawn = m_Bot.GetPawn();
		if (!pawn)
			return;
		vector botPos = m_Bot.GetPosition();
		vector dir = pawn.GetDirection();
		dir[1] = 0.0;
		dir.Normalize();

		vector frontPos = SnapToGround(botPos + dir * 2.0);
		vector backPos = SnapToGround(botPos - dir * 2.0);

		m_ZombieFront = EntityAI.Cast(GetGame().CreateObject("ZmbM_PatrolNormal_Autumn", frontPos, false));
		m_ZombieBack = EntityAI.Cast(GetGame().CreateObject("ZmbM_PatrolNormal_Autumn", backPos, false));

		if (m_ZombieFront)
			m_Bot.RegisterHostile(m_ZombieFront, 1.0);
		if (m_ZombieBack)
			m_Bot.RegisterHostile(m_ZombieBack, 1.0);
	}
}

//! Firing: a bot with a loaded AKM receives a damage threat, must enter the
//! Shooting state and fire — the ammo count of the magazine in hands drops.
class dmBotTest_Shoot : dmBotTestCase
{
	int m_Phase = 0;
	EntityAI m_Target;
	int m_StartAmmo = 0;

	//! Spawn distance from the player (meters); 0 = DM_SPAWN_DISTANCE default.
	void SetSpawnDistance(int meters)
	{
		m_SpawnDistance = meters;
	}

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		bot.SetFSM(dmBotPreset_Combat.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

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

			vector pos = m_Bot.GetPosition();
			vector dir = m_Bot.GetPawn().GetDirection();
			dir[1] = 0.0;
			dir.Normalize();
			pos = pos + dir * 15.0;

			m_Target = EntityAI.Cast(GetGame().CreateObject("dmAI_SurvivorM_Denis", SnapToGround(pos), false));
			if (!m_Target)
				return "FAIL: не удалось заспавнить цель";

			m_Bot.RegisterHostile(m_Target, 1.0);
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

	//! Name of the current FSM state ("none" when unavailable).
	string CurrentStateName()
	{
		string state = "none";
		dmBotFSM fsm = m_Bot.GetFSM();
		if (fsm && fsm.GetCurrentState())
			state = fsm.GetCurrentState().GetName();
		return state;
	}

	string FailDebug(int ammo)
	{
		string msg = "FAIL: бот не выстрелил (патроны " + ammo + " / " + m_StartAmmo;
		msg = msg + ", state=" + CurrentStateName() + ", HasNoAmmo=" + m_Bot.HasNoAmmo();
		msg = msg + ", HasFirearm=" + m_Bot.HasFirearmInHands() + ")";
		return msg;
	}
}

//! Aim-accuracy ladder: Idle<->Shooting + reload. A bot with an EMPTY AKM (no
//! magazine) + a backpack of 10x 5-round mags spawns in front of the player and
//! fires single shots at a humanoid dummy target placed along the player's line
//! of sight at 50,100,... up to min(N, look distance) meters, until it kills it.
class dmBotTest_Aim : dmBotTestCase
{
	ref array<float> m_Distances;
	ref array<int> m_Results;
	EntityAI m_TargetEntity;
	int m_Pass;
	int m_PassPhase;      // 0=spawn, 1=shooting, 2=pause
	float m_PassTimer;
	int m_StartAmmo;
	int m_MaxDistMeters;
	vector m_LookDir;

	//! Max target distance (meters); 0 = DM_AIM_TEST_MAX_DIST default.
	void SetMaxDistance(int meters)
	{
		m_MaxDistMeters = meters;
	}

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		// 1) horizontal look direction only (distance stays as the player set it)
		GetPlayerLookDir(player, m_LookDir);
		float maxDist = DM_AIM_TEST_MAX_DIST;
		if (m_MaxDistMeters > 0)
			maxDist = m_MaxDistMeters;

		// 2) place + face the bot along the look line
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;
		vector spawnPos = player.GetPosition() + m_LookDir * 0.5;
		pawn.SetPosition(spawnPos);
		bot.SetDirection(m_LookDir);

		// 3) empty AKM + optic + backpack of mags
		GiveEmptyAKMWithMags(pawn);

		// 4) minimal FSM: Idle + Shooting (test the transitions + reload)
		dmBotFSM fsm = new dmBotFSM(bot);
		dmBotState idle = new dmBotState_Idle();
		dmBotState shoot = new dmBotState_Shooting();
		fsm.AddState(idle, "Idle");
		fsm.AddState(shoot, "Shooting");
		idle.AddTransition(shoot, 1.0).Require(dmBotConditions.HasHostile());
		shoot.AddTransition(idle, 1.0);
		fsm.SetDefaultState("Idle");
		fsm.Start();
		bot.SetFSM(fsm);

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
			SpawnTarget(dist);
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

	//! One-line result table + PASS.
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

	//! Spawn the humanoid dummy target at `distance` meters along the look line,
	//! on the ground (SurfaceY), then force it hostile (threat 1.0).
	void SpawnTarget(float distance)
	{
		vector botPos = m_Bot.GetPosition();
		vector pos = botPos + m_LookDir * distance;
		pos[1] = 0.1;

		m_TargetEntity = EntityAI.Cast(GetGame().CreateObject("dmAI_SurvivorM_Denis", SnapToGround(pos), false));
		if (m_TargetEntity)
			m_Bot.RegisterHostile(m_TargetEntity, 1.0);
	}

	//! Player's horizontal look direction (head-bone forward with pitch zeroed,
	//! as if looking at the horizon). Normalized.
	void GetPlayerLookDir(PlayerBase player, out vector lookDir)
	{
		int headBone = player.GetBoneIndexByName("Head");
		if (headBone != -1)
		{
			vector headTransform[4];
			player.GetBoneTransformWS(headBone, headTransform);
			lookDir = headTransform[1];
		}
		else
		{
			lookDir = MiscGameplayFunctions.GetHeadingVector(player);
		}
		lookDir[1] = 0.0;
		lookDir.Normalize();
	}

	//! Equip an EMPTY AKM (no magazine) + PSO11Optic + a backpack with
	//! DM_AIM_TEST_MAG_COUNT magazines of DM_AIM_TEST_MAG_ROUNDS rounds each.
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

	//! Total live rounds across ALL magazines (hands + backpack) plus the chambered
	//! round (1 if a live round is chambered). Shots fired = start - current.
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

	//! Name of the current FSM state ("none" when unavailable).
	string CurrentStateName()
	{
		string state = "none";
		dmBotFSM fsm = m_Bot.GetFSM();
		if (fsm && fsm.GetCurrentState())
			state = fsm.GetCurrentState().GetName();
		return state;
	}
}

//! Emote: the bot plays a gesture animation by EmoteConstants ID. Verifies the
//! emote action command actually starts on the pawn.
class dmBotTest_Emote : dmBotTestCase
{
	int m_EmoteID;

	void SetEmoteID(int id)
	{
		m_EmoteID = id;
	}

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
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
class dmBotTest_WeaponLoad : dmBotTestCase
{
	int m_Phase = 0;
	float m_Phase2Time = 0.0;
	Magazine m_Mag;
	Weapon_Base m_M4;

	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
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

	//! Despawn the phase-1 bot and spawn a fresh one on the same spot with the
	//! phase-2 loadout (empty M4A1 + backpack with ammo pile + empty magazine).
	string SpawnPhase2()
	{
		vector pos = m_Bot.GetPosition();
		m_Bot.Despawn();

		ref dmAISurvivor bot2 = new dmAISurvivor();
		PlayerBase pawn2 = bot2.Spawn(pos, Vector(0, 0, 0));
		if (!pawn2)
		{
			m_Bot = null;
			dmBotTestRunner.GetInstance().m_Bot = null;
			return "FAIL: не удалось заспавнить второго бота";
		}

		m_Bot = bot2;
		dmBotTestRunner.GetInstance().m_Bot = bot2;
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

	//! Name of the current FSM state ("none" when unavailable).
	string CurrentStateName()
	{
		string state = "none";
		dmBotFSM fsm = m_Bot.GetFSM();
		if (fsm && fsm.GetCurrentState())
			state = fsm.GetCurrentState().GetName();
		return state;
	}
}

//! Weapon selection by target: a bot with a loaded B95 (shoulder), a barbed bat
//! (melee) and spare .308 ammo in a backpack must pick the rifle vs a distant
//! player dummy and switch to melee vs a nearby zombie.
class dmBotTest_WeaponSelection : dmBotTestCase
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
			SpawnDummy75();
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
			SpawnZombie15();
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

	//! Total live cartridges across all B95 muzzles (chambers).
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

	//! Spawn the humanoid dummy 75 m along the bot's look line and force it hostile.
	void SpawnDummy75()
	{
		vector botPos = m_Bot.GetPosition();
		vector dir = m_Bot.GetPawn().GetDirection();
		dir[1] = 0.0;
		dir.Normalize();
		vector pos = botPos + dir * 75.0;
		m_Target = EntityAI.Cast(GetGame().CreateObject("dmAI_SurvivorM_Denis", SnapToGround(pos), false));
		if (m_Target)
			m_Bot.RegisterHostile(m_Target, 1.0);
	}

	//! Spawn a zombie 15 m along the bot's look line and force it hostile.
	void SpawnZombie15()
	{
		vector botPos = m_Bot.GetPosition();
		vector dir = m_Bot.GetPawn().GetDirection();
		dir[1] = 0.0;
		dir.Normalize();
		vector pos = botPos + dir * 15.0;
		m_Target = EntityAI.Cast(GetGame().CreateObject("ZmbM_PatrolNormal_Autumn", SnapToGround(pos), false));
		if (m_Target)
			m_Bot.RegisterHostile(m_Target, 1.0);
	}

	//! Kill the current target so the bot's hostile set clears.
	void CleanupTarget()
	{
		if (m_Target)
			m_Target.SetHealth(0.0);
		m_Target = null;
	}

	//! Name of the current FSM state ("none" when unavailable).
	string CurrentStateName()
	{
		string state = "none";
		dmBotFSM fsm = m_Bot.GetFSM();
		if (fsm && fsm.GetCurrentState())
			state = fsm.GetCurrentState().GetName();
		return state;
	}
}
