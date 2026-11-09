modded class PlayerBase
{
	void PlayerBase()
	{
		dmEntityRegistry.RegisterPlayer(this);
	}

	void ~PlayerBase()
	{
		dmEntityRegistry.UnregisterPlayer(this);
	}
	
	override bool Save()
	{
		if ( !GetHive() ) return false;

		return super.Save();
	}

	//------------------------------------------------------------------
	// A4/A5 — собственные источники света (emit / illumination)
	//------------------------------------------------------------------

	//! Кэш собственного света игрока. Пересчитывается лениво при первом обращении
	//! за окном DM_ILLUMINATION_CACHE_INTERVAL — не перебираем инвентарь на каждый
	//! вызов геттера (потребители — серверный мозг и botdump).
	private float m_EmitLightCached = 0.0;
	private float m_IllumCached = 0.0;
	//! Момент последнего пересчёта (GetTickTime); отрицательный сентинел — первый
	//! вызов гарантированно пересчитает кэш.
	private float m_LightCacheTime = -3.0;

	//! Насколько далеко игрока видно (0..1): максимум интенсивности эмиссии среди
	//! его активных источников света. Направленные насадки (UniversalLight/TLRLight
	//! на оружии) учитываются только при поднятом оружии (IsRaised).
	float EmitLightIntensivity()
	{
		RefreshLightCache();
		return m_EmitLightCached;
	}

	//! Насколько далеко игрок сам видит (бинарно 0/1): есть ли хотя бы один
	//! источник, дающий зрение (emit выше DM_ILLUMINATION_THRESHOLD, либо NVG/NV-оптика).
	float IlluminationIntensivity()
	{
		RefreshLightCache();
		return m_IllumCached;
	}

	//! Ленивый пересчёт кэша (дебаунс DM_ILLUMINATION_CACHE_INTERVAL). Оба значения
	//! считаются в одном проходе по инвентарю.
	private void RefreshLightCache()
	{
		float now = GetGame().GetTickTime();
		if (now - m_LightCacheTime < DM_ILLUMINATION_CACHE_INTERVAL)
			return;

		m_LightCacheTime = now;
		RecomputeLightCache();
	}

	//! Перебор надетого и рук (рекурсивно по аттачментам, БЕЗ карго) + классификация
	//! активных источников. NVG — только illumination, не эмиттер.
	private void RecomputeLightCache()
	{
		float emitMax = 0.0;
		float illum = 0.0;
		bool raised = IsRaised();

		ClassifyItemTree(GetItemInHands(), raised, emitMax, illum);

		GameInventory inv = GetInventory();
		if (inv)
		{
			int count = inv.GetAttachmentSlotsCount();
			int i;
			for (i = 0; i < count; i++)
			{
				ClassifyItemTree(inv.FindAttachment(inv.GetAttachmentSlotId(i)), raised, emitMax, illum);
			}
		}

		m_EmitLightCached = emitMax;
		m_IllumCached = illum;
	}

	//! Рекурсивно: классифицируем сам предмет + его аттачменты (насадка-фонарь на
	//! оружии/шлеме, NVG). Карго не трогаем — включённый источник в рюкзаке «не у
	//! игрока».
	private void ClassifyItemTree(EntityAI item, bool raised, inout float emitMax, inout float illum)
	{
		if (!item)
			return;

		ClassifyLight(item, raised, emitMax, illum);

		GameInventory itemInv = item.GetInventory();
		if (!itemInv)
			return;

		int count = itemInv.GetAttachmentSlotsCount();
		int i;
		for (i = 0; i < count; i++)
		{
			ClassifyItemTree(itemInv.FindAttachment(itemInv.GetAttachmentSlotId(i)), raised, emitMax, illum);
		}
	}

	//! Классификация одного предмета как источника света (классы/направленность —
	//! research light-sources-classification.md). «Включён» — единый детект
	//! GetCompEM().IsWorking(). Направленность UniversalLight — по РОДИТЕЛЮ
	//! (IsWeapon), не по классу/слоту: на оружии — узкий прожектор (гейт IsRaised),
	//! на шлеме/в руках — широкий (без гейта). NVG/NV-оптика — зрение, не эмиссия.
	private void ClassifyLight(EntityAI item, bool raised, inout float emitMax, inout float illum)
	{
		//! NVG и NV-оптика дают зрение, но НЕ эмитят свет (emit=0). NVGoggles ловим
		//! раньше ItemOptics — NVGoggles тоже ItemOptics-наследник, но его
		//! GetCurrentNVType() == NVTypes.NONE.
		if (NVGoggles.Cast(item))
		{
			if (IsLightOn(item))
				illum = 1.0;
			return;
		}
		ItemOptics optics = ItemOptics.Cast(item);
		if (optics)
		{
			int nvType = optics.GetCurrentNVType();
			if (nvType != NVTypes.NONE && nvType != NVTypes.NV_OPTICS_OFF)
			{
				illum = 1.0;
				return;
			}
		}

		//! Остальные источники — эмиссия: считаем emit по классу/направленности,
		//! затем illum решается ЕДИНЫМ порогом (emit > DM_ILLUMINATION_THRESHOLD),
		//! чтобы яркий roadflare не «выпадал» из зрения отдельной веткой.
		float emit = 0.0;

		if (UniversalLight.Cast(item))
		{
			if (IsLightOn(item))
			{
				if (IsWeaponAttached(item))
				{
					if (raised)
						emit = DM_EMIT_UNIVERSAL_WEAPON;
				}
				else
				{
					emit = DM_EMIT_UNIVERSAL_HELMET;
				}
			}
		}
		else if (TLRLight.Cast(item))
		{
			if (IsLightOn(item) && raised)
				emit = DM_EMIT_TLR;
		}
		else if (FlammableBase.Cast(item))
		{
			if (IsLightOn(item))
				emit = DM_EMIT_TORCH;
		}
		else if (Roadflare.Cast(item))
		{
			if (IsLightOn(item))
				emit = DM_EMIT_ROADFLARE;
		}
		else if (Chemlight_ColorBase.Cast(item))
		{
			if (IsLightOn(item))
				emit = DM_EMIT_CHEMLIGHT;
		}
		else if (Headtorch_ColorBase.Cast(item))
		{
			if (IsLightOn(item))
				emit = DM_EMIT_HEADTORCH;
		}
		else if (Flashlight.Cast(item))
		{
			if (IsLightOn(item))
				emit = DM_EMIT_FLASHLIGHT;
		}

		if (emit > emitMax)
			emitMax = emit;
		if (emit > DM_ILLUMINATION_THRESHOLD)
			illum = 1.0;
	}

	//! UniversalLight на оружии (или на цевье) — узкий направленный прожектор;
	//! на шлеме/в руках — широкий постоянный свет. Направленность задаёт РОДИТЕЛЬ
	//! (IsWeapon), а не класс и не строка слота (research §2).
	private bool IsWeaponAttached(EntityAI item)
	{
		EntityAI parent = item.GetHierarchyParent();
		if (!parent)
			return false;
		return parent.IsWeapon();
	}

	//! Единый детект «источник включён»: энергоменеджер реально излучает (SwitchOn
	//! + есть энергия, не мокрый, не ruined). Сервер-авторитетно (research §2).
	private bool IsLightOn(EntityAI item)
	{
		if (!item.HasEnergyManager() || !item.GetCompEM())
			return false;
		return item.GetCompEM().IsWorking();
	}
}
