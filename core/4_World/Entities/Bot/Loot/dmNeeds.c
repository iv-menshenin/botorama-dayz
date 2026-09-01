//! dmNeeds — координатор нужд (слой 2 лута). Тикается из Update бота (троттлинг 5с):
//! читает dmRequirements (инвентарь) и пишет в dmWishlist (желания). Домены остаются
//! чистыми — связи живут здесь.
class dmNeeds
{
	float m_TickAccum;   // накопитель до DM_NEEDS_TICK_INTERVAL

	void dmNeeds()
	{
		m_TickAccum = 0.0;
	}

	//! Tick из Update бота. Троттлинг ~5с.
	void Update(dmAISurvivor bot, float pDt)
	{
		m_TickAccum += pDt;
		if (m_TickAccum < DM_NEEDS_TICK_INTERVAL)
			return;
		m_TickAccum = 0.0;

		if (!bot)
			return;

		dmRequirements req = bot.GetRequirements();
		dmWishlist wish = bot.GetWishlist();
		if (!wish)
			return;

		if (req)
			req.Update(bot);

		//! Нет оружия (огнестрел) → желать Weapon_Base.
		bool hasWeapon = bot.HasFirearmInHands();
		if (!hasWeapon && req)
			hasWeapon = req.HasItemInherited(Weapon_Base);
		if (hasWeapon)
			wish.RemoveDesired(Weapon_Base);
		else
			wish.SetDesired(Weapon_Base, 1.0);

		//! Нет ножа → желать ToolBase (база ножей; в ванили нет Knife_Base,
		//! все ножи наследуют ToolBase напрямую).
		bool hasKnife = false;
		if (req)
			hasKnife = req.HasKnife();
		if (hasKnife)
			wish.RemoveDesired(ToolBase);
		else
			wish.SetDesired(ToolBase, 1.0);

		//! Повреждённое оружие + нет чистящего набора → желать WeaponCleaningKit.
		bool needClean = false;
		if (req)
			needClean = req.HasDamagedWeapon() && !req.HasItemInherited(WeaponCleaningKit);
		if (needClean)
			wish.SetDesired(WeaponCleaningKit, 1.0);
		else
			wish.RemoveDesired(WeaponCleaningKit);
	}
};
