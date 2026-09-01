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
}
