//! dmLoot — лутинг-движок (статик): классификация предмета → категория, определение
//! места (категорийно: аттачмент-слот → карго всего одетого инвентаря) и инвентарные
//! примитивы (ручной ре-синк сети). Дальше категории питают dmWishlist/dmRequirements/
//! dmNeeds; интенты обращаются к dmLoot, а не к пешке напрямую.

enum dmLootCategory
{
	FOOD,      // еда/вода (Edible_Base)
	WEAPON,    // оружие
	MELEE,
	MAGAZINE,  // магазин
	AMMO,      // пачка патронов
	CLOTHING,  // одежда/обувь
	REPAIR,    // инструменты починки
	MEDICAL,   // бинты/медицина
	OTHER      // прочее
};

//! Вид лечебного предмета (для ИИ-лечения dmBotState_MedicalCare).
enum dmMedicalItemKind
{
	BANDAGE,       // перевязочное (BandageDressing/Rag/Bandana_ColorBase)
	SPLINT,        // шина (Splint)
	PAINKILLER,    // болеутоляющее (PainkillerTablets)
	CHARCOAL,      // активированный уголь (CharcoalTablets)
	TETRACYCLINE,  // антибиотик (TetracyclineAntibiotics)
	VITAMINS       // мультивитамины (VitaminBottle)
};

class dmLoot
{
	//! Надёжная классификация мили (НЕ флаг isMeleeWeapon, который даёт ложные
	//! срабатывания — см. research/combat.md): огнестрел — не мили; мили — по
	//! inventorySlot (Knife/Melee/Shoulder) или itemInfo (Knife/Axe).
	static bool IsMelee(EntityAI item)
	{
		if (!item)
			return false;
		if (item.IsWeapon())
			return false;

		array<string> inventorySlot = new array<string>();
		item.ConfigGetTextArray("inventorySlot", inventorySlot);
		if (inventorySlot.Find("Knife") > -1) return true;
		if (inventorySlot.Find("Melee") > -1) return true;
		if (inventorySlot.Find("Shoulder") > -1) return true;

		array<string> itemInfo = new array<string>();
		item.ConfigGetTextArray("itemInfo", itemInfo);
		if (itemInfo.Find("Knife") > -1) return true;
		if (itemInfo.Find("Axe") > -1) return true;

		return false;
	}

	//! Категория лута для предмета (ванильное наследование). null → OTHER.
	static dmLootCategory GetCategory(ItemBase item)
	{
		if (!item)
			return dmLootCategory.OTHER;

		if (IsMedical(item))
			return dmLootCategory.MEDICAL;

		if (item.IsInherited(Edible_Base))
			return dmLootCategory.FOOD;

		if (item.IsWeapon())
			return dmLootCategory.WEAPON;

		if (IsMelee(item))
			return dmLootCategory.MELEE;

		if (item.IsMagazine())
			return dmLootCategory.MAGAZINE;

		if (item.IsAmmoPile())
			return dmLootCategory.AMMO;

		if (item.IsClothing())
			return dmLootCategory.CLOTHING;

		if (item.IsInherited(WeaponCleaningKit) || item.IsInherited(SewingKit) || item.IsInherited(LeatherSewingKit) || item.IsInherited(DuctTape))
			return dmLootCategory.REPAIR;

		return dmLootCategory.OTHER;
	}

	//! Напиток (жидкость): бутылка/фляга или банка газировки.
	static bool IsDrink(ItemBase item)
	{
		if (!item)
			return false;
		if (item.IsInherited(Bottle_Base))
			return true;
		if (item.IsInherited(SodaCan_ColorBase))
			return true;
		return false;
	}

