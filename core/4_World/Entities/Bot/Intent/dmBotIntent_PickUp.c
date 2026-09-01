//! dmBotIntent_PickUp — walk to a world item and take it into the inventory.
//!
//! It inherits the full path-following machinery from dmBotIntent_MoveTo
//! (steering, stuck detection, vault/climb, ladders, recovery) and only overrides
//! the goal (the item's position), the start (aim the route at the item) and the
//! action at the goal (take the item into the inventory). The item is static, but
//! the goal is refreshed each tick in case it shifts.
class dmBotIntent_PickUp : dmBotIntent_MoveTo
{
	EntityAI m_Item;

	void dmBotIntent_PickUp()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
		m_ReachDistance = DM_PICKUP_REACH;
	}

	override string GetIntentName()
	{
		return "PickUp";
	}

	override void OnStart(dmAISurvivor bot)
	{
		if (m_Item)
			m_Goal = m_Item.GetPosition();
		super.OnStart(bot);
	}

	//! Цель = позиция предмета (предмет статичен, но обновляем на случай сдвига).
	override void UpdateGoal(dmAISurvivor bot, float pDt)
	{
		if (m_Item)
			m_Goal = m_Item.GetPosition();
	}

	//! Достигли предмета — поднять в инвентарь.
	override void OnReachedGoal(dmAISurvivor bot)
	{
		if (!m_Item || m_Item.IsDamageDestroyed() || m_Item.IsSetForDeletion())
		{
			Fail();
			return;
		}

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn && pawn.GetInventory().TakeEntityToInventory(InventoryMode.SERVER, FindInventoryLocationType.CARGO, m_Item))
			Finish();
		else
			Fail();
	}
};
