class dmBotState_Exploration : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;     // маршрут к зданию (DESIRABLE)
	ref dmBotIntent_PickUp m_PickUp;   // подбор предмета (EXCLUSIVE)
	Building m_CurrentBuilding;
	ref array<vector> m_InteriorPoints; // интерьер-точки текущего здания (исходный список)
	ref dmBuildingRoutePlanner m_Planner; // жадный планировщик обхода интерьера (переиспользуется)
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
		m_Planner = new dmBuildingRoutePlanner();
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
			if (!bot.IsExplorationStandStill())
				EnsureExplore(bot, pawn);
		}

		TickRoute(pawn);

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
			bot.Discard(item);
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

	//! Выбор лучшего предмета рядом по desire. «Оценил не нужно» → Ignore с TTL,
	//! чтобы не пере-сканировать и не пере-оценивать тот же предмет каждый тик.
	EntityAI pickUpItemSelect(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		array<EntityAI> items = dmLoot.ScanNearbyItems(pawn, DM_EXPLORE_PICKUP_RADIUS);
		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] Рядом " + items.Count() + " вещей, выбираю, что подобрать");
		#endif

		//! Чистый выбор по desire — БЕЗ CanCarry/выброса: мутация только для победителя.
		ItemBase best = null;
		float bestDesire = 0.0;
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			ItemBase item = ItemBase.Cast(items[i]);
			if (!item) continue;

			//! Ruined — не подбираем (дёшево: только здоровье, без рейтинга/переборки).
			if (item.IsDamageDestroyed() || item.GetHealth01() <= 0.0)
				continue;

			float desire = bot.CalcDesired(item);
			if (desire <= DM_EXPLORE_PICKUP_THRESHOLD)
			{
				bot.GetWishlist().Ignore(item);   // оценил «не нужно» → игнор (TTL), не пере-сканировать каждый тик
				continue;
			}

			if (desire > bestDesire)
			{
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

		//! Единственная точка мутации: сделать место под победителя, затем CanCarry.
		dmLoot.ClearWeaponRatingCache();
		if (!EnsureCarry(bot, pawn, best, bestDesire))
		{
			bot.Discard(best);   // НЕ пересоревноваться за ту же вещь
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] Не могу это нести: " + best.GetType() + " pos=" + best.GetPosition());
			#endif
			return null;
		}

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] Мне нравится: " + best.GetType() + " на " + bestDesire);
		#endif
		return best;
	}

	//! «Сделать место» под победителя (единственная мутация, вне цикла выбора):
	//! мили-лимит, конкуренция огнестрела за SHOULDER/MELEE, либо эвакуация.
	//! true = теперь CanCarry.
	bool EnsureCarry(dmAISurvivor bot, dmAISurvivorBase pawn, ItemBase item, float desire)
	{
		dmLootCategory cat = dmLoot.GetCategory(item);

		//! Мили: лимит DM_LOOT_MELEE_MAX суммарно (независимо от того, есть ли место).
		if (cat == dmLootCategory.MELEE)
		{
			if (!ApplyMeleeLimit(bot, pawn))
				return false;   // на лимите и карго-мили нет — не опустошаем слот, отвергаем
			return dmLoot.CanCarry(pawn, item);
		}

		if (dmLoot.CanCarry(pawn, item))
			return true;

		//! Огнестрел (не пистолет) при занятых слотах — конкуренция за SHOULDER/MELEE.
		if (item.IsInherited(Weapon_Base) && !item.IsInherited(Pistol_Base))
			return CompeteFirearm(bot, pawn, item);

		//! Одежда — подтвердить замену в том же слоте (слот валиден → надеть поверх);
		//! сам сброс старого + перенос карго делает InventoryChangeClothes.
		if (cat == dmLootCategory.CLOTHING)
			return CompeteClothing(bot, pawn, item);

		//! Прочее — эвакуация (полный репак, если желание это оправдывает).
		return EvacuateForItem(bot, pawn, item, desire);
	}

	//! Эвакуация под «прочий» предмет (food/ammo/magazine/medical/repair/other):
	//! при отсутствии места и достаточном желании вернуть true — сам предмет не
	//! выбрасываем, полный репак (drop-all → собрать с конца по рангу) делает
	//! dmBotIntent_PickUp.Evacuate через inventory-frames.
	bool EvacuateForItem(dmAISurvivor bot, dmAISurvivorBase pawn, ItemBase item, float desire)
	{
		dmRequirements req = bot.GetRequirements();
		if (!req)
			return false;
		ref array<EntityAI> order = req.GetDiscardOrder();
		if (order.Count() == 0)
			return false;
		if (!req.WorthEvacuating(desire))
			return false;
		return true;   // полный репак делает PickUp.Evacuate у предмета (через inventory-frames)
	}

	//! Выбросить предмет и внести его в игнор (чтобы не подбирать выброшенное заново).
	bool DropAndIgnore(dmAISurvivor bot, dmAISurvivorBase pawn, EntityAI item)
	{
		if (!item || !pawn.DropItem(ItemBase.Cast(item)))
			return false;
		bot.Discard(item);
		return true;
	}

	//! Конкуренция огнестрела за SHOULDER/MELEE: выбросить ruined в слотах, затем —
	//! слабейший занятый слот (по RateWeapon), если он слабее кандидата. Единый
	//! механизм покрывает и «мили против мили», и «слабый огнестрел против кандидата».
	//! true = слот освобождён и кандидат помещается.
	bool CompeteFirearm(dmAISurvivor bot, dmAISurvivorBase pawn, ItemBase item)
	{
		float rate = dmLoot.RateWeapon(item, bot);
		if (rate <= 0.0)
			return false;

		GameInventory inv = pawn.GetInventory();
		if (!inv)
			return false;

		//! Ruined в слотах SHOULDER/MELEE — выбросить немедленно.
		ItemBase slotted = ItemBase.Cast(inv.FindAttachment(InventorySlots.SHOULDER));
		if (slotted && dmLoot.RateWeapon(slotted, bot) == -1.0)
			DropAndIgnore(bot, pawn, slotted);
		slotted = ItemBase.Cast(inv.FindAttachment(InventorySlots.MELEE));
		if (slotted && dmLoot.RateWeapon(slotted, bot) == -1.0)
			DropAndIgnore(bot, pawn, slotted);

		if (dmLoot.CanCarry(pawn, item))
			return true;

		//! Слабейший занятый SHOULDER/MELEE — вытеснить, если слабее кандидата.
		array<EntityAI> occupied = new array<EntityAI>();
		slotted = ItemBase.Cast(inv.FindAttachment(InventorySlots.SHOULDER));
		if (slotted)
			occupied.Insert(slotted);
		slotted = ItemBase.Cast(inv.FindAttachment(InventorySlots.MELEE));
		if (slotted)
			occupied.Insert(slotted);

		EntityAI weaker = dmLoot.FindWeakest(occupied, bot);
		if (weaker && dmLoot.RateWeapon(ItemBase.Cast(weaker), bot) < rate)
		{
			DropAndIgnore(bot, pawn, weaker);
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] огнестрел вытесняет " + weaker.GetType() + " ради " + item.GetType());
			#endif
		}
		return dmLoot.CanCarry(pawn, item);
	}

	//! Одежда: замена в ТОМ ЖЕ слоте. Старое НЕ выбрасываем здесь — смену с переносом
	//! карго делает InventoryChangeClothes внутри dmBotIntent_PickUp (сброс старого →
	//! надеть новое → перенести карго). Раньше старое выбрасывалось заранее (DropAndIgnore),
	//! в момент смены слот был уже пуст → InventoryChangeClothes не строила цепочку
	//! переноса карго → лут оставался в сброшенной одежде (bugreport #72).
	bool CompeteClothing(dmAISurvivor bot, dmAISurvivorBase pawn, ItemBase item)
	{
		return dmLoot.GetClothingSlotId(pawn, item) != InventorySlots.INVALID;
	}

	//! Лимит мили: если уже >= DM_LOOT_MELEE_MAX мили — выбросить слабейшее мили В КАРГО.
	//! Слоты (MELEE/SHOULDER) и руки НЕ трогаем — слот не должен пустеть.
	//! true = под лимитом ИЛИ выбросили карго-место (можно CanCarry); false = на лимите
	//! и карго-мили нет (входящее отвергаем, чтобы не опустошать слот).
	bool ApplyMeleeLimit(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		dmRequirements req = bot.GetRequirements();
		if (!req)
			return true;

		array<EntityAI> meleeItems = req.GetAllMelee(bot);
		if (meleeItems.Count() < DM_LOOT_MELEE_MAX)
			return true;

		//! Только карго-мили участвует в конкуренции — слоты и руки не трогаем.
		array<EntityAI> cargoMelee = new array<EntityAI>();
		InventoryLocation loc = new InventoryLocation();
		int i;
		for (i = 0; i < meleeItems.Count(); i++)
		{
			EntityAI candidate = meleeItems[i];
			if (!candidate)
				continue;
			if (candidate.GetInventory().GetCurrentInventoryLocation(loc))
			{
				if (loc.GetType() == InventoryLocationType.CARGO)
					cargoMelee.Insert(candidate);
			}
		}

		if (cargoMelee.Count() == 0)
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] мили лимит: карго-мили нет — входящее отвергаю (слот не трогаю)");
			#endif
			return false;
		}

		EntityAI weakest = dmLoot.FindWeakest(cargoMelee, bot);
		if (!DropAndIgnore(bot, pawn, weakest))
			return false;

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] мили лимит: выбрасываю " + weakest.GetType() + " (карго)");
		#endif
		return true;
	}

	//! Обход здания: планировщик (жадный коммивояжёр) упорядочивает интерьер-точки,
	//! бот идёт по готовому маршруту; по опустошении — здание помечается посещённым.
	void EnsureExplore(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		dmExplorer explorer = bot.GetExplorer();

		if (m_CurrentBuilding && m_BuildingTime > DM_EXPLORE_BUILDING_TIMEOUT)
		{
			if (m_Move) { m_Move.Finish(); m_Move = null; }
			dmBotLog.Error("[Loot] Explore: здание слишком долго, пропускаю " + m_CurrentBuilding.GetType());
			explorer.MarkLocationBuildingVisited(m_CurrentBuilding);
			ResetBuilding();
		}

		if (m_Move && (m_Move.IsFinished() || m_Move.IsExpired() || m_Move.IsFailed()))
			m_Move = null;

		if (!m_Move && m_CurrentBuilding)
		{
			vector next;
			if (m_Planner.NextPoint(next))
			{
				StartMoveTo(bot, next);
			}
			else if (m_Planner.IsDone())
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] Explore: здание обойдено " + m_CurrentBuilding.GetType());
				#endif
				explorer.MarkLocationBuildingVisited(m_CurrentBuilding);
				ResetBuilding();
			}
		}

		if (!m_CurrentBuilding)
		{
			Building b = explorer.GetNextUnvisitedBuilding(bot);
			if (!b)
				return;
			m_CurrentBuilding = b;
			m_BuildingTime = 0.0;
			m_InteriorPoints = new array<vector>();
			dmBuildingInteriorMap.Get().GetRoamWorldPoints(b, m_InteriorPoints);
			if (m_InteriorPoints.Count() == 0)
				m_InteriorPoints.Insert(b.GetPosition());

			m_Planner.Init(pawn.GetPosition(), m_InteriorPoints);
		}
	}

	//! Пер-тик сопровождение маршрута: шаг планировщика + проксимити-скип «увидел
	//! точку → посетил». Первую точку бот получает через обычный цикл EnsureExplore.
	void TickRoute(dmAISurvivorBase pawn)
	{
		if (!m_Planner)
			return;

		if (!m_Planner.IsDone())
			m_Planner.Tick();

		m_Planner.RemovePointsNear(pawn.GetPosition(), DM_EXPLORE_INTERIOR_SEE);
	}

	//! Выдать MoveTo к точке маршрута (DESIRABLE, как и раньше).
	void StartMoveTo(dmAISurvivor bot, vector goal)
	{
		m_Move = new dmBotIntent_MoveTo();
		m_Move.m_Goal = goal;
		m_Move.m_ReachDistance = DM_EXPLORE_INTERIOR_REACH;
		m_Move.m_Priority = dmBotIntentPriority.DESIRABLE;
		bot.AddFSMIntent(m_Move);
	}

	//! Сбросить текущее здание (планировщик переиспользуется для следующего).
	void ResetBuilding()
	{
		m_CurrentBuilding = null;
		m_InteriorPoints = null;
		m_BuildingTime = 0.0;
	}

	//! Проверить условия выхода из исследования (looting.json). При срабатывании —
	//! SetNothingToDo(true) и возврат true (OnUpdate вернёт EXIT).
	bool CheckExits(dmAISurvivor bot)
	{
		//! Тестовый флаг «стоять на месте»: не выгоняем из Exploration по времени/локации.
		if (bot.IsExplorationStandStill())
			return false;

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
