//! dmLoot — статичная классификация предметов (предмет → категория лута).
//!
//! Слой 1 будущего лута: по ItemBase возвращает dmLootCategory через ванильное
//! класс-наследование и виртуальные методы Object (аналог Expansion
//! eAIItemTargetInformation.CalculateThreat). Дальше категории питают
//! dmWishlist/dmRequirements/dmNeeds.

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

	//! Куда положить предмет (слот-зависимо): одежда → свободный слот из
	//! inventorySlot[]; мили → SHOULDER/MELEE, затем руки; оружие → руки;
	//! остальное → карго. Возвращает true и заполняет dst.
	static bool FindDestination(PlayerBase pawn, EntityAI item, out InventoryLocation dst)
	{
		if (!pawn || !item) return false;
		GameInventory inv = pawn.GetInventory();
		if (!inv)
			return false;

        foreach(int slot: m_AttachmentSlots)
        {
            EntityAI attachment = inv.FindAttachment(slot);
            if ( !attachment )
			{
				// Attachment candidate?
				if ( inv.CanAddAttachmentEx(item, slot) )
				{
					dst.SetAttachment(pawn, item, slot);
					#ifdef DM_BOT_DEBUG_LOOTING
					dmBotLog.Debug("[Loot] Have empty slot " + InventorySlots.GetSlotName( slot ) + " for item");
					#endif
					return true;
				}
				continue;
			}
			GameInventory attInv = attachment.GetInventory();
			if (attInv.FindFreeLocationFor(item, FindInventoryLocationType.CARGO | FindInventoryLocationType.ATTACHMENT, dst))
			{
				#ifdef DM_BOT_DEBUG_LOOTING
				dmBotLog.Debug("[Loot] Got empty space " + dst.DumpToString());
				#endif
				return true;
			}
        }
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
}
