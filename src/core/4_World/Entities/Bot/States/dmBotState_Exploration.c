class dmBotState_Exploration : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;     // маршрут к зданию (DESIRABLE)
	ref dmBotIntent_PickUp m_PickUp;   // подбор предмета (EXCLUSIVE)
	Building m_CurrentBuilding;
	ref array<vector> m_InteriorPoints; // интерьер-точки текущего здания (порядок обхода)
	int m_PointIndex;                   // индекс следующей интерьер-точки
	float m_DropCooldown;
	float m_BuildingTime;               // сколько времени бот возится с текущим зданием

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.INTERRUPTIBLE;
	}

	override bool CanEnter()
	{
		return !GetOwner().GetExplorer().IsNothingToDo();
	}

	override void OnEntry(dmBotState from)
	{
		m_Move = null;
		m_PickUp = null;
		m_CurrentBuilding = null;
		m_InteriorPoints = null;
		m_PointIndex = 0;
		m_DropCooldown = 0.0;
		m_BuildingTime = 0.0;

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

		bot.GetExplorer().RefreshBuildingsIfEmpty(pDt);

		if (m_CurrentBuilding)
			m_BuildingTime = m_BuildingTime + pDt;

		bot.GetExplorer().TickLocationTime(pDt);

		if (CheckExits(bot))
			return EXIT;

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

	//! Обход здания: интерьер-точки по порядку, затем позиция здания как якорь;
	//! по завершении последней точки — пометить здание посещённым.
	void EnsureExplore(dmAISurvivor bot)
	{
		dmExplorer explorer = bot.GetExplorer();

		if (m_CurrentBuilding && m_BuildingTime > DM_EXPLORE_BUILDING_TIMEOUT)
		{
			if (m_Move) { m_Move.Finish(); m_Move = null; }
			dmBotLog.Error("[Loot] Explore: здание слишком долго, пропускаю " + m_CurrentBuilding.GetType());
			explorer.MarkLocationBuildingVisited(m_CurrentBuilding);
			m_CurrentBuilding = null;
			m_InteriorPoints = null;
			m_PointIndex = 0;
			m_BuildingTime = 0.0;
		}

		if (m_Move && (m_Move.IsFinished() || m_Move.IsExpired() || m_Move.IsFailed()))
		{
			bool reached = m_Move.IsFinished();
			m_Move = null;
			if (m_CurrentBuilding && m_InteriorPoints && m_PointIndex >= m_InteriorPoints.Count())
			{
				if (reached)
				{
					#ifdef DM_BOT_DEBUG_LOOTING
					dmBotLog.Debug("[Loot] Explore: здание обойдено " + m_CurrentBuilding.GetType());
					#endif
				}
				else
				{
					dmBotLog.Error("[Loot] Explore: здание недостижимо, пропускаю " + m_CurrentBuilding.GetType());
				}
				explorer.MarkLocationBuildingVisited(m_CurrentBuilding);
				m_CurrentBuilding = null;
				m_InteriorPoints = null;
				m_PointIndex = 0;
			}
		}
		if (m_Move)
			return;

		if (!m_CurrentBuilding)
		{
			Building b = explorer.GetNextUnvisitedBuilding(bot);
			if (!b)
				return;
			m_CurrentBuilding = b;
			m_BuildingTime = 0.0;
			m_PointIndex = 0;
			m_InteriorPoints = new array<vector>();
			dmBuildingInteriorMap.Get().GetRoamWorldPoints(b, m_InteriorPoints);
		}

		vector goal;
		if (m_PointIndex < m_InteriorPoints.Count())
		{
			goal = m_InteriorPoints[m_PointIndex];
			m_PointIndex = m_PointIndex + 1;
		}
		else
		{
			goal = m_CurrentBuilding.GetPosition();
		}

		m_Move = new dmBotIntent_MoveTo();
		m_Move.m_Goal = goal;
		m_Move.m_ReachDistance = DM_EXPLORE_INTERIOR_REACH;
		m_Move.m_Priority = dmBotIntentPriority.DESIRABLE;
		bot.AddFSMIntent(m_Move);
	}

	//! Проверить условия выхода из исследования (looting.json). При срабатывании —
	//! SetNothingToDo(true) и возврат true (OnUpdate вернёт EXIT).
	bool CheckExits(dmAISurvivor bot)
	{
		dmExplorer explorer = bot.GetExplorer();
		dmExplorationConfig cfg = dmLootingSettings.Get().GetExploration();
		if (!cfg)
			return false;

		int visited = explorer.LocationVisitedCount();
		int unvisited = explorer.LocationUnvisitedCount();
		float visitedF = visited;
		float totalF = visited + unvisited;
		float time = explorer.GetTimeInLocation();
		bool done = false;

		if (unvisited == 0 && totalF > 0.0)
			done = true;
		if (totalF == 0.0 && time > DM_EXPLORE_EMPTY_LOCATION_TIME)
			done = true;
		if (cfg.ExitVisitedCount > 0 && visited >= cfg.ExitVisitedCount)
			done = true;
		if (cfg.ExitVisitedPercent > 0.0 && totalF > 0.0 && visitedF > totalF * cfg.ExitVisitedPercent)
			done = true;
		if (cfg.ExitVisitedPercentTime > 0.0 && totalF > 0.0 && visitedF > totalF * cfg.ExitVisitedPercentTime && time > cfg.ExitTimePercentSeconds)
			done = true;
		if (cfg.ExitTimeSeconds > 0.0 && time > cfg.ExitTimeSeconds)
			done = true;

		if (!done)
			return false;

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] Explore done: visited=" + visited + " unvisited=" + unvisited + " time=" + time);
		#endif
		explorer.SetNothingToDo(true);
		return true;
	}

	override void OnExit(dmBotState to)
	{
		if (m_Move) { m_Move.Finish(); m_Move = null; }
		if (m_PickUp) { m_PickUp.Finish(); m_PickUp = null; }
	}
};
