//! dmLoot — статичная классификация предметов (предмет → категория лута).
//!
//! Слой 1 будущего лута: по ItemBase возвращает dmLootCategory через ванильное
//! класс-наследование и виртуальные методы Object (аналог Expansion
//! eAIItemTargetInformation.CalculateThreat). Дальше категории питают
//! dmWishlist/dmRequirements/dmNeeds.

enum dmLootCategory
{
	FOOD,      // еда/вода (Edible_Base)
	WEAPON,    // оружие (огнестрел + мили)
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

		if (item.IsWeapon() || item.IsMeleeWeapon())
			return dmLootCategory.WEAPON;

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

	//! Куда положить предмет (слот-зависимо): одежда → свободный слот из
	//! inventorySlot[]; мили → SHOULDER/MELEE, затем руки; оружие → руки;
	//! остальное → карго. Возвращает true и заполняет dst.
	static bool FindDestination(PlayerBase pawn, ItemBase item, out InventoryLocation dst)
	{
		if (!pawn || !item)
			return false;
		GameInventory inv = pawn.GetInventory();
		if (!inv)
			return false;

		if (item.IsClothing())
		{
			array<string> slots = new array<string>();
			item.ConfigGetTextArray("inventorySlot", slots);
			int i;
			for (i = 0; i < slots.Count(); i++)
			{
				int slotId = InventorySlots.GetSlotIdFromString(slots[i]);
				if (!InventorySlots.IsSlotIdValid(slotId))
					continue;
				if (!inv.HasAttachmentSlot(slotId))
					continue;
				if (!inv.FindAttachment(slotId))
				{
					dst = new InventoryLocation();
					dst.SetAttachment(pawn, item, slotId);
					return true;
				}
			}
			return false;
		}

		if (item.IsMeleeWeapon())
		{
			int shoulder = InventorySlots.SHOULDER;
			if (InventorySlots.IsSlotIdValid(shoulder) && inv.HasAttachmentSlot(shoulder) && !inv.FindAttachment(shoulder))
			{
				dst = new InventoryLocation();
				dst.SetAttachment(pawn, item, shoulder);
				return true;
			}
			int melee = InventorySlots.MELEE;
			if (InventorySlots.IsSlotIdValid(melee) && inv.HasAttachmentSlot(melee) && !inv.FindAttachment(melee))
			{
				dst = new InventoryLocation();
				dst.SetAttachment(pawn, item, melee);
				return true;
			}
			if (inv.CanAddEntityIntoHands(item))
			{
				dst = new InventoryLocation();
				dst.SetHands(pawn, item);
				return true;
			}
			return false;
		}

		if (item.IsWeapon())
		{
			if (inv.CanAddEntityIntoHands(item))
			{
				dst = new InventoryLocation();
				dst.SetHands(pawn, item);
				return true;
			}
			return false;
		}

		InventoryLocation loc = new InventoryLocation();
		if (inv.FindFreeLocationFor(item, FindInventoryLocationType.CARGO, loc))
		{
			dst = loc;
			return true;
		}
		return false;
	}

	//! Может ли бот реально разместить предмет (без самого переноса).
	static bool CanCarry(PlayerBase pawn, ItemBase item)
	{
		InventoryLocation dst = new InventoryLocation();
		return FindDestination(pawn, item, dst);
	}
}
