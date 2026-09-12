//! dmBotMedicalTest — автотесты ИИ-лечения (MedicalCare). Каждый сценарий выдаёт
//! боту полную аптечку (GiveMedicalPants), запускает чистый пресет Idle + MedicalCare,
//! накладывает эффект и ждёт, пока бот сам себя вылечит.

//! Перевязка: бот с кровотечением (рана Pelvis) должен сам перевязаться.
class dmBotTest_Bandaging : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		GiveMedicalPants();
		bot.SetFSM(dmBotTestPreset_Medical.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
			pawn.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("Pelvis");
	}

	override string GetSummary()
	{
		return "Тест «Перевязка». Боту дают кровотечение (рана Pelvis) и аптечку. Ожидается: бот входит в MedicalCare и перевязывается — IsBleeding()==false.";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (!pawn.IsBleeding())
			return "PASS: бот перевязал рану (t=" + elapsed + " c)";
		return "";
	}
}

//! Шина: бот с переломом должен сам наложить шину (GetBrokenLegs()==BROKEN_LEGS_SPLINT).
class dmBotTest_Splinting : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		GiveMedicalPants();
		bot.SetFSM(dmBotTestPreset_Medical.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			pawn.SetBrokenLegs(-eBrokenLegs.BROKEN_LEGS);
			pawn.GetModifiersManager().ActivateModifier(eModifiers.MDF_BROKEN_LEGS);
		}
	}

	override string GetSummary()
	{
		return "Тест «Шина». Боту ломают ногу и дают аптечку (с шиной). Ожидается: бот накладывает шину — GetBrokenLegs()==BROKEN_LEGS_SPLINT.";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetBrokenLegs() == eBrokenLegs.BROKEN_LEGS_SPLINT)
			return "PASS: бот наложил шину (t=" + elapsed + " c)";
		return "";
	}
}

//! Обезболивающее (бонус a): после наложения шины бот принимает обезбол.
class dmBotTest_Painkiller : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		GiveMedicalPants();
		bot.SetFSM(dmBotTestPreset_Medical.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			pawn.SetBrokenLegs(-eBrokenLegs.BROKEN_LEGS);
			pawn.GetModifiersManager().ActivateModifier(eModifiers.MDF_BROKEN_LEGS);
		}
	}

	override string GetSummary()
	{
		return "Тест «Обезболивающее (после шины)». Боту ломают ногу и дают аптечку. Ожидается: после шины бот принимает обезбол — MDF_PAINKILLERS активен.";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetModifiersManager().IsModifierActive(eModifiers.MDF_PAINKILLERS))
			return "PASS: бот принял обезболивающее после шины (t=" + elapsed + " c)";
		return "";
	}
}

//! Обезболивающее (бонус b): кровотечение + низкое HP — после перевязки обезбол.
class dmBotTest_PainkillerBandage : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		GiveMedicalPants();
		bot.SetFSM(dmBotTestPreset_Medical.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			pawn.SetHealth("", "Health", 30.0);
			pawn.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("Pelvis");
		}
	}

	override string GetSummary()
	{
		return "Тест «Обезболивающее (после перевязки)». Боту дают кровотечение + низкое HP (30) и аптечку. Ожидается: после перевязки (HP<75%) бот принимает обезбол — MDF_PAINKILLERS активен.";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetModifiersManager().IsModifierActive(eModifiers.MDF_PAINKILLERS))
			return "PASS: бот принял обезболивающее после перевязки (t=" + elapsed + " c)";
		return "";
	}
}

//! Уголь: сальмонелла — бот принимает активированный уголь (MDF_CHARCOAL).
class dmBotTest_Charcoal : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		GiveMedicalPants();
		bot.SetFSM(dmBotTestPreset_Medical.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			pawn.InsertAgent(eAgents.SALMONELLA, 60.0);
			pawn.GetModifiersManager().ActivateModifier(eModifiers.MDF_SALMONELLA);
		}
	}

	override string GetSummary()
	{
		return "Тест «Уголь». Боту дают сальмонеллу (MDF_SALMONELLA) и аптечку. Ожидается: бот принимает уголь — MDF_CHARCOAL активен.";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetModifiersManager().IsModifierActive(eModifiers.MDF_CHARCOAL))
			return "PASS: бот принял уголь (t=" + elapsed + " c)";
		return "";
	}
}

//! Тетрациклин: грипп — бот принимает антибиотик (MDF_ANTIBIOTICS).
class dmBotTest_Tetracycline : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		GiveMedicalPants();
		bot.SetFSM(dmBotTestPreset_Medical.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			pawn.InsertAgent(eAgents.INFLUENZA, 600.0);
			pawn.GetModifiersManager().ActivateModifier(eModifiers.MDF_INFLUENZA);
		}
	}

	override string GetSummary()
	{
		return "Тест «Тетрациклин». Боту дают грипп (MDF_INFLUENZA) и аптечку. Ожидается: бот принимает антибиотик — MDF_ANTIBIOTICS активен.";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetModifiersManager().IsModifierActive(eModifiers.MDF_ANTIBIOTICS))
			return "PASS: бот принял антибиотик (t=" + elapsed + " c)";
		return "";
	}
}

//! Витамины: холод (HeatComfort<=-0.15) — бот принимает витамины (MDF_IMMUNITYBOOST).
class dmBotTest_Vitamins : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);
		GiveMedicalPants();
		bot.SetFSM(dmBotTestPreset_Medical.Create(bot));

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			PlayerStat<float> hc = pawn.GetStatHeatComfort();
			if (hc)
				hc.Set(-0.5);
		}
	}

	override string GetSummary()
	{
		return "Тест «Витамины». Боту ставят холод (HeatComfort=-0.5) и дают аптечку. Ожидается: бот принимает витамины — MDF_IMMUNITYBOOST активен.";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetModifiersManager().IsModifierActive(eModifiers.MDF_IMMUNITYBOOST))
			return "PASS: бот принял витамины (t=" + elapsed + " c)";
		return "";
	}
}
