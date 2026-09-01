//! dmWishlist — реестр желаний/отвращений + скорер «хочу ли я эту вещь».
//!
//! Чистый домен (слой 1 лута): хранит желания (класс + необходимость), отвращения
//! (junk) и игнор конкретных вещей (с таймаутом). CalcDesired(item) возвращает
//! 0..1. Динамические нужды (голод/нет оружия/повреждено) выставляет dmNeeds
//! (Фаза 3) через SetDesired — здесь их нет. Ссылок на dmRequirements/dmNeeds/
//! dmAISurvivor нет: этот класс никого не пишет, только отвечает на вопросы.

//! Одно желание/отвращение: класс (с наследованием) + уровень необходимости.
class dmDesire
{
	typename m_Class;
	float m_Necessity;   // 0..1 (для junk не используется)
};

//! Игнорируемая вещь (не подбирать снова): сущность + время истечения игнора.
class dmIgnoredItem
{
	EntityAI m_Item;
	float m_ExpireTime;  // GetGame().GetTickTime() + DM_LOOT_IGNORE_TIMEOUT
};

class dmWishlist
{
	ref array<ref dmDesire> m_Desired;
	ref array<ref dmDesire> m_Junk;
	ref array<ref dmIgnoredItem> m_Ignored;

	void dmWishlist()
	{
		m_Desired = new array<ref dmDesire>();
		m_Junk = new array<ref dmDesire>();
		m_Ignored = new array<ref dmIgnoredItem>();
	}

	//! Задать/обновить желание класса (наследуется: Edible_Base покрывает всю еду).
	void SetDesired(typename itemClass, float necessity)
	{
		int i;
		for (i = 0; i < m_Desired.Count(); i++)
		{
			if (m_Desired[i].m_Class == itemClass)
			{
				m_Desired[i].m_Necessity = necessity;
				return;
			}
		}

		dmDesire desire = new dmDesire();
		desire.m_Class = itemClass;
		desire.m_Necessity = necessity;
		m_Desired.Insert(desire);
	}

	void RemoveDesired(typename itemClass)
	{
		int i;
		for (i = m_Desired.Count() - 1; i >= 0; i--)
		{
			if (m_Desired[i].m_Class == itemClass)
				m_Desired.Remove(i);
		}
	}

	void SetJunk(typename itemClass)
	{
		int i;
		for (i = 0; i < m_Junk.Count(); i++)
		{
			if (m_Junk[i].m_Class == itemClass)
				return;
		}

		dmDesire junk = new dmDesire();
		junk.m_Class = itemClass;
		junk.m_Necessity = 0.0;
		m_Junk.Insert(junk);
	}

	void RemoveJunk(typename itemClass)
	{
		int i;
		for (i = m_Junk.Count() - 1; i >= 0; i--)
		{
			if (m_Junk[i].m_Class == itemClass)
				m_Junk.Remove(i);
		}
	}

	//! Игнорировать конкретную вещь (например выброшенную) на DM_LOOT_IGNORE_TIMEOUT секунд.
	void Ignore(EntityAI item)
	{
		if (!item)
			return;

		float expire = GetGame().GetTickTime() + DM_LOOT_IGNORE_TIMEOUT;

		int i;
		for (i = 0; i < m_Ignored.Count(); i++)
		{
			if (m_Ignored[i].m_Item == item)
			{
				m_Ignored[i].m_ExpireTime = expire;
				return;
			}
		}

		dmIgnoredItem ignored = new dmIgnoredItem();
		ignored.m_Item = item;
		ignored.m_ExpireTime = expire;
		m_Ignored.Insert(ignored);
	}

	void Unignore(EntityAI item)
	{
		if (!item)
			return;

		int i;
		for (i = m_Ignored.Count() - 1; i >= 0; i--)
		{
			if (m_Ignored[i].m_Item == item)
				m_Ignored.Remove(i);
		}
	}

	//! Скорер: 0..1, насколько бот хочет эту вещь.
	float CalcDesired(ItemBase item)
	{
		if (!item)
			return 0.0;

		float now = GetGame().GetTickTime();

		int i;

		// 1) ленивая чистка истёкших игноров
		for (i = m_Ignored.Count() - 1; i >= 0; i--)
		{
			if (m_Ignored[i].m_ExpireTime <= now)
				m_Ignored.Remove(i);
		}

		// 2) игнор конкретной вещи → 0
		for (i = 0; i < m_Ignored.Count(); i++)
		{
			if (m_Ignored[i].m_Item == item)
				return 0.0;
		}

		// 3) junk: наследование класса → 0
		for (i = 0; i < m_Junk.Count(); i++)
		{
			if (item.IsInherited(m_Junk[i].m_Class))
				return 0.0;
		}

		// 4) desired: наследование класса → необходимость
		for (i = 0; i < m_Desired.Count(); i++)
		{
			if (item.IsInherited(m_Desired[i].m_Class))
				return m_Desired[i].m_Necessity;
		}

		// 5) fallback — базовое желание по категории
		dmLootCategory category = dmLoot.GetCategory(item);
		if (category == dmLootCategory.FOOD)
			return 0.5;
		if (category == dmLootCategory.WEAPON)
			return 0.8;
		if (category == dmLootCategory.MAGAZINE)
			return 0.6;
		if (category == dmLootCategory.AMMO)
			return 0.5;
		if (category == dmLootCategory.CLOTHING)
			return 0.3;
		if (category == dmLootCategory.REPAIR)
			return 0.5;
		if (category == dmLootCategory.MEDICAL)
			return 0.5;
		return 0.0;
	}
}