	//! Первый съедобный предмет в инвентаре (drink=false — еда, drink=true — напиток).
	static ItemBase FindEdible(PlayerBase pawn, bool drink)
	{
		if (!pawn)
			return null;
		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			ItemBase item = ItemBase.Cast(items[i]);
			if (!item)
				continue;
			if (item.IsDamageDestroyed() || item.GetHealth01() <= 0.0)
				continue;
			if (IsMedical(item))
				continue;
			if (item.GetQuantity() <= 0.0 && item.IsOpen())   // пустая открытая бутылка/еда — пропуск; запечатанная консерва (qty=0, !IsOpen) — для вскрытия
				continue;
			if (!item.IsInherited(Edible_Base))
				continue;
			bool isDrink = IsDrink(item);
			if (drink == isDrink)
				return item;
		}
		return null;
	}

	//! Перевязочное: бинт, тряпка или бандана (ванильное наследование).
	static bool IsBandage(ItemBase item)
	{
		if (!item)
			return false;
		if (item.IsInherited(BandageDressing))
			return true;
		if (item.IsInherited(Rag))
			return true;
		if (item.IsInherited(Bandana_ColorBase))
			return true;
		return false;
	}

	//! Подходит ли предмет под заданный вид лечебного (не разрушенный, живой).
	static bool MatchesMedicalKind(ItemBase item, dmMedicalItemKind kind)
	{
		if (kind == dmMedicalItemKind.BANDAGE)
			return IsBandage(item);
		if (kind == dmMedicalItemKind.SPLINT)
			return Splint.Cast(item) != null;
		if (kind == dmMedicalItemKind.PAINKILLER)
			return PainkillerTablets.Cast(item) != null;
		if (kind == dmMedicalItemKind.CHARCOAL)
			return CharcoalTablets.Cast(item) != null;
		if (kind == dmMedicalItemKind.TETRACYCLINE)
			return TetracyclineAntibiotics.Cast(item) != null;
		if (kind == dmMedicalItemKind.VITAMINS)
			return VitaminBottle.Cast(item) != null;
		return false;
	}

	//! Является ли предмет медикаментом.
	static bool IsMedical(ItemBase item)
	{
		if ( IsBandage(item) )
			return true;
		if ( Splint.Cast(item) )
			return true;
		if ( PainkillerTablets.Cast(item) )
			return true;
		if ( CharcoalTablets.Cast(item) )
			return true;
		if ( TetracyclineAntibiotics.Cast(item) )
			return true;
		if ( VitaminBottle.Cast(item) )
			return true;
		if ( IodineTincture.Cast(item) )
			return true;
		if ( DisinfectantAlcohol.Cast(item) )
			return true;
		if ( PurificationTablets.Cast(item) )
			return true;
		if ( ChelatingTablets.Cast(item) )
			return true;
		return false;
	}

	//! Первый подходящий предмет заданного вида в инвентаре пешки (INORDER),
	//! пропуская разрушенные/мёртвые. null — нет.
	static ItemBase FindMedicalItem(PlayerBase pawn, dmMedicalItemKind kind)
	{
		if (!pawn)
			return null;

		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int i;
		for (i = 0; i < items.Count(); i++)
		{
			ItemBase item = ItemBase.Cast(items[i]);
			if (!item)
				continue;
			if (item.IsDamageDestroyed() || item.GetHealth01() <= 0.0)
				continue;
			if (MatchesMedicalKind(item, kind))
				return item;
		}
		return null;
	}

	//! Плата за спавн шины (когда её нет в инвентаре): сначала 1 бинт (BandageDressing),
	//! иначе до DM_MEDICAL_SPLINT_RAG_COST тряпок (сколько есть), иначе false — бесплатно.
	//! Бандана в плату не идёт. Расход через AddQuantity(-n, true).
	static bool PayForSplint(PlayerBase pawn)
	{
		if (!pawn)
			return false;

		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int i;
		ItemBase item;
		ItemBase bandage = null;
		float need;
		float qty;

		for (i = 0; i < items.Count(); i++)
		{
			item = ItemBase.Cast(items[i]);
			if (!item)
				continue;
			if (item.IsDamageDestroyed() || item.GetHealth01() <= 0.0)
				continue;
			if (item.IsInherited(BandageDressing))
			{
				bandage = item;
				break;
			}
		}

		if (bandage)
		{
			bandage.AddQuantity(-1, true);
			return true;
		}

		need = DM_MEDICAL_SPLINT_RAG_COST;
		for (i = 0; i < items.Count(); i++)
		{
			item = ItemBase.Cast(items[i]);
			if (!item)
				continue;
			if (item.IsDamageDestroyed() || item.GetHealth01() <= 0.0)
				continue;
			if (!item.IsInherited(Rag))
				continue;
			qty = item.GetQuantity();
			if (qty >= need)
			{
				item.AddQuantity(-need, true);
				return true;
			}
			item.AddQuantity(-qty, true);
			need = need - qty;
			if (need <= 0.0)
				return true;
		}
		return false;
	}

	//! Предметы-на-земле (ItemBase вне чьего-либо инвентаря) в кубе radius вокруг пешки.
	static array<EntityAI> ScanNearbyItems(PlayerBase pawn, float radius)
	{
		array<EntityAI> result = new array<EntityAI>();
		if (!pawn)
			return result;

		vector botPos = pawn.GetPosition();
		vector minPos = botPos - Vector(radius, radius, radius);
		vector maxPos = botPos + Vector(radius, radius, radius);

		array<EntityAI> entities = new array<EntityAI>();
		DayZPlayerUtils.SceneGetEntitiesInBox(minPos, maxPos, entities, QueryFlags.DYNAMIC);

		int i;
		for (i = 0; i < entities.Count(); i++)
		{
			ItemBase item = ItemBase.Cast(entities[i]);
			if (item && !item.GetHierarchyRootPlayer() && !item.IsDamageDestroyed())
				result.Insert(item);
		}
		return result;
	}

	//! Дропаемое карго (вариант A): нативный EnumerateInventory(LEVELORDER) + фильтр
	//! по InventoryLocationType.CARGO. Возвращает НОВЫЙ массив (как ScanNearbyItems).
	static array<EntityAI> CollectCargoFiltered(PlayerBase pawn)
	{
		array<EntityAI> result = new array<EntityAI>();
		if (!pawn)
			return result;

		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		int i;
		for (i = 0; i < items.Count(); i++)
		{
			InventoryLocation loc = new InventoryLocation();
			if (items[i].GetInventory().GetCurrentInventoryLocation(loc) && loc.GetType() == InventoryLocationType.CARGO)
				result.Insert(items[i]);
		}
		return result;
	}

	//! Дропаемое карго (вариант B): ручная рекурсия. Карго → дропаемое (+ спуск внутрь
	//! контейнера); слоты → ТОЛЬКО спуск (аттач сам НЕ дропаем).
	static array<EntityAI> CollectCargoRecursive(PlayerBase pawn)
	{
		array<EntityAI> result = new array<EntityAI>();
		if (!pawn)
			return result;
		CollectCargoRecursiveEntity(pawn, result);
		return result;
	}

	//! Рекурсивный спуск (вариант B): карго сущности — дропаемое (вставка + спуск),
	//! слоты-аттачи — только спуск (магазин/оптика/GPS сами НЕ дропаются).
	private static void CollectCargoRecursiveEntity(EntityAI ent, array<EntityAI> outItems)
	{
		GameInventory inv = ent.GetInventory();
		if (!inv)
			return;

		int i;
		CargoBase cargo = inv.GetCargo();
		if (cargo)
		{
			for (i = 0; i < cargo.GetItemCount(); i++)
			{
				EntityAI it = cargo.GetItem(i);
				outItems.Insert(it);
				CollectCargoRecursiveEntity(it, outItems);
			}
		}

		for (i = 0; i < inv.GetAttachmentSlotsCount(); i++)
		{
			EntityAI att = inv.FindAttachment(inv.GetAttachmentSlotId(i));
			if (att)
				CollectCargoRecursiveEntity(att, outItems);
		}
	}

	static void RemoveNearbyItem(PlayerBase pawn, EntityAI item, float radius)
	{

	}

	static ref array<int> m_AttachmentSlots = {
		InventorySlots.SHOULDER,
		InventorySlots.MELEE,
		InventorySlots.HEADGEAR,
		InventorySlots.MASK,
		InventorySlots.EYEWEAR,
		InventorySlots.GLOVES,
		InventorySlots.ARMBAND,
		InventorySlots.HIPS,
		InventorySlots.BACK,
		InventorySlots.BODY,
		InventorySlots.VEST,
		InventorySlots.LEGS,
		InventorySlots.FEET
	};

	//! Слот одежды из конфига inventorySlot[0] (Body/Feet/Legs/...); INVALID если нет/нет слота.
	static int GetClothingSlotId(PlayerBase pawn, ItemBase item)
	{
		if (!pawn || !item)
			return InventorySlots.INVALID;

		array<string> names = new array<string>();
		item.ConfigGetTextArray("inventorySlot", names);
		if (names.Count() == 0)
			return InventorySlots.INVALID;

		int slotId = InventorySlots.GetSlotIdFromString(names[0]);
		if (slotId == InventorySlots.INVALID)
			return InventorySlots.INVALID;

		GameInventory inv = pawn.GetInventory();
		if (!inv || !inv.HasAttachmentSlot(slotId))
			return InventorySlots.INVALID;

		return slotId;
	}

	//! Свободный ПОДХОДЯЩИЙ слот для предмета: WEAPON → SHOULDER; MELEE → MELEE, затем
	//! SHOULDER; CLOTHING → слот из inventorySlot[]; прочие → false. Возвращает true и
	//! заполняет slotId только если слот свободен и подходит.
	static bool FindAttachmentSlot(PlayerBase pawn, ItemBase item, out int slotId)
	{
		slotId = InventorySlots.INVALID;
		if (!pawn || !item)
			return false;
		GameInventory inv = pawn.GetInventory();
		if (!inv)
			return false;

		dmLootCategory cat = GetCategory(item);
		#ifdef DM_BOT_DEBUG_LOOTING
		string catLabel = "прочее";
		if (cat == dmLootCategory.WEAPON)
			catLabel = "оружие";
		else if (cat == dmLootCategory.MELEE)
			catLabel = "мили";
		else if (cat == dmLootCategory.CLOTHING)
			catLabel = "одежда";
		dmBotLog.Debug("[Loot] FindAttachmentSlot: тип=" + item.GetType() + " " + catLabel);
		#endif

		if (cat == dmLootCategory.WEAPON)
		{
			if (inv.CanAddAttachmentEx(item, InventorySlots.SHOULDER) && !inv.FindAttachment(InventorySlots.SHOULDER))
			{
				slotId = InventorySlots.SHOULDER;
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] FindAttachmentSlot: слот " + InventorySlots.GetSlotName(slotId) + " свободен для " + item.GetType());
				#endif
				return true;
			}
			return false;
		}
		if (cat == dmLootCategory.MELEE)
		{
			if (inv.CanAddAttachmentEx(item, InventorySlots.MELEE) && !inv.FindAttachment(InventorySlots.MELEE))
			{
				slotId = InventorySlots.MELEE;
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] FindAttachmentSlot: слот " + InventorySlots.GetSlotName(slotId) + " свободен для " + item.GetType());
				#endif
				return true;
			}
			if (inv.CanAddAttachmentEx(item, InventorySlots.SHOULDER) && !inv.FindAttachment(InventorySlots.SHOULDER))
			{
				slotId = InventorySlots.SHOULDER;
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] FindAttachmentSlot: слот " + InventorySlots.GetSlotName(slotId) + " свободен для " + item.GetType());
				#endif
				return true;
			}
			return false;
		}
		if (cat == dmLootCategory.CLOTHING)
		{
			array<string> names = new array<string>();
			item.ConfigGetTextArray("inventorySlot", names);
			if (names.Count() == 0)
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] FindAttachmentSlot: " + item.GetType() + " пустой inventorySlot");
				#endif
				return false;
			}
			slotId = InventorySlots.GetSlotIdFromString(names[0]);
			if (slotId == InventorySlots.INVALID)
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] FindAttachmentSlot: " + item.GetType() + " нет слота");
				#endif
				return false;
			}
			if (!inv.HasAttachmentSlot(slotId))
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] FindAttachmentSlot: " + item.GetType() + " нет слота");
				#endif
				return false;
			}
			if (!inv.FindAttachment(slotId))
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] FindAttachmentSlot: слот " + InventorySlots.GetSlotName(slotId) + " свободен для " + item.GetType());
				#endif
				return true;
			}
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] FindAttachmentSlot: " + item.GetType() + " слот занят");
			#endif
			return false;
		}
		return false;
	}

	//! Свободное место в карго ВСЕГО одетого инвентаря (не только рюкзак): перебрать
	//! m_AttachmentSlots, для каждого занятого слота искать свободное карго.
	static bool FindCargo(PlayerBase pawn, EntityAI item, out InventoryLocation dst)
	{
		if (!pawn || !item)
			return false;
		GameInventory inv = pawn.GetInventory();
		if (!inv)
			return false;

		int i;
		for (i = 0; i < m_AttachmentSlots.Count(); i++)
		{
			EntityAI att = inv.FindAttachment(m_AttachmentSlots[i]);
			if (!att)
				continue;
			if (att.GetInventory().FindFreeLocationFor(item, FindInventoryLocationType.CARGO, dst))
				return true;
		}
		return false;
	}

	//! Куда положить предмет (категорийно): аттачмент-слот (оружие/мили/одежда) →
	//! карго всего одетого инвентаря. Каждая true-ветка заполняет dst.
	static bool FindDestination(PlayerBase pawn, EntityAI item, out InventoryLocation dst)
	{
		if (!pawn || !item)
			return false;

		ItemBase ib = ItemBase.Cast(item);
		if (ib)
		{
			int slotId;
			if (FindAttachmentSlot(pawn, ib, slotId))
			{
				dst.SetAttachment(pawn, item, slotId);
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] FindDestination: слот " + InventorySlots.GetSlotName(slotId) + " для " + item.GetType());
				#endif
				return true;
			}
		}

		if (FindCargo(pawn, item, dst))
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] FindDestination: карго для " + item.GetType());
			#endif
			return true;
		}

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] FindDestination: нет места для " + item.GetType());
		#endif
		return false;
	}

	//! Может ли бот реально разместить предмет (без самого переноса).
	static bool CanCarry(PlayerBase pawn, ItemBase item)
	{
		InventoryLocation dst = new InventoryLocation();
		if ( pawn.GetInventory() && FindDestination(pawn, item, dst) )
		{
			if ( pawn.GetInventory().LocationCanAddEntity(dst) ) return true;
			dst.Reset();
		}
		return false;
	}

	//! Надеть item в руки. Обёртка RemoteObjectTreeDelete/Create обязательна для синка
	//! клиенту (иначе «призрак»: клиент держит старую картину — предмет на земле/надетый).
	//! HideItemInHands(true/false) — отдельный трюк анимации руки, к сетевому синку отношения не имеет.
	static bool TakeToHands(PlayerBase pawn, ItemBase item)
	{
		if (!pawn || !item)
			return false;
		InventoryLocation src = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(src))
			return false;

		InventoryLocation dst = new InventoryLocation();
		dst.SetHands(pawn, item);

		GetGame().RemoteObjectTreeDelete(item);
		bool ok = pawn.LocalTakeToDst(src, dst);
		pawn.GetItemAccessor().HideItemInHands(true);
		pawn.GetItemAccessor().HideItemInHands(false);
		GetGame().RemoteObjectTreeCreate(item);
		return ok;
	}

	//! Перенести item в карго контейнера `to`; при to == null — в любое свободное место
	//! через FindDestination (аттачмент → карго). Обёртка RemoteObjectTreeDelete/Create
	//! обязательна для синка клиенту (иначе «призрак» — клиент держит старую картину).
	static bool TakeIntoCargo(PlayerBase pawn, ItemBase item, EntityAI to = null)
	{
		if (!pawn || !item)
			return false;
		InventoryLocation src = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(src))
			return false;

		InventoryLocation dst = new InventoryLocation();
		if (to)
		{
			if (!to.GetInventory().FindFreeLocationFor(item, FindInventoryLocationType.CARGO, dst))
				return false;
		}
		else
		{
			if (!FindDestination(pawn, item, dst))
				return false;
		}

		GetGame().RemoteObjectTreeDelete(item);
		bool ok = pawn.LocalTakeToDst(src, dst);
		GetGame().RemoteObjectTreeCreate(item);
		return ok;
	}

	//! Надеть item СТРОГО в слот slotId (dst.SetAttachment(pawn, item, slotId)). При неудаче
	//! возвращает false (предмет остаётся на земле). Обёртка RemoteObjectTreeDelete/Create
	//! обязательна для синка клиенту (иначе «призрак» — клиент держит старую картину).
	//! SetAttachment — аналог dst.SetHands(pawn, item) в TakeToHands.
	static bool TakeToAttachmentSlot(PlayerBase pawn, ItemBase item, int slotId)
	{
		if (!pawn || !item)
			return false;
		InventoryLocation src = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(src))
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] TakeToAttachmentSlot: нет InventoryLocation у " + item.GetType());
			#endif
			return false;
		}

		InventoryLocation dst = new InventoryLocation();
		dst.SetAttachment(pawn, item, slotId);

		GetGame().RemoteObjectTreeDelete(item);
		bool ok = pawn.LocalTakeToDst(src, dst);
		GetGame().RemoteObjectTreeCreate(item);
		return ok;
	}

	//! Перенести item прямо в уже заполненное назначение dst. Обёртка RemoteObjectTreeDelete/Create
	//! обязательна для синка клиенту (иначе «призрак» — клиент держит старую картину).
	static bool TakeIntoDestination(PlayerBase pawn, ItemBase item, InventoryLocation dst)
	{
		if (!pawn || !item || !dst)
			return false;
		InventoryLocation src = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(src))
			return false;

		GetGame().RemoteObjectTreeDelete(item);
		bool ok = pawn.LocalTakeToDst(src, dst);
		GetGame().RemoteObjectTreeCreate(item);
		return ok;
	}

	//! Освободить руки: переместить предмет из рук в первое свободное место
	//! (MELEE → SHOULDER → Backpack_1 рюкзака). false — рука пуста либо места нет.
	//! Мув ТОЙ ЖЕ сущности (НЕ clone+delete): RemoteObjectTreeDelete → LocalTakeToDst
	//! → отложенный RemoteObjectTreeCreate (CallLater). Из HANDS LocalTakeToDst уходит
	//! в deferred HandEvent (мув следующим тиком, item-move-sync.md), поэтому create
	//! откладывается — синхронный create пере-зарегистрировал бы предмет на прежнем
	//! месте (HANDS). При неудаче мува create делается сразу (вернуть предмет клиенту).
	static bool FreeHands(PlayerBase pawn)
	{
		if (!pawn)
			return false;

		EntityAI item = pawn.GetItemInHands();
		if (!item)
			return false;

		GameInventory inv = pawn.GetInventory();
		if (!inv)
			return false;

		InventoryLocation dst = new InventoryLocation();

		int slotId = InventorySlots.INVALID;
		if (inv.CanAddAttachmentEx(item, InventorySlots.MELEE) && !inv.FindAttachment(InventorySlots.MELEE))
			slotId = InventorySlots.MELEE;
		else if (inv.CanAddAttachmentEx(item, InventorySlots.SHOULDER) && !inv.FindAttachment(InventorySlots.SHOULDER))
			slotId = InventorySlots.SHOULDER;

		if (slotId != InventorySlots.INVALID)
		{
			dst.SetAttachment(pawn, item, slotId);
			return TakeFromHands(pawn, item, dst);
		}

		EntityAI bag = inv.FindAttachment(InventorySlots.BACK);
		if (bag)
		{
			int backSlotId = InventorySlots.GetSlotIdFromString("Backpack_1");
			if (bag.GetInventory().CanAddAttachmentEx(item, backSlotId) && !bag.GetInventory().FindAttachment(backSlotId))
			{
				dst.SetAttachment(bag, item, backSlotId);
				return TakeFromHands(pawn, item, dst);
			}
		}

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] FreeHands: нет свободного места для " + item.GetType());
		#endif
		return false;
	}

	//! Мув ТОЙ ЖЕ сущности из рук в слот dst: RemoteObjectTreeDelete → LocalTakeToDst
	//! → отложенный RemoteObjectTreeCreate. Из HANDS LocalTakeToDst уходит в deferred
	//! HandEvent, поэтому create откладывается CallLater (следующий тик, после
	//! фактического перемещения); при неудаче мува create делается сразу, не оставляя
	//! предмет unregistered на клиенте.
	private static bool TakeFromHands(PlayerBase pawn, EntityAI item, InventoryLocation dst)
	{
		InventoryLocation src = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(src))
			return false;

		GetGame().RemoteObjectTreeDelete(item);
		bool ok = pawn.LocalTakeToDst(src, dst);
		if (ok)
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ReCreateTree, 0, false, item);
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] FreeHands: " + item.GetType() + " в слоте (deferred create)");
			#endif
		}
		else
		{
			GetGame().RemoteObjectTreeCreate(item);
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] FreeHands: мув не удался для " + item.GetType() + " (create сразу)");
			#endif
		}
		return ok;
	}

	//! Отложенный RemoteObjectTreeCreate (deferred_delete_create, item-move-sync.md):
	//! пере-регистрирует сетевое дерево предмета на клиенте уже на НОВОМ месте.
	private static void ReCreateTree(EntityAI item)
	{
		if (item && !item.IsSetForDeletion())
			GetGame().RemoteObjectTreeCreate(item);
	}

	//! -------------------------------------------------------------------------
	//! Рейтинг оружия — БОЕВАЯ ЦЕННОСТЬ. Ось, ОТЛИЧНАЯ от dmRequirements.GetBaseIndex
	//! (та — «необходимость категории» для порядка выброса при переполнении:
	//! GetDiscardOrder/MaybeDrop). RateWeapon отвечает «какое оружие ценнее в бою»
	//! для конкуренции SHOULDER/MELEE: мили = 0.1 × здоровье, огнестрел = патроны ×
	//! (initSpeed / 1000) × здоровье. Эти две системы НЕ смешиваются: GetBaseIndex
	//! никогда не участвует в конкуренции оружия, а RateWeapon — в порядке выброса.
	//! -------------------------------------------------------------------------

	//! Кэш «пуля → патроны» на время ОДНОЙ оценки (pickUpItemSelect → EnsureCarry),
	//! чтобы EnumerateInventory по одному калибру не повторялся. Очищается в начале
	//! оценки через ClearWeaponRatingCache().
	static string s_LastBulletType = "";
	static float s_LastAmmoCount = 0.0;

	static void ClearWeaponRatingCache()
	{
		s_LastBulletType = "";
		s_LastAmmoCount = 0.0;
	}

	//! Рейтинг оружия (выше = нужнее в бою): ruined → -1 (выбросить немедленно),
	//! мили → 0.1 × здоровье, огнестрел (не пистолет) → ammoCount × (initSpeed / 1000) ×
	//! здоровье, пистолет → 0 (не участвует в конкуренции SHOULDER/MELEE в Фазе 1).
	//! ammoCount — патроны подходящего калибра в инвентаре пешки; initSpeed — из CfgAmmo.
	static float RateWeapon(ItemBase item, dmAISurvivor bot)
	{
		if (!item)
			return -1.0;

		float hp = item.GetHealth01();
		if (item.IsDamageDestroyed() || hp <= 0.0)
			return -1.0;

		float rate;

		if (IsMelee(item))
		{
			rate = DM_LOOT_MELEE_RATE_MULT * hp;
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] rate " + item.GetType() + " = " + rate + " hp=" + hp);
			#endif
			return rate;
		}

		if (item.IsInherited(Pistol_Base))
		{
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] rate " + item.GetType() + " = 0.0 (пистолет)");
			#endif
			return 0.0;
		}

		if (item.IsInherited(Weapon_Base))
		{
			Weapon_Base weapon = Weapon_Base.Cast(item);
			if (!weapon)
				return 0.0;

			string bulletType;
			if (!GetFirearmBulletType(weapon, bulletType))
				return 0.0;

			float ammo = GetFirearmAmmoCount(bot, bulletType);
			float speed = GetFirearmInitSpeed(bulletType);
			rate = ammo * (speed / DM_LOOT_FIREARM_SPEED_DIV) * hp;
			#ifdef DM_BOT_DEBUG_LOOTING
			dmBotLog.Debug("[Loot] rate " + item.GetType() + " = " + rate);
			dmBotLog.Debug("[Loot] rate: ammo=" + ammo + " speed=" + speed + " hp=" + hp);
			#endif
			return rate;
		}

		return 0.0;
	}

	//! Слабейший предмет списка по RateWeapon (боевая ценность). null — список пуст.
	static EntityAI FindWeakest(array<EntityAI> items, dmAISurvivor bot)
	{
		if (!items || items.Count() == 0)
			return null;

		EntityAI weakest = items[0];
		float weakestRate = RateWeapon(ItemBase.Cast(weakest), bot);
		int i;
		float r;
		for (i = 1; i < items.Count(); i++)
		{
			r = RateWeapon(ItemBase.Cast(items[i]), bot);
			if (r < weakestRate)
			{
				weakestRate = r;
				weakest = items[i];
			}
		}
		return weakest;
	}

	//! Патронов подходящего калибра в инвентаре пешки (магазины + россыпные пачки).
	//! Калибр — по разрешённому имени пули (tracer/обычный — один initSpeed).
	//! Magazine — typedef Magazine_Base, поэтому Magazine.Cast покрывает И отъёмный
	//! Magazine, И Ammunition_Base (россыпные пачки). Результат кэшируется на время оценки.
	private static float GetFirearmAmmoCount(dmAISurvivor bot, string bulletType)
	{
		if (!bot || bulletType == "")
			return 0.0;

		if (bulletType == s_LastBulletType)
			return s_LastAmmoCount;

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return 0.0;

		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int count = 0;
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			Magazine mag = Magazine.Cast(items[i]);
			if (!mag)
				continue;
			string itemBullet = dmWeaponRange.GetBulletFromMagazine(mag.GetType());
			if (itemBullet == "")
				continue;
			if (itemBullet == bulletType)
				count = count + mag.GetAmmoCount();
		}

		s_LastBulletType = bulletType;
		s_LastAmmoCount = (float)count;
		return s_LastAmmoCount;
	}

	//! Начальная скорость пули (initSpeed из CfgAmmo). Фолбэк — DM_AI_DEFAULT_INIT_SPEED.
	private static float GetFirearmInitSpeed(string bulletType)
	{
		float speed = g_Game.ConfigGetFloat("CfgAmmo " + bulletType + " initSpeed");
		if (speed <= 0.0)
			return DM_AI_DEFAULT_INIT_SPEED;
		return speed;
	}

	//! Тип пули оружия: chamberableFrom[0] (имя CfgMagazines, детерминированно) →
	//! имя пули CfgAmmo через dmWeaponRange.GetBulletFromMagazine (единый резолв).
	private static bool GetFirearmBulletType(Weapon_Base weapon, out string bulletType)
	{
		bulletType = "";
		if (!weapon)
			return false;
		array<string> chamberable = new array<string>();
		weapon.ConfigGetTextArray("chamberableFrom", chamberable);
		if (chamberable.Count() == 0)
			return false;
		bulletType = dmWeaponRange.GetBulletFromMagazine(chamberable[0]);
		return bulletType != "";
	}
}
