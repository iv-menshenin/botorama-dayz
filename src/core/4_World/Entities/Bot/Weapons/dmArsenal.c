//! dmArsenal — профиль арсенала бота: зарегистрированный огнестрел, мили и
//! патроны по калибру. Заполняется вызовами Register/Unregister (подбор/выброс —
//! подключаются отдельной задачей); фоновый Tick делает стартовый рескан (5 с
//! после спавна) и периодическую полную актуализацию (раз в случайные 7–10 мин)
//! — страховка от дрейфа состояния: выстрел сжигает патроны без Unregister,
//! предмет мог выпасть и т.п.
//!
//! Калибр — класс пули (CfgAmmo <bullet>), резолвится так же, как в
//! dmWeaponRange.GetCaliber: CfgMagazines <тип магазина/пачки> ammo -> имя пули.
//! Счёт патронов — GetAmmoCount() (Magazine — typedef, покрывает и отъёмный
//! магазин, и Ammunition_Base — россыпную пачку).
class dmArsenal
{
	ref dmAISurvivor m_Owner;
	ref array<EntityAI> m_Weapons;        // зарегистрированный огнестрел (Weapon_Base, не мили)
	ref map<string, int> m_AmmoByCaliber; // калибр (класс пули) -> сумма патронов
	ref array<EntityAI> m_Melee;          // зарегистрированное мили

	float m_PeriodicTimer;                 // счётчик до следующей периодической актуализации
	float m_PeriodicInterval;              // текущий случайный интервал (сек)
	ref array<EntityAI> m_RescanQueue;     // вещи для пофреймового ре-регистра (null = нет активного рескана)
	int m_RescanIdx;
	float m_SpawnTimer;
	bool m_InitialRescanDone;

	void dmArsenal(dmAISurvivor owner)
	{
		m_Owner = owner;
		m_Weapons = new array<EntityAI>();
		m_AmmoByCaliber = new map<string, int>();
		m_Melee = new array<EntityAI>();
		m_RescanQueue = null;
		m_RescanIdx = 0;
		m_SpawnTimer = 0.0;
		m_InitialRescanDone = false;
		m_PeriodicTimer = 0.0;
		m_PeriodicInterval = RandomInterval();
	}

	//! Подбор предмета: огнестрел/мили — в списки, магазин/рассыпные патроны — в подсчёт по калибру.
	void Register(EntityAI item)
	{
		if (!item)
			return;
		Weapon_Base w = Weapon_Base.Cast(item);
		if (w && !w.IsMeleeWeapon())
		{
			if (m_Weapons.Find(item) < 0)
			{
				m_Weapons.Insert(item);
				#ifdef DM_BOT_DEBUG_ARSENAL
				dmBotLog.Debug("[Arsenal] Register weapon: " + item.GetType());
				#endif
			}
			return;
		}
		if (dmLoot.IsMelee(item))
		{
			if (m_Melee.Find(item) < 0)
			{
				m_Melee.Insert(item);
				#ifdef DM_BOT_DEBUG_ARSENAL
				dmBotLog.Debug("[Arsenal] Register melee: " + item.GetType());
				#endif
			}
			return;
		}
		if (item.IsMagazine() || item.IsAmmoPile())
		{
			string caliber = GetItemCaliber(item);
			if (caliber == "")
			{
				dmBotLog.Error("[Arsenal] Register: cannot resolve caliber for " + item.GetType());
				return;
			}
			int rounds = GetItemRounds(item);
			int total = GetCaliberCount(caliber) + rounds;
			m_AmmoByCaliber.Set(caliber, total);
			#ifdef DM_BOT_DEBUG_ARSENAL
			dmBotLog.Debug("[Arsenal] Register ammo: type=" + item.GetType() + " caliber=" + caliber);
			dmBotLog.Debug("[Arsenal] Register ammo: rounds=" + rounds + " total=" + total);
			#endif
		}
	}

	//! Выброс предмета: обратная регистрация.
	void Unregister(EntityAI item)
	{
		if (!item)
			return;
		if (m_Weapons.Find(item) >= 0)
		{
			m_Weapons.RemoveItem(item);
			#ifdef DM_BOT_DEBUG_ARSENAL
			dmBotLog.Debug("[Arsenal] Unregister weapon: " + item.GetType());
			#endif
		}
		if (m_Melee.Find(item) >= 0)
		{
			m_Melee.RemoveItem(item);
			#ifdef DM_BOT_DEBUG_ARSENAL
			dmBotLog.Debug("[Arsenal] Unregister melee: " + item.GetType());
			#endif
		}
		if (item.IsMagazine() || item.IsAmmoPile())
		{
			string caliber = GetItemCaliber(item);
			if (caliber == "")
			{
				dmBotLog.Error("[Arsenal] Unregister: cannot resolve caliber for " + item.GetType());
				return;
			}
			int rounds = GetItemRounds(item);
			int cur = GetCaliberCount(caliber) - rounds;
			if (cur <= 0)
				m_AmmoByCaliber.Remove(caliber);
			else
				m_AmmoByCaliber.Set(caliber, cur);
			#ifdef DM_BOT_DEBUG_ARSENAL
			dmBotLog.Debug("[Arsenal] Unregister ammo: type=" + item.GetType() + " caliber=" + caliber);
			dmBotLog.Debug("[Arsenal] Unregister ammo: rounds=" + rounds + " left=" + cur);
			#endif
		}
	}

