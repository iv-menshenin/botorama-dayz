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

class dmLoot
{
	//! Категория лута для предмета (ванильное наследование). null → OTHER.
	static dmLootCategory GetCategory(ItemBase item)
	{
		if (!item)
			return dmLootCategory.OTHER;

		if (item.IsInherited(Edible_Base))
			return dmLootCategory.FOOD;

		if (item.IsWeapon())
			return dmLootCategory.WEAPON;

		if (item.IsMeleeWeapon())
			return dmLootCategory.MELEE;

		if (item.IsMagazine())
			return dmLootCategory.MAGAZINE;

		if (item.IsAmmoPile())
			return dmLootCategory.AMMO;

		if (item.IsClothing())
			return dmLootCategory.CLOTHING;

		if (item.IsInherited(WeaponCleaningKit) || item.IsInherited(SewingKit) || item.IsInherited(LeatherSewingKit) || item.IsInherited(DuctTape))
			return dmLootCategory.REPAIR;

		if (item.IsInherited(BandageDressing))
			return dmLootCategory.MEDICAL;

		return dmLootCategory.OTHER;
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

	//! Надеть item в руки с ручным ре-синком сети (готча — docs/research/loot.md):
	//! SERVER-перенос у AI-бота не кладёт оружие в руки сам.
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
	//! через FindDestination (аттачмент → карго). Ручной ре-синк сети (готча — research/loot.md).
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

	//! Надеть item СТРОГО в слот slotId (dst.SetAttachment(pawn, item, slotId)) с ручным
	//! ре-синком сети: item приходит с земли, а SERVER-перенос у AI-бота не синкается
	//! сам (готча — docs/research/loot.md). При неудаче возвращает false (предмет
	//! остаётся на земле). SetAttachment — аналог dst.SetHands(pawn, item) в TakeToHands.
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

	//! Перенести item прямо в уже заполненное назначение dst с ручным ре-синком сети.
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
}
