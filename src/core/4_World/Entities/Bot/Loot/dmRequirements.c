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
	int m_TotalCapacity;                   // суммарная ёмкость всех cargo (ячеек)

	void dmRequirements()
	{
		m_Items = new array<ref dmItemIndex>();
		m_Ruined = new array<EntityAI>();
		m_UsedRatio = 0.0;
		m_TotalCapacity = 0;
	}

	//! Индекс НЕОБХОДИМОСТИ категории — для порядка выброса при переполнении
	//! (GetDiscardOrder/MaybeDrop): выше = нужнее, ниже = выбросить первым.
	//! Это НЕ боевая ценность оружия — её считает dmLoot.RateWeapon (отдельная ось для
	//! конкуренции SHOULDER/MELEE). Две системы НЕ смешиваются.
	//! FOOD=1, WEAPON=0.9, MELEE=1, MAGAZINE=1, AMMO=1, CLOTHING=0.5, REPAIR=0.7, MEDICAL=1, OTHER=0.5.
	float GetBaseIndex(dmLootCategory category)
	{
		if (category == dmLootCategory.FOOD)
			return 1.0;
		if (category == dmLootCategory.WEAPON)
			return 0.9;
		if (category == dmLootCategory.MELEE)
			return 1.0;
		if (category == dmLootCategory.MAGAZINE)
			return 1.0;
		if (category == dmLootCategory.AMMO)
			return 1.0;
		if (category == dmLootCategory.CLOTHING)
			return 0.5;
		if (category == dmLootCategory.REPAIR)
			return 0.7;
		if (category == dmLootCategory.MEDICAL)
			return 1.0;
		return 0.5;
	}

	//! Decay (множитель ранга дубликатов) по группе. Патрон/магазин под своё
	//! оружие держится дороже чужого калибра; еда/вода и ремонт — со своим decay;
	//! прочее (weapon/melee/clothing/medical/other) — по умолчанию.
	private float GetDecay(dmLootCategory category, ItemBase item, dmAISurvivor bot)
	{
		if (category == dmLootCategory.AMMO || category == dmLootCategory.MAGAZINE)
		{
			if (IsMatchingCaliber(item, bot))
				return DM_LOOT_DECAY_AMMO_MATCH;
			return DM_LOOT_DECAY_AMMO_OTHER;
		}
		if (category == dmLootCategory.FOOD)
			return DM_LOOT_DECAY_FOOD;
		if (category == dmLootCategory.REPAIR)
			return DM_LOOT_DECAY_REPAIR;
		return DM_LOOT_DECAY_DEFAULT;
	}

	//! Калибр патрона/магазина соответствует одному из зарегистрированных стволов.
	private bool IsMatchingCaliber(ItemBase item, dmAISurvivor bot)
	{
		if (!item || !bot)
			return false;
		string bullet = dmWeaponRange.GetBulletFromMagazine(item.GetType());
		if (bullet == "")
			return false;
		return bot.GetArsenal().HasWeaponForCaliber(bullet);
	}

	//! Анализ инвентаря. Тик ~5с (вызывает dmNeeds/состояние).
	void Update(dmAISurvivor bot)
	{
		m_Items.Clear();
		m_Ruined.Clear();
		m_UsedRatio = 0.0;
		m_TotalCapacity = 0;

		if (!bot)
			return;

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return;

		array<EntityAI> items = dmLoot.CollectCargoRecursive(pawn);

		SortItemsByQuality(items);

		int foodCount = 0;
		int weaponCount = 0;
		int meleeCount = 0;
		int magMatchCount = 0;
		int magOtherCount = 0;
		int ammoMatchCount = 0;
		int ammoOtherCount = 0;
		int clothCount = 0;
		int repairCount = 0;
		int medicalCount = 0;
		int otherCount = 0;

		int i;
		int k;
		ItemBase item;
		dmLootCategory category;
		float baseIndex;
		float decay;
		float factor;
		float index;
		int n;
		bool isMatch;
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
			decay = GetDecay(category, item, bot);

			isMatch = false;
			if (category == dmLootCategory.AMMO || category == dmLootCategory.MAGAZINE)
				isMatch = IsMatchingCaliber(item, bot);

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
			else if (category == dmLootCategory.MELEE)
			{
				meleeCount = meleeCount + 1;
				n = meleeCount;
			}
			else if (category == dmLootCategory.MAGAZINE)
			{
				if (isMatch)
				{
					magMatchCount = magMatchCount + 1;
					n = magMatchCount;
				}
				else
				{
					magOtherCount = magOtherCount + 1;
					n = magOtherCount;
				}
			}
			else if (category == dmLootCategory.AMMO)
			{
				if (isMatch)
				{
					ammoMatchCount = ammoMatchCount + 1;
					n = ammoMatchCount;
				}
				else
				{
					ammoOtherCount = ammoOtherCount + 1;
					n = ammoOtherCount;
				}
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

			factor = 1.0;
			for (k = 1; k < n; k++)
				factor = factor * decay;

			index = baseIndex * factor;

			entry = new dmItemIndex();
			entry.m_Item = item;
			entry.m_Index = index;
			m_Items.Insert(entry);
		}

		m_foodCount = foodCount;
		m_weaponCount = weaponCount;
		m_meleeCount = meleeCount;
		m_medicalCount = medicalCount;

		SortByIndex();
		ComputeUsedRatio(pawn);

		#ifdef DM_BOT_DEBUG_LOOTING
		dmBotLog.Debug("[Loot] Requirements: items=" + m_Items.Count() + " ruined=" + m_Ruined.Count() + " used=" + m_UsedRatio + " cap=" + m_TotalCapacity);
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

	//! Debug: «тип=индекс» для каждого предмета в порядке GetDiscardOrder (по
	//! возрастанию индекса). Baseline-характеризация ранжирования.
	array<string> DebugRankDump()
	{
		array<string> lines = new array<string>();
		int i;
		for (i = 0; i < m_Items.Count(); i++)
		{
			if (!m_Items[i])
				continue;
			lines.Insert(m_Items[i].m_Item.GetType() + "=" + m_Items[i].m_Index);
		}
		return lines;
	}

	//! Стоит ли эвакуироваться ради предмета с desire: желание выше индекса наименее
	//! ценного предмета (иначе выбросили бы более нужное ради менее нужного). Без
	//! предметов в инвентаре эвакуировать нечего.
	bool WorthEvacuating(float desire)
	{
		if (m_Items.Count() == 0)
			return false;
		return desire > m_Items[0].m_Index;
	}

	//! Все мили-предметы на персонаже (слоты SHOULDER/MELEE/руки/карго) — живая
	//! перебор инвентаря пешки (не кэш m_Items).
	array<EntityAI> GetAllMelee(dmAISurvivor bot)
	{
		array<EntityAI> result = new array<EntityAI>();
		if (!bot)
			return result;
		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
			return result;

		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);

		int i;
		for (i = 0; i < items.Count(); i++)
		{
			if (dmLoot.IsMelee(items[i]))
				result.Insert(items[i]);
		}
		return result;
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
			if (m_Items[i].m_Item.IsInherited(ToolBase) && dmLoot.IsMelee(m_Items[i].m_Item))
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

	//! Сортировка items по убыванию качества (лучшая вещь типа получает n=1,
	//! т.е. выше индекс). Тот же selection sort, что SortByIndex, но критерий —
	//! Quality(item) = GetQuantityNormalized() + GetHealth01(). Строит новый массив
	//! (без перекладывания ссылок во временную переменную), затем копирует обратно.
	private void SortItemsByQuality(array<EntityAI> items)
	{
		ref array<EntityAI> sorted = new array<EntityAI>();
		int bestIdx;
		int i;
		while (items.Count() > 0)
		{
			bestIdx = 0;
			for (i = 1; i < items.Count(); i++)
			{
				if (Quality(items[i]) > Quality(items[bestIdx]))
					bestIdx = i;
			}
			sorted.Insert(items[bestIdx]);
			items.Remove(bestIdx);
		}
		for (i = 0; i < sorted.Count(); i++)
			items.Insert(sorted[i]);
	}

	//! Качество предмета (0..2): нормализованная доля содержимого (полная бутылка = 1,
	//! пустая = 0) + нормированное здоровье (целый = 1, битый = 0). Выше = нужнее.
	private float Quality(EntityAI item)
	{
		ItemBase ib = ItemBase.Cast(item);
		if (!ib)
			return 0.0;
		return ib.GetQuantityNormalized() + ib.GetHealth01();
	}

	//! Занятая доля ВСЕХ cargo пешки (главный + надетые вещи): capacity =
	//! width*height ячеек, занято = сумма размеров предметов (GetItemSize).
	//! Нативного GetFreeCapacity в GameInventory/CargoBase нет (см. ванильный
	//! GUI: GetCargoCapacity = сумма GetItemSize, GetMaxCargoCapacity =
	//! GetWidth()*GetHeight()).
	private void ComputeUsedRatio(PlayerBase pawn)
	{
		GameInventory inv = pawn.GetInventory();
		if (!inv)
			return;

		int total = 0;
		int used = 0;

		// Главный cargo (сам инвентарь).
		AccumulateCargo(inv.GetCargo(), total, used);

		// Cargo всех надетых вещей (рюкзак, штаны, куртка, жилет, ...).
		int i;
		int slotCount = inv.GetAttachmentSlotsCount();
		for (i = 0; i < slotCount; i++)
		{
			EntityAI att = inv.FindAttachment(inv.GetAttachmentSlotId(i));
			if (att)
				AccumulateCargo(att.GetInventory().GetCargo(), total, used);
		}

		m_TotalCapacity = total;
		if (total <= 0)
			return;
		m_UsedRatio = (float)used / (float)total;
	}

	//! Прибавить ёмкость и занятые ячейки одного cargo (null → ничего).
	private void AccumulateCargo(CargoBase cargo, out int total, out int used)
	{
		if (!cargo)
			return;
		total = total + cargo.GetWidth() * cargo.GetHeight();
		int i;
		int w;
		int h;
		for (i = 0; i < cargo.GetItemCount(); i++)
		{
			if (cargo.GetItemSize(i, w, h))
				used = used + w * h;
		}
	}

	int m_foodCount = 0;
	int m_weaponCount = 0;
	int m_meleeCount = 0;
	int m_medicalCount = 0;

	int CheckFoodCount()
	{
		return m_foodCount;
	}
	
	int CheckWeaponCount()
	{
		return m_weaponCount;
	}

	int CheckMeleeCount()
	{
		return m_meleeCount;
	}

	int CheckMedicalCount()
	{
		return m_medicalCount;
	}
}
