//! dmFSMCommand — "/fsm ..." commands: draft a state machine (new/add/apply).

class dmFSMCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_FSM;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
			return false;

		if (parts[1] == DM_CHAT_FSM_NEW)
			return HandleNew(player);
		if (parts[1] == DM_CHAT_ADD)
			return HandleAdd(player, parts);
		if (parts[1] == DM_CHAT_FSM_APPLY)
			return HandleApply(player);

		return false;
	}

	private bool HandleNew(PlayerBase player)
	{
		dmCommandContext.s_DraftStates.Clear();
		dmCommandManager.ChatToPlayer(player, "FSM: новый (пустой)");
		return true;
	}

	private bool HandleAdd(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 3)
			return false;

		string state = parts[2];
		if (state != DM_CHAT_FSM_IDLE && state != DM_CHAT_FSM_PATROL && state != DM_CHAT_FSM_STEALTH)
		{
			dmCommandManager.ChatToPlayer(player, "Неизвестное состояние: " + state);
			return false;
		}

		if (state == DM_CHAT_FSM_STEALTH)
		{
			dmCommandManager.GetPlayerLookPoint(player, dmCommandContext.s_DraftStealthCover);
		}

		dmCommandContext.s_DraftStates.Insert(state);
		dmCommandManager.ChatToPlayer(player, "FSM: добавлено состояние " + state);
		return true;
	}

	private bool HandleApply(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
		{
			dmCommandManager.ChatToPlayer(player, "Нет бота — сначала /bot spawn test");
			return false;
		}

		dmCommandContext.ApplyDraftFSM(bot);
		dmCommandManager.ChatToPlayer(player, "FSM применён (состояний: " + dmCommandContext.s_DraftStates.Count() + ")");
		return true;
	}
}
