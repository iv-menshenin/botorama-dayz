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

		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
		{
			dmWorldPoiLocation loc = dmWorldPOIRegistry.Get().GetLocationAt(pawn.GetPosition());
			if (loc)
				bot.GetExplorer().ArriveAtLocation(loc);
		}
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
			return EXIT;

		MaybeDrop(bot, pawn, pDt);

		if (!EnsurePickUp(bot, pawn))
		{
			EnsureExplore(bot);
		}

		return CONTINUE;
	}

	//! При переполнении — выбросить самую ненужную вещь (с троттлингом).
	void MaybeDrop(dmAISurvivor bot, dmAISurvivorBase pawn, float pDt)
	{
		m_DropCooldown -= pDt;
		if (m_DropCooldown > 0.0)
			return;
		m_DropCooldown = DM_EXPLORE_DROP_COOLDOWN;

		dmRequirements req = bot.GetRequirements();
		#ifdef DM_BOT_DEBUG_LOOTING
		if ( req && !req.IsFull() )
		{
			dmBotLog.Debug("[Loot] Полно места в инвентаре");
		}
		#endif
		if (!req || !req.IsFull())
			return;

		ref array<EntityAI> order = req.GetDiscardOrder();
		if (order.Count() == 0)
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] Выбрасывать нечего");
			#endif
			return;
		}

		EntityAI item = order[0];
		if (pawn.DropItem(ItemBase.Cast(item)))
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] Выбрасываю: " + item.GetType());
			#endif
			bot.GetWishlist().Ignore(item);
		}
	}

	//! Оппортунистический подбор: самый желаемый предмет рядом (порог).
	bool EnsurePickUp(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		if (m_PickUp)
		{
			if (m_PickUp.IsFinished() || m_PickUp.IsExpired() || m_PickUp.IsFailed()) m_PickUp = null;
		}
		if (m_PickUp) return true;

		EntityAI item = pickUpItemSelect(bot, pawn);
		if (item)
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] Собираюсь залутать " + item.GetType() + " тут " + item.GetPosition());
			#endif
			m_PickUp = new dmBotIntent_PickUp();
			m_PickUp.m_Item = item;
			bot.AddFSMIntent(m_PickUp);
			dmLoot.RemoveNearbyItem(pawn, item, DM_EXPLORE_PICKUP_RADIUS);
			return true;
		}
		return false;
	}

	EntityAI pickUpItemSelect(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		array<EntityAI> items = dmLoot.ScanNearbyItems(pawn, DM_EXPLORE_PICKUP_RADIUS);
		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] Рядом " + items.Count() + " вещей, выбираю, что подобрать");
		#endif
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
			float desire = bot.CalcDesired(item);
			if (desire > DM_EXPLORE_PICKUP_THRESHOLD && desire > bestDesire)
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] Мне нравится: " + item.GetType() + " на " + desire);
				#endif
				best = item;
				bestDesire = desire;
			}
		}

		if (!best)
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] Все хлам");
			#endif
			return null;
		}

		return best;
	}

	//! Блуждание к ближайшему непосещённому зданию; по достижении — пометить.
	void EnsureExplore(dmAISurvivor bot)
	{
		if (m_Move && (m_Move.IsFinished() || m_Move.IsExpired()))
		{
			if (m_CurrentBuilding)
				bot.GetExplorer().MarkLocationBuildingVisited(m_CurrentBuilding);
			m_Move = null;
			m_CurrentBuilding = null;
		}
		if (m_Move)
			return;

		Building building = bot.GetExplorer().GetNextUnvisitedBuilding(bot);
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
