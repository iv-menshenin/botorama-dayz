//! dmRequirements — анализатор инвентаря (индекс необходимости предметов).
//!
//! Чистый домен (слой 1 лута): тикается раз в ~5с, перебирает инвентарь и
//! проставляет каждому предмету индекс необходимости. Индексы нужны, чтобы при
//! переполнении понять, что выбросить (GetDiscardOrder), а разрушенное собрать в
//! m_Ruined. Динамические нужды (нет оружия → желание) — это dmNeeds (Фаза 3),
//! здесь их нет. Ссылок на dmWishlist/dmNeeds нет: класс только описывает
//! состояние инвентаря и никого не пишет.

//! Предмет + его индекс необходимости (для порядка выброса).
class dmItemIndex
{
	EntityAI m_Item;
	float m_Index;   // выше = нужнее; ниже = выбросить первым
};

class dmRequirements
{
	ref array<ref dmItemIndex> m_Items;    // результат Update (отсортирован по индексу)
	ref array<EntityAI> m_Ruined;          // разрушенные (выбросить сразу)
	float m_UsedRatio;                     // занятая доля ёмкости инвентаря (0..1)

	void dmRequirements()
	{
		m_Items = new array<ref dmItemIndex>();
		m_Ruined = new array<EntityAI>();
		m_UsedRatio = 0.0;
	}

	//! Базовый индекс необходимости по категории (ванильное наследование уже в dmLoot).
	//! FOOD=1, WEAPON=1, MAGAZINE=1, AMMO=1, CLOTHING=0.5, REPAIR=1, MEDICAL=1, OTHER=0.5.
	float GetBaseIndex(dmLootCategory category)
	{
		if (category == dmLootCategory.FOOD)
			return 1.0;
		if (category == dmLootCategory.WEAPON)
			return 1.0;
		if (category == dmLootCategory.MAGAZINE)
			return 1.0;
		if (category == dmLootCategory.AMMO)
			return 1.0;
		if (category == dmLootCategory.CLOTHING)
			return 0.5;
		if (category == dmLootCategory.REPAIR)
			return 1.0;
		if (category == dmLootCategory.MEDICAL)
			return 1.0;
		return 0.5;
	}