	//! Фоновый тик: стартовый отложенный рескан (5 с после спавна) + периодическая
	//! самовосстанавливающаяся актуализация (полный ре-регистр раз в 7–10 минут).
	void Tick(float dt)
	{
		if (!m_InitialRescanDone)
		{
			m_SpawnTimer += dt;
			if (m_SpawnTimer >= DM_ARSENAL_RESCAN_DELAY && !m_RescanQueue)
				BeginRescan();
			if (m_RescanQueue)
				ProcessRescanFrame();
		}
		else
		{
			//! Периодическая самовосстанавливающаяся актуализация: раз в случайные
			//! 7–10 минут полный ре-регистр всех вещей инвентаря (пофреймово).
			m_PeriodicTimer += dt;
			if (m_PeriodicTimer >= m_PeriodicInterval && !m_RescanQueue)
			{
				BeginRescan();
				m_PeriodicTimer = 0.0;
				m_PeriodicInterval = RandomInterval();
			}
			if (m_RescanQueue)
				ProcessRescanFrame();
		}
	}

	//! Начать полный рескан: очистить списки и зафиксировать очередь вещей инвентаря.
	private void BeginRescan()
	{
		m_Weapons.Clear();
		m_Melee.Clear();
		m_AmmoByCaliber.Clear();

		dmAISurvivorBase pawn = m_Owner.GetPawn();
		if (!pawn)
		{
			m_InitialRescanDone = true; // пешки нет — нечего ре-регистрировать
			return;
		}
		m_RescanQueue = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, m_RescanQueue);
		m_RescanIdx = 0;
	}

	//! Одна вещь за фрейм: Register + инкремент индекса; при исчерпании — завершение.
	private void ProcessRescanFrame()
	{
		if (m_RescanIdx < m_RescanQueue.Count())
		{
			Register(m_RescanQueue[m_RescanIdx]);
			m_RescanIdx = m_RescanIdx + 1;
		}
		if (m_RescanIdx >= m_RescanQueue.Count())
		{
			m_RescanQueue = null;
			m_InitialRescanDone = true;
			#ifdef DM_BOT_DEBUG_ARSENAL
			dmBotLog.Debug("[Arsenal] Rescan done: weapons=" + m_Weapons.Count() + " melee=" + m_Melee.Count() + " calibers=" + m_AmmoByCaliber.Count());
			#endif
		}
	}

	//! Все винтовки, эффективные на дистанции dist (GetEffectiveRange >= dist).
	array<Weapon_Base> WeaponAllowDist(float dist)
	{
		array<Weapon_Base> result = new array<Weapon_Base>();
		int i;
		for (i = 0; i < m_Weapons.Count(); i++)
		{
			Weapon_Base w = Weapon_Base.Cast(m_Weapons[i]);
			if (!w)
				continue;
			if (dmWeaponRange.GetEffectiveRange(w) >= dist)
				result.Insert(w);
		}
		return result;
	}

	//! Все патроны под калибр данной винтовки.
	int WeaponAmmoCount(Weapon_Base w)
	{
		string caliber = dmWeaponRange.GetCaliber(w);
		if (caliber == "")
			return 0;
		return GetCaliberCount(caliber);
	}

	//! Самое удобное мили: в руках мили, иначе первое не сломанное в списке.
	ItemBase WeaponMelee()
	{
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_Owner.GetPawn());
		if (pawn)
		{
			EntityAI inHands = pawn.GetItemInHands();
			if (inHands && dmLoot.IsMelee(inHands) && !inHands.IsRuined())
				return ItemBase.Cast(inHands);
		}
		int i;
		for (i = 0; i < m_Melee.Count(); i++)
		{
			if (m_Melee[i] && !m_Melee[i].IsRuined())
				return ItemBase.Cast(m_Melee[i]);
		}
		return null;
	}

	//! Число патронов под калибр в m_AmmoByCaliber (0 при отсутствии).
	private int GetCaliberCount(string caliber)
	{
		int count;
		if (!m_AmmoByCaliber.Find(caliber, count))
			return 0;
		return count;
	}

	//! Калибр (класс пули) магазина/россыпи: CfgMagazines <тип> ammo -> имя пули.
	private string GetItemCaliber(EntityAI item)
	{
		if (!item)
			return "";
		string bullet;
		if (!g_Game.ConfigGetText(CFG_MAGAZINESPATH + " " + item.GetType() + " ammo", bullet))
			return "";
		return bullet;
	}

	//! Число патронов в магазине/россыпи. Magazine — typedef, покрывает и отъёмный
	//! магазин, и Ammunition_Base (россыпную пачку); GetAmmoCount() — канонический
	//! счётчик патронов в обоих случаях (как в dmLoot.GetFirearmAmmoCount).
	private int GetItemRounds(EntityAI item)
	{
		Magazine mag = Magazine.Cast(item);
		if (!mag)
			return 0;
		return mag.GetAmmoCount();
	}

	//! Случайный интервал актуализации: 7–10 минут (в секундах).
	private float RandomInterval()
	{
		int minutes = Math.RandomIntInclusive(DM_ARSENAL_RESCAN_MIN_MINUTES, DM_ARSENAL_RESCAN_MAX_MINUTES);
		int seconds = minutes * 60;
		return (float)seconds;
	}
}
