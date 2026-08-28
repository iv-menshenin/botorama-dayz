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
		vector spawnPos = player.GetPosition() + fwd * DM_SPAWN_DISTANCE;

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
		move.m_Target = ForwardTarget(m_Player, 20.0);
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
		move.m_Target = ForwardTarget(player, 300.0);
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
		move.m_Target = ForwardTarget(player, 100.0);
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

//! Death: setting Health to 0 must kill the bot and trigger the brain cleanup.
class dmBotTest_Death : dmBotTestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		bot.GetPawn().SetHealth("", "Health", 0.0);
	}

	override string GetSummary()
	{
		return "Тест «Смерть и очистка». Боту ставят Health=0. Ожидается: бот умирает, и мозг удаляет его (вместе с пешкой) из мира.";
	}

	override float GetDuration() { return 8.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "PASS: бот удалён из мира (мозг очищен)";

		return "";
	}
}