	//! Анализ инвентаря. Тик ~5с (вызывает dmNeeds/состояние).
	void Update(dmAISurvivor bot)
	{
		m_Items.Clear();
		m_Ruined.Clear();
		m_UsedRatio = 0.0;

		if (!bot)
			return;

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int foodCount = 0;
		int weaponCount = 0;
		int magCount = 0;
		int ammoCount = 0;
		int clothCount = 0;
		int repairCount = 0;
		int medicalCount = 0;
		int otherCount = 0;

		int i;
		int k;
		ItemBase item;
		dmLootCategory category;
		float baseIndex;
		int n;
		float div;
		float index;
		dmItemIndex entry;

		for (i = 0; i < items.Count(); i++)
		{
			item = ItemBase.Cast(items[i]);
			if (!item)
				continue;

			if (item.IsDamageDestroyed() || item.GetHealth01() <= 0.0)
			{
				m_Ruined.Insert(item);
				continue;
			}

			category = dmLoot.GetCategory(item);
			baseIndex = GetBaseIndex(category);

			n = 1;
			if (category == dmLootCategory.FOOD)
			{
				foodCount = foodCount + 1;
				n = foodCount;
			}
			else if (category == dmLootCategory.WEAPON)
			{
				weaponCount = weaponCount + 1;
				n = weaponCount;
			}
			else if (category == dmLootCategory.MAGAZINE)
			{
				magCount = magCount + 1;
				n = magCount;
			}
			else if (category == dmLootCategory.AMMO)
			{
				ammoCount = ammoCount + 1;
				n = ammoCount;
			}
			else if (category == dmLootCategory.CLOTHING)
			{
				clothCount = clothCount + 1;
				n = clothCount;
			}
			else if (category == dmLootCategory.REPAIR)
			{
				repairCount = repairCount + 1;
				n = repairCount;
			}
			else if (category == dmLootCategory.MEDICAL)
			{
				medicalCount = medicalCount + 1;
				n = medicalCount;
			}
			else
			{
				otherCount = otherCount + 1;
				n = otherCount;
			}

			div = 1.0;
			for (k = 1; k < n; k++)
				div = div * 2.0;

			index = baseIndex / div;

			entry = new dmItemIndex();
			entry.m_Item = item;
			entry.m_Index = index;
			m_Items.Insert(entry);
		}

		SortByIndex();
		ComputeUsedRatio(pawn);

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] Requirements: items=" + m_Items.Count() + " ruined=" + m_Ruined.Count() + " used=" + m_UsedRatio);
		#endif
	}

	//! Порядок выброса: предметы по возрастанию индекса (первый = выбросить первым).
	ref array<EntityAI> GetDiscardOrder()
	{
		ref array<EntityAI> order = new array<EntityAI>();
		int i;
		for (i = 0; i < m_Items.Count(); i++)
			order.Insert(m_Items[i].m_Item);
		return order;
	}

	ref array<EntityAI> GetRuined()
	{
		return m_Ruined;
	}

	//! Есть ли в инвентаре (не разрушенный) предмет заданного класса (наследование).
	bool HasItemInherited(typename itemClass)
	{
		int i;
		for (i = 0; i < m_Items.Count(); i++)
		{
			if (m_Items[i].m_Item.IsInherited(itemClass))
				return true;
		}
		return false;
	}

	//! Есть ли нож (ToolBase + мили-оружие) в инвентаре.
	bool HasKnife()
	{
		int i;
		for (i = 0; i < m_Items.Count(); i++)
		{
			if (m_Items[i].m_Item.IsInherited(ToolBase) && m_Items[i].m_Item.IsMeleeWeapon())
				return true;
		}
		return false;
	}

	//! Есть ли повреждённое (не разрушенное) огнестрельное оружие в инвентаре.
	bool HasDamagedWeapon()
	{
		int i;
		for (i = 0; i < m_Items.Count(); i++)
		{
			ItemBase item = ItemBase.Cast(m_Items[i].m_Item);
			if (item && item.IsInherited(Weapon_Base) && item.GetHealth01() < 1.0)
				return true;
		}
		return false;
	}

	//! Инвентарь почти полон (занято больше DM_LOOT_FULL_THRESHOLD = 0.75 ёмкости,
	//! т.е. свободно < 25%). Опирается на занятую долю, посчитанную в Update.
	bool IsFull()
	{
		return m_UsedRatio > DM_LOOT_FULL_THRESHOLD;
	}

	//! Сортировка m_Items по возрастанию индекса (selection sort; инвентарь мал).
	//! Строит новый массив, вынимая минимальный элемент — без перекладывания
	//! сильных ссылок во временную переменную (иначе предмет можно потерять на
	//! промежуточном шаге свопа).
	private void SortByIndex()
	{
		ref array<ref dmItemIndex> sorted = new array<ref dmItemIndex>();
		int minIdx;
		int i;
		while (m_Items.Count() > 0)
		{
			minIdx = 0;
			for (i = 1; i < m_Items.Count(); i++)
			{
				if (m_Items[i].m_Index < m_Items[minIdx].m_Index)
					minIdx = i;
			}
			sorted.Insert(m_Items[minIdx]);
			m_Items.Remove(minIdx);
		}
		m_Items = sorted;
	}

	//! Занятая доля главного cargo (CargoGrid): capacity = width*height ячеек,
	//! занято = сумма размеров предметов (GetItemSize). Нативного GetFreeCapacity
	//! в GameInventory/CargoBase нет (см. ванильный GUI: GetCargoCapacity =
	//! сумма GetItemSize, GetMaxCargoCapacity = GetWidth()*GetHeight()).
	private void ComputeUsedRatio(PlayerBase pawn)
	{
		CargoBase cargo = pawn.GetInventory().GetCargo();
		if (!cargo)
			return;

		int total = cargo.GetWidth() * cargo.GetHeight();
		if (total <= 0)
			return;

		int used = 0;
		int i;
		int w;
		int h;
		for (i = 0; i < cargo.GetItemCount(); i++)
		{
			if (cargo.GetItemSize(i, w, h))
				used = used + w * h;
		}

		m_UsedRatio = (float)used / (float)total;
	}
}
