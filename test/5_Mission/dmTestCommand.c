//! dmTestCommand — "/test ..." scripted test scenarios.

class dmTestCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_CMD_TEST;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		//! /test bot patrol
		if (parts.Count() < 3)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи сценарий: /test bot patrol");
			return false;
		}

		if (parts[1] == DM_CHAT_CMD && parts[2] == DM_CHAT_FSM_PATROL)
			return HandlePatrol(player);

		dmCommandManager.ChatToPlayer(player, "Неизвестный сценарий: " + parts[2]);
		return false;
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
