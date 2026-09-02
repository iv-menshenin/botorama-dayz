class dmBotState_Exploration : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;     // маршрут к зданию (DESIRABLE)
	ref dmBotIntent_PickUp m_PickUp;   // подбор предмета (EXCLUSIVE)
	Building m_CurrentBuilding;
	float m_DropCooldown;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.INTERRUPTIBLE;
	}

	override void OnEntry(dmBotState from)
	{
		m_Move = null;
		m_PickUp = null;
		m_CurrentBuilding = null;
		m_DropCooldown = 0.0;
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return EXIT;

		MaybeDrop(bot, pawn, pDt);
		EnsurePickUp(bot, pawn);
		EnsureExplore(bot);

		return CONTINUE;
	}

	//! При переполнении — выбросить самую ненужную вещь (с троттлингом).
	void MaybeDrop(dmAISurvivor bot, dmAISurvivorBase pawn, float pDt)
	{
		m_DropCooldown -= pDt;
		if (m_DropCooldown > 0.0)
			return;

		dmRequirements req = bot.GetRequirements();
		if (!req || !req.IsFull())
			return;

		ref array<EntityAI> order = req.GetDiscardOrder();
		if (order.Count() == 0)
			return;

		EntityAI item = order[0];
		if (pawn.DropItem(item))
		{
			bot.GetWishlist().Ignore(item);
			m_DropCooldown = DM_EXPLORE_DROP_COOLDOWN;
		}
	}

	//! Оппортунистический подбор: самый желаемый предмет рядом (порог).
	void EnsurePickUp(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		if (m_PickUp)
		{
			if (m_PickUp.IsFinished() || m_PickUp.IsExpired() || m_PickUp.IsFailed()) m_PickUp = null;
		}
		if (m_PickUp) return;

		array<EntityAI> items = dmLoot.ScanNearbyItems(pawn, DM_EXPLORE_PICKUP_RADIUS);
		dmWishlist wish = bot.GetWishlist();
		ItemBase best = null;
		float bestDesire = 0.0;
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			ItemBase item = ItemBase.Cast(items[i]);
			if (!item) continue;
			if (!dmLoot.CanCarry(pawn, item))
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] Не могу это нести: " + item.GetType() + " pos=" + item.GetPosition());
				#endif
				continue;
			}
			float desire = wish.CalcDesired(item);
			if (desire > DM_EXPLORE_PICKUP_THRESHOLD && desire > bestDesire)
			{
				best = item;
				bestDesire = desire;
			}
		}

		if (!best)
			return;

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[LOOT] Собираюсь залутать " + best.GetType() + " тут " + best.GetPosition());
		#endif
		m_PickUp = new dmBotIntent_PickUp();
		m_PickUp.m_Item = best;
		bot.AddFSMIntent(m_PickUp);
		dmLoot.RemoveNearbyItem(pawn, best, DM_EXPLORE_PICKUP_RADIUS);
	}

	//! Блуждание к ближайшему непосещённому зданию; по достижении — пометить.
	void EnsureExplore(dmAISurvivor bot)
	{
		if (m_Move && (m_Move.IsFinished() || m_Move.IsExpired()))
		{
			if (m_CurrentBuilding)
				bot.GetExplorer().MarkVisited(m_CurrentBuilding);
			m_Move = null;
			m_CurrentBuilding = null;
		}
		if (m_Move)
			return;

		Building building = bot.GetExplorer().GetNearest(bot, DM_EXPLORE_EXPLORE_RADIUS);
		if (!building)
			return;

		m_CurrentBuilding = building;
		m_Move = new dmBotIntent_MoveTo();
		m_Move.m_Goal = building.GetPosition();
		m_Move.m_ReachDistance = DM_EXPLORE_BUILDING_REACH;
		m_Move.m_Priority = dmBotIntentPriority.DESIRABLE;
		bot.AddFSMIntent(m_Move);
	}

	override void OnExit(dmBotState to)
	{
		if (m_Move) { m_Move.Finish(); m_Move = null; }
		if (m_PickUp) { m_PickUp.Finish(); m_PickUp = null; }
	}
};
