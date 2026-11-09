class dmBotState_Exploration : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;     // маршрут к зданию (DESIRABLE)
	ref dmBotIntent_PickUp m_PickUp;   // подбор предмета (EXCLUSIVE)
	Building m_CurrentBuilding;
	ref array<vector> m_InteriorPoints; // интерьер-точки текущего здания (исходный список)
	ref dmBuildingRoutePlanner m_Planner; // жадный планировщик обхода интерьера (переиспользуется)
	float m_DropCooldown;
	float m_BuildingTime;               // сколько времени бот возится с текущим зданием
	float m_ContainerScanCooldown;      // троттлинг контейнерного скана (~1 Гц)
	ref array<EntityAI> m_ContainerItems; // кэш результата контейнерного скана между тиками
	float m_CorpseScanCooldown;         // троттлинг скана трупов (~0.5 Гц)
	EntityAI m_Corpse;                  // текущий труп-цель (стрип)
	ref dmBotIntent_MoveTo m_CorpseMove; // маршрут к трупу (DESIRABLE)
	ref array<EntityAI> m_StrippedCorpses; // уже остриженные трупы (не ре-стрипать)
	float m_WreckScanCooldown;          // троттлинг скана приоритетных событий (~0.1 Гц)
	bool m_SelectingBuilding;           // идёт скан выбора здания
	ref dmBuildingScanCursor m_SelectCursor; // курсор скана
	int m_SelectPhase;                  // 0 = врек-скан, 1 = выбор здания
	ref array<Building> m_SelectWrecks; // накопленные вреки (фаза 0)

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
		m_ContainerScanCooldown = 0.0;
		m_ContainerItems = null;
		m_CorpseScanCooldown = 0.0;
		m_Corpse = null;
		m_CorpseMove = null;
		m_StrippedCorpses = new array<EntityAI>();
		m_WreckScanCooldown = 0.0;
		m_SelectingBuilding = false;
		m_SelectCursor = null;
		m_SelectPhase = 0;
		m_SelectWrecks = new array<Building>();

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

		#ifdef DM_BOT_PROFILE
		float _t0 = 0.0;
		#endif

		//! Суб-спаны Exploration (атрибуция фрейм-спайков к операциям). Ручной
		//! замер через Record вместо dmBotSpan: у Enfusion нет блочной области
		//! видимости, деструктор scope-guard сработал бы на выходе из OnUpdate.
		#ifdef DM_BOT_PROFILE
		_t0 = GetGame().GetTickTime();
		#endif
		MaybeDrop(bot, pawn, pDt);
		#ifdef DM_BOT_PROFILE
		if (dmBotProfiler.IsEnabled())
			dmBotProfiler.Record("Expl.Drop", GetGame().GetTickTime() - _t0);
		#endif

		m_ContainerScanCooldown -= pDt;
		m_CorpseScanCooldown -= pDt;
		m_WreckScanCooldown -= pDt;

		bool pickingUp;
		#ifdef DM_BOT_PROFILE
		_t0 = GetGame().GetTickTime();
		#endif
		pickingUp = EnsurePickUp(bot, pawn);
		#ifdef DM_BOT_PROFILE
		if (dmBotProfiler.IsEnabled())
			dmBotProfiler.Record("Expl.PickUp", GetGame().GetTickTime() - _t0);
		#endif

		if (!pickingUp)
		{
			#ifdef DM_BOT_PROFILE
			_t0 = GetGame().GetTickTime();
			#endif
			EnsureCorpse(bot, pawn);
			#ifdef DM_BOT_PROFILE
			if (dmBotProfiler.IsEnabled())
				dmBotProfiler.Record("Expl.Corpse", GetGame().GetTickTime() - _t0);
			#endif
			if (!m_CorpseMove && !bot.IsExplorationStandStill())
			{
				#ifdef DM_BOT_PROFILE
				_t0 = GetGame().GetTickTime();
				#endif
				EnsureExplore(bot, pawn);
				#ifdef DM_BOT_PROFILE
				if (dmBotProfiler.IsEnabled())
					dmBotProfiler.Record("Expl.Explore", GetGame().GetTickTime() - _t0);
				#endif
			}
		}

		#ifdef DM_BOT_PROFILE
		_t0 = GetGame().GetTickTime();
		#endif
		TickRoute(pawn);
		#ifdef DM_BOT_PROFILE
		if (dmBotProfiler.IsEnabled())
			dmBotProfiler.Record("Expl.Route", GetGame().GetTickTime() - _t0);
		#endif

		#ifdef DM_BOT_PROFILE
		_t0 = GetGame().GetTickTime();
		#endif
		bot.GetExplorer().RefreshBuildingsIfEmpty(pDt);
		#ifdef DM_BOT_PROFILE
		if (dmBotProfiler.IsEnabled())
			dmBotProfiler.Record("Expl.Refresh", GetGame().GetTickTime() - _t0);
		#endif

		if (m_CurrentBuilding)
			m_BuildingTime = m_BuildingTime + pDt;

		bot.GetExplorer().TickLocationTime(pDt);

		bool exiting;
		#ifdef DM_BOT_PROFILE
		_t0 = GetGame().GetTickTime();
		#endif
		exiting = CheckExits(bot);
		#ifdef DM_BOT_PROFILE
		if (dmBotProfiler.IsEnabled())
			dmBotProfiler.Record("Expl.Exits", GetGame().GetTickTime() - _t0);
		#endif
		if (exiting)
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
			dmBotLog.Debug(GetOwner().GetName(), "[Loot] Полно места в инвентаре");
		}
		#endif
		if (!req || !req.IsFull())
			return;

		ref array<EntityAI> order = req.GetDiscardOrder();
		if (order.Count() == 0)
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug(GetOwner().GetName(), "[Loot] Выбрасывать нечего");
			#endif
			return;
		}

		EntityAI item = order[0];
		if (pawn.DropItem(ItemBase.Cast(item)))
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug(GetOwner().GetName(), "[Loot] Выбрасываю: " + item.GetType());
			#endif
			bot.Discard(item);
		}
	}

	EntityAI m_PickUpItem;

	//! Оппортунистический подбор: самый желаемый предмет рядом (порог).
	bool EnsurePickUp(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		if (m_PickUp)
		{
			if ( m_PickUpItem )
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug(GetOwner().GetName(), "[Loot] Ну его нафиг, не могу достать " + m_PickUpItem.GetType() + " тут " + m_PickUpItem.GetPosition());
				#endif
				if (m_PickUp.IsExpired() || m_PickUp.IsFailed()) bot.Discard(m_PickUpItem);
			}
			if (m_PickUp.IsFinished() || m_PickUp.IsExpired() || m_PickUp.IsFailed()) m_PickUp = null;
		}
		if (m_PickUp) return true;

		m_PickUpItem = pickUpItemSelect(bot, pawn);
		if ( m_PickUpItem )
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug(GetOwner().GetName(), "[Loot] Собираюсь залутать " + m_PickUpItem.GetType() + " тут " + m_PickUpItem.GetPosition());
			#endif
			m_PickUp = new dmBotIntent_PickUp();
			m_PickUp.m_Item = m_PickUpItem;
			bot.AddFSMIntent(m_PickUp);
			dmLoot.RemoveNearbyItem(pawn, m_PickUpItem, DM_EXPLORE_PICKUP_RADIUS);
			return true;
		}
		return false;
	}

	//! Выбор лучшего предмета рядом по desire. «Оценил не нужно» → Ignore с TTL,
	//! чтобы не пере-сканировать и не пере-оценивать тот же предмет каждый тик.
	EntityAI pickUpItemSelect(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		//! Контейнеры троттлим ~1 Гц (12-м бокс дороже 3-м лужного), результат кэшируем
		//! в m_ContainerItems. Лужный скан — per-tick (дёшев).
		if (m_ContainerScanCooldown <= 0.0)
		{
			m_ContainerScanCooldown = DM_EXPLORE_CONTAINER_SCAN_INTERVAL;
			m_ContainerItems = dmLoot.ScanContainerItems(pawn, DM_EXPLORE_CONTAINER_SCAN_RADIUS);
		}

		array<EntityAI> items = dmLoot.ScanNearbyItems(pawn, DM_EXPLORE_PICKUP_RADIUS);
		int groundCount = items.Count();
		int containerCount = 0;
		int i;
		if (m_ContainerItems)
		{
			containerCount = m_ContainerItems.Count();
			for (i = 0; i < containerCount; i++)
				items.Insert(m_ContainerItems[i]);
		}

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug(GetOwner().GetName(), "[Loot] Рядом " + groundCount + " на земле + " + containerCount + " в контейнерах, выбираю, что подобрать");
		#endif

		//! Чистый выбор по desire — БЕЗ CanCarry/выброса: мутация только для победителя.
		ItemBase best = null;
		float bestDesire = 0.0;
		for (i = 0; i < items.Count(); i++)
		{
			ItemBase item = ItemBase.Cast(items[i]);
			if (!item) continue;

			//! Уже в чьей-то иерархии (кэш контейнеров мог устареть на тик) — пропуск.
			if (item.GetHierarchyRootPlayer())
				continue;

			//! Контейнер сам не подбираем (берём только его содержимое).
			if (dmLoot.IsLootContainer(item))
				continue;

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
			dmBotLog.Debug(GetOwner().GetName(), "[Loot] Все хлам");
			#endif
			return null;
		}

		//! Единственная точка мутации: сделать место под победителя, затем CanCarry.
		dmLoot.ClearWeaponRatingCache();
		if (!EnsureCarry(bot, pawn, best, bestDesire))
		{
			bot.Discard(best);   // НЕ пересоревноваться за ту же вещь
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug(GetOwner().GetName(), "[Loot] Не могу это нести: " + best.GetType() + " pos=" + best.GetPosition());
			#endif
			return null;
		}

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug(GetOwner().GetName(), "[Loot] Мне нравится: " + best.GetType() + " на " + bestDesire);
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
			dmBotLog.Debug(GetOwner().GetName(), "[Loot] огнестрел вытесняет " + weaker.GetType() + " ради " + item.GetType());
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
			dmBotLog.Debug(GetOwner().GetName(), "[Loot] мили лимит: карго-мили нет — входящее отвергаю (слот не трогаю)");
			#endif
			return false;
		}

		EntityAI weakest = dmLoot.FindWeakest(cargoMelee, bot);
		if (!DropAndIgnore(bot, pawn, weakest))
			return false;

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug(GetOwner().GetName(), "[Loot] мили лимит: выбрасываю " + weakest.GetType() + " (карго)");
		#endif
		return true;
	}

	//! Обход здания: планировщик (жадный коммивояжёр) упорядочивает интерьер-точки,
	//! бот идёт по готовому маршруту; по опустошении — здание помечается посещённым.
	//! Следующее здание выбирается чанкованно, с приоритетом вреков (см. EnsureBuildingSelection).
	void EnsureExplore(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		if (m_CurrentBuilding && m_BuildingTime > DM_EXPLORE_BUILDING_TIMEOUT)
		{
			if (m_Move) { m_Move.Finish(); m_Move = null; }
			dmBotLog.Error(GetOwner().GetName(), "[Loot] Explore: здание слишком долго, пропускаю " + m_CurrentBuilding.GetType());
			MarkBuildingVisited(bot, m_CurrentBuilding);
			ResetBuilding();
		}

		if (m_Move && (m_Move.IsFinished() || m_Move.IsExpired() || m_Move.IsFailed()))
		{
			if (m_Move.IsFailed() && m_CurrentBuilding && IsWreck(m_CurrentBuilding))
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug(GetOwner().GetName(), "[Loot] Врек недостижим, пропускаю: " + m_CurrentBuilding.GetType());
				#endif
				MarkBuildingVisited(bot, m_CurrentBuilding);
				ResetBuilding();
			}
			m_Move = null;
		}

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
				dmBotLog.Debug(GetOwner().GetName(), "[Loot] Explore: здание обойдено " + m_CurrentBuilding.GetType());
				#endif
				MarkBuildingVisited(bot, m_CurrentBuilding);
				ResetBuilding();
			}
		}

		if (!m_CurrentBuilding)
		{
			EnsureBuildingSelection(bot, pawn);
		}
	}

	//! Пометить здание посещённым: локация + (если врек) отдельная врек-отметка.
	void MarkBuildingVisited(dmAISurvivor bot, Building b)
	{
		if (!b)
			return;
		dmExplorer explorer = bot.GetExplorer();
		explorer.MarkLocationBuildingVisited(b);
		if (IsWreck(b))
			explorer.MarkWreckVisited(b);
	}

	//! Здание — приоритетное событие (врек MILITARY_WRECK)?
	bool IsWreck(Building b)
	{
		return b && dmBuildingInteriorMap.Get().GetType(b.GetType()) == dmWorldPOIType.MILITARY_WRECK;
	}

	//! Достаточно ли энергии/воды для приоритетного похода к событию. Порог витал
	//! градуируется по дистанции до врека: чем ближе, тем меньше нужно (см. константы).
	bool HasEnergyForPriorityEvent(dmAISurvivorBase pawn, float dist)
	{
		float min = DM_PRIORITY_EVENT_MIN_FAR;
		if (dist < DM_PRIORITY_EVENT_DIST_NEAR)
			min = DM_PRIORITY_EVENT_MIN_NEAR;
		else if (dist < DM_PRIORITY_EVENT_DIST_MID)
			min = DM_PRIORITY_EVENT_MIN_MID;
		return pawn.GetStatEnergy().Get() >= min && pawn.GetStatWater().Get() >= min;
	}

	//! Чанкованный выбор следующего здания (state machine, раз в тик). Заменяет
	//! синхронный ChooseNextBuilding: O(POI)-скан (врек-скан + nearest-поиск здания)
	//! размазан по тикам бюджетом DM_EXPLORE_SCAN_BUDGET, чтобы крупный POI не
	//! давал пер-фрейм спайк. Фаза 0 — врек-скан, фаза 1 — выбор ближайшего здания.
	void EnsureBuildingSelection(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		dmExplorer explorer = bot.GetExplorer();
		dmWorldPoiLocation loc = null;
		bool done;

		if (!m_SelectingBuilding)
		{
			m_SelectingBuilding = true;
			loc = explorer.GetCurrentLocation();
			if (m_WreckScanCooldown <= 0.0 && loc)
			{
				m_WreckScanCooldown = DM_EXPLORE_WRECK_SCAN_INTERVAL;
				m_SelectPhase = 0;
				m_SelectCursor = new dmBuildingScanCursor();
				m_SelectWrecks.Clear();
			}
			else
			{
				m_SelectPhase = 1;
				explorer.StartNearestUnvisitedScan(m_SelectCursor);
			}
		}

		if (m_SelectPhase == 0)
		{
			loc = explorer.GetCurrentLocation();
			done = true;
			if (loc)
				done = dmLiveBuildingRegistry.Get().ContinueBuildingsOfType(loc.Id, dmWorldPOIType.MILITARY_WRECK, m_SelectCursor, m_SelectWrecks, DM_EXPLORE_SCAN_BUDGET);
			if (!done)
				return;

			Building wreck = SelectNearestUnvisitedWreck(bot, pawn);
			if (wreck)
			{
				vector pos = pawn.GetPosition();
				vector wpos = wreck.GetPosition();
				float dist = Math.Sqrt(dmMath.DistSq2D(pos, wpos));
				if (HasEnergyForPriorityEvent(pawn, dist))
				{
					#ifdef DM_BOT_DEBUG_LOOTING
					dmBotLog.Debug(GetOwner().GetName(), "[Loot] Нашёл событие/врек: " + wreck.GetType() + " — приоритет, иду");
					#endif
					m_SelectingBuilding = false;
					SetCurrentBuilding(pawn, wreck);
					return;
				}
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug(GetOwner().GetName(), "[Loot] Врек в радиусе, но мало энергии/воды: " + wreck.GetType() + " dist=" + dist + " — пропускаю");
				#endif
			}

			m_SelectPhase = 1;
			explorer.StartNearestUnvisitedScan(m_SelectCursor);
			return;
		}

		if (explorer.ContinueNearestUnvisitedScan(bot, m_SelectCursor, DM_EXPLORE_SCAN_BUDGET))
		{
			m_SelectingBuilding = false;
			Building best = m_SelectCursor.m_Best;
			if (best)
				SetCurrentBuilding(pawn, best);
		}
	}

	//! Из накопленных вреков (m_SelectWrecks) — ближайший НЕпосещённый в радиусе
	//! DM_EXPLORE_WRECK_RADIUS (2D-квадрат дистанции), или null. Аналог прежнего
	//! dmExplorer.GetNearestUnvisitedWreck, но по уже чанкованно-собранному списку.
	Building SelectNearestUnvisitedWreck(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		dmExplorer explorer = bot.GetExplorer();
		vector pos = pawn.GetPosition();
		float radiusSq = DM_EXPLORE_WRECK_RADIUS * DM_EXPLORE_WRECK_RADIUS;
		Building nearest = null;
		float bestSq = radiusSq;
		int i;
		for (i = 0; i < m_SelectWrecks.Count(); i++)
		{
			Building b = m_SelectWrecks[i];
			if (!b || !b.IsAlive())
				continue;
			if (explorer.HasVisitedWreck(b))
				continue;
			vector bPos = b.GetPosition();
			float dSq = dmMath.DistSq2D(bPos, pos);
			if (dSq > radiusSq)
				continue;
			if (!nearest || dSq < bestSq)
			{
				nearest = b;
				bestSq = dSq;
			}
		}
		return nearest;
	}

	//! Назначить выбранное здание текущим и инициализировать планировщик обхода.
	void SetCurrentBuilding(dmAISurvivorBase pawn, Building b)
	{
		m_CurrentBuilding = b;
		m_BuildingTime = 0.0;
		m_InteriorPoints = new array<vector>();
		dmBuildingInteriorMap.Get().GetRoamWorldPoints(b, m_InteriorPoints);
		if (m_InteriorPoints.Count() == 0)
			m_InteriorPoints.Insert(b.GetPosition());
		m_Planner.Init(pawn.GetPosition(), m_InteriorPoints);
	}

	//! Труп-лут: обнаружить труп → идти к нему (DESIRABLE) → стрип на землю. Выпавшее
	//! подберётся штатным EnsurePickUp следующим тиком. Не конфликтует с EnsureExplore:
	//! у трупа свой маршрут m_CorpseMove, обход здания не идёт параллельно (прерываем
	//! m_Move при старте подхода к трупу).
	void EnsureCorpse(dmAISurvivor bot, dmAISurvivorBase pawn)
	{
		// Завершился маршрут к трупу.
		if (m_CorpseMove && (m_CorpseMove.IsFinished() || m_CorpseMove.IsExpired() || m_CorpseMove.IsFailed()))
			m_CorpseMove = null;

		// Труп-цель удалён (деспавн).
		if (m_Corpse && m_Corpse.IsSetForDeletion())
		{
			m_Corpse = null;
			m_CorpseMove = null;
		}

		// Бот у трупа и не идёт — стрип.
		if (m_Corpse && !m_CorpseMove)
		{
			vector pos = pawn.GetPosition();
			vector cpos = m_Corpse.GetPosition();
			if (vector.Distance(pos, cpos) <= DM_EXPLORE_CORPSE_REACH)
			{
				array<EntityAI> dropped = new array<EntityAI>();
				dmLoot.StripCorpse(m_Corpse, dropped);
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug(GetOwner().GetName(), "[Loot] Острижен труп: " + m_Corpse.GetType() + " вещей " + dropped.Count());
				#endif
				m_StrippedCorpses.Insert(m_Corpse);
				if (m_StrippedCorpses.Count() > DM_EXPLORE_CORPSE_STRIPPED_MAX)
					m_StrippedCorpses.Remove(0);   // старейший вытесняется (лимит роста)
				m_Corpse = null;
			}
		}

		// Идём к трупу — не пере-создавать.
		if (m_CorpseMove)
			return;

		// Нет цели — троттл-скан ближайшего не-остриженного трупа.
		if (!m_Corpse && m_CorpseScanCooldown <= 0.0)
		{
			m_CorpseScanCooldown = DM_EXPLORE_CORPSE_SCAN_INTERVAL;
			CleanStrippedCorpses();
			m_Corpse = FindNearestCorpse(pawn);
			#ifdef DM_BOT_DEBUG_LOOTING
			if (m_Corpse)
				dmBotLog.Debug(GetOwner().GetName(), "[Loot] Нашёл труп: " + m_Corpse.GetType() + " на " + vector.Distance(pawn.GetPosition(), m_Corpse.GetPosition()) + " м");
			#endif
		}

		// Есть цель, нет маршрута — идти к трупу (прервав обход здания).
		if (m_Corpse && !m_CorpseMove)
		{
			if (m_Move) { m_Move.Finish(); m_Move = null; }
			m_CorpseMove = new dmBotIntent_MoveTo();
			m_CorpseMove.m_Goal = m_Corpse.GetPosition();
			m_CorpseMove.m_ReachDistance = DM_EXPLORE_CORPSE_REACH;
			m_CorpseMove.m_Priority = dmBotIntentPriority.DESIRABLE;
			bot.AddFSMIntent(m_CorpseMove);
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug(GetOwner().GetName(), "[Loot] Иду к трупу: " + m_Corpse.GetType());
			#endif
		}
	}

	//! Лениво выкинуть из m_StrippedCorpses удалённые (деспавн) трупы.
	void CleanStrippedCorpses()
	{
		int i;
		for (i = m_StrippedCorpses.Count() - 1; i >= 0; i--)
		{
			EntityAI c = m_StrippedCorpses[i];
			if (!c || c.IsSetForDeletion())
				m_StrippedCorpses.Remove(i);
		}
	}

	//! Ближайший НЕ-остриженный труп в DM_EXPLORE_CORPSE_RADIUS (по 2D-квадрату
	//! дистанции, Y игнорируется — труп этажом выше не должен «выигрывать»).
	EntityAI FindNearestCorpse(dmAISurvivorBase pawn)
	{
		array<EntityAI> corpses = dmLoot.ScanCorpses(pawn, DM_EXPLORE_CORPSE_RADIUS);
		EntityAI nearest = null;
		float best = DM_EXPLORE_CORPSE_RADIUS * DM_EXPLORE_CORPSE_RADIUS;
		vector pos = pawn.GetPosition();
		int i;
		for (i = 0; i < corpses.Count(); i++)
		{
			EntityAI c = corpses[i];
			if (!c || IsCorpseStripped(c))
				continue;
			float d = dmMath.DistSq2D(pos, c.GetPosition());
			if (d < best)
			{
				best = d;
				nearest = c;
			}
		}
		return nearest;
	}

	//! Уже острижен ли труп (по ссылке в m_StrippedCorpses).
	bool IsCorpseStripped(EntityAI corpse)
	{
		int i;
		for (i = 0; i < m_StrippedCorpses.Count(); i++)
		{
			if (m_StrippedCorpses[i] == corpse)
				return true;
		}
		return false;
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
		dmBotLog.Debug(GetOwner().GetName(), "[Loot] Explore done: visited=" + visited + " unvisited=" + unvisited + " time=" + time);
		#endif
		explorer.SetNothingToDo(true);
		return true;
	}

	override void OnExit(dmBotState to)
	{
		if (m_Move) { m_Move.Finish(); m_Move = null; }
		if (m_CorpseMove) { m_CorpseMove.Finish(); m_CorpseMove = null; }
		if (m_PickUp) { m_PickUp.Finish(); m_PickUp = null; }
	}
};
