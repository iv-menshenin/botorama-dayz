//! dmBotEatDrinkTest — автотесты персонального рефлекса еды/питья (dmBotIntent_EatDrink).
//! Интент персональный (PARALLEL+IDLE), FSM не нужен: сценарий даёт предмет, обнуляет
//! стат и ждёт, пока бот сам положит еду/жидкость в живот.

//! Еда: бот с обнулённой энергией и яблоком должен сам поесть.
class dmBotTest_Eat : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			pawn.GetInventory().CreateInInventory("Apple");
			pawn.GetStatEnergy().Set(0.0);
		}

		dmBotIntent_EatDrink intent = new dmBotIntent_EatDrink();
		bot.AddPersonalityIntent(intent);
	}

	override string GetSummary()
	{
		return "Тест «Еда». Боту дают яблоко и обнуляют энергию. Ожидается: бот ест — живот заполняется (GetStomachVolume()>0).";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetStomach().GetStomachVolume() > 0.0)
			return "PASS: бот поел — живот заполнен (t=" + elapsed + " c)";
		return "";
	}
}

//! Питьё: бот с обнулённой водой и полной бутылкой должен сам попить.
class dmBotTest_Drink : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			ItemBase drink = ItemBase.Cast(pawn.GetInventory().CreateInInventory("WaterBottle"));
			if (drink)
			{
				drink.SetLiquidType(LIQUID_WATER);
				drink.SetQuantityMax();
			}
			pawn.GetStatWater().Set(0.0);
		}

		dmBotIntent_EatDrink intent = new dmBotIntent_EatDrink();
		bot.AddPersonalityIntent(intent);
	}

	override string GetSummary()
	{
		return "Тест «Питьё». Боту дают полную бутылку воды и обнуляют воду. Ожидается: бот пьёт — живот заполняется (GetStomachVolume()>0).";
	}

	override float GetDuration() { return 30.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetStomach().GetStomachVolume() > 0.0)
			return "PASS: бот попил — живот заполнен (t=" + elapsed + " c)";
		return "";
	}
}

//! Консерва: бот с обнулённой энергией и закрытой PeachesCan должен открыть банку
//! (замена на PeachesCan_Opened) и съесть — живот заполняется.
class dmBotTest_EatCan : dmTestSuite_TestCase
{
	override void Setup(dmAISurvivor bot, PlayerBase player)
	{
		super.Setup(bot, player);

		PlayerBase pawn = bot.GetPawn();
		if (pawn)
		{
			pawn.GetInventory().CreateInInventory("PeachesCan");
			pawn.GetStatEnergy().Set(0.0);
		}

		dmBotIntent_EatDrink intent = new dmBotIntent_EatDrink();
		bot.AddPersonalityIntent(intent);
	}

	override string GetSummary()
	{
		return "Тест «Консерва». Боту дают закрытую PeachesCan и обнуляют энергию. Ожидается: бот открывает банку (PeachesCan_Opened) и съедает — живот заполняется (GetStomachVolume()>0).";
	}

	override float GetDuration() { return 45.0; }

	override string OnCheck(float elapsed)
	{
		if (!m_Bot || !m_Bot.IsSpawned())
			return "FAIL: бот исчез из мира";

		PlayerBase pawn = m_Bot.GetPawn();
		if (pawn.GetStomach().GetStomachVolume() > 0.0)
			return "PASS: бот открыл и съел консерву — живот заполнен (t=" + elapsed + " c)";
		return "";
	}
}
