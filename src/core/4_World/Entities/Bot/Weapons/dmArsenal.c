//! dmArsenal — профиль арсенала бота: зарегистрированный огнестрел, мили и
//! патроны по калибру. Заполняется вызовами Register/Unregister (подбор/выброс —
//! подключаются отдельной задачей); фоновый Tick раз в DM_ARSENAL_RECHECK_INTERVAL
//! перепроверяет по одному предмету и по одному калибру (страховка от дрейфа
//! состояния: выстрел сжигает патроны без Unregister, предмет мог выпасть и т.п.).
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

	float m_RecheckTimer;
	int m_RecheckIdx;                      // какой m_Weapons проверяем следующим
	int m_CaliberRecheckIdx;               // какой калибр пересчитываем следующим

	void dmArsenal(dmAISurvivor owner)
	{
		m_Owner = owner;
		m_Weapons = new array<EntityAI>();
		m_AmmoByCaliber = new map<string, int>();
		m_Melee = new array<EntityAI>();
		m_RecheckTimer = 0.0;
		m_RecheckIdx = 0;
		m_CaliberRecheckIdx = 0;
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

	//! Фоновый пере-чек: раз в DM_ARSENAL_RECHECK_INTERVAL — один предмет + один калибр.
	void Tick(float dt)
	{
		m_RecheckTimer += dt;
		if (m_RecheckTimer < DM_ARSENAL_RECHECK_INTERVAL)
			return;
		m_RecheckTimer = 0.0;
		RecheckOneWeapon();
		RecheckOneCaliber();
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

	//! Жив ли предмет и лежит ли он (по-прежнему) в инвентаре пешки.
	private bool IsInInventory(EntityAI item)
	{
		if (!item)
			return false;
		dmAISurvivorBase pawn = m_Owner.GetPawn();
		if (!pawn)
			return false;
		array<EntityAI> items = new array<EntityAI>();
		pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
		return items.Find(item) >= 0;
	}

	//! Проверить m_Weapons[m_RecheckIdx] (по кругу): выпал из инвентаря — Remove.
	private void RecheckOneWeapon()
	{
		if (m_Weapons.Count() == 0)
			return;
		if (m_RecheckIdx >= m_Weapons.Count())
			m_RecheckIdx = 0;
		EntityAI item = m_Weapons[m_RecheckIdx];
		if (!item || !IsInInventory(item))
		{
			m_Weapons.RemoveItem(item);
			#ifdef DM_BOT_DEBUG_ARSENAL
			if (item)
				dmBotLog.Debug("[Arsenal] RecheckOneWeapon: " + item.GetType() + " left inventory");
			#endif
		}
		if (m_Weapons.Count() > 0)
			m_RecheckIdx = (m_RecheckIdx + 1) % m_Weapons.Count();
		else
			m_RecheckIdx = 0;
	}

	//! Пересчитать следующий калибр из m_AmmoByCaliber: пронумеровать инвентарь,
	//! суммировать GetItemRounds у магазинов/пачек этого калибра.
	private void RecheckOneCaliber()
	{
		if (m_AmmoByCaliber.Count() == 0)
			return;
		TStringArray calibers = m_AmmoByCaliber.GetKeyArray();
		if (m_CaliberRecheckIdx >= calibers.Count())
			m_CaliberRecheckIdx = 0;
		string caliber = calibers[m_CaliberRecheckIdx];
		if (m_CaliberRecheckIdx < calibers.Count() - 1)
			m_CaliberRecheckIdx = m_CaliberRecheckIdx + 1;
		else
			m_CaliberRecheckIdx = 0;

		int total = 0;
		dmAISurvivorBase pawn = m_Owner.GetPawn();
		if (pawn)
		{
			array<EntityAI> items = new array<EntityAI>();
			pawn.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
			int i;
			for (i = 0; i < items.Count(); i++)
			{
				EntityAI it = items[i];
				if (!it)
					continue;
				if (!it.IsMagazine() && !it.IsAmmoPile())
					continue;
				if (GetItemCaliber(it) != caliber)
					continue;
				total = total + GetItemRounds(it);
			}
		}

		if (total <= 0)
			m_AmmoByCaliber.Remove(caliber);
		else
			m_AmmoByCaliber.Set(caliber, total);
		#ifdef DM_BOT_DEBUG_ARSENAL
		dmBotLog.Debug("[Arsenal] RecheckOneCaliber: " + caliber + " -> " + total);
		#endif
	}
}
