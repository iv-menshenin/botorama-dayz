//! dmLaunchCommand — команда "/launch ...": стресс-тест сценарии.
class dmLaunchCommand : dmCommandModule
{
	override string GetName()
	{
		return "launch";
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи сценарий: /launch invasion");
			return false;
		}

		if (parts[1] == DM_CHAT_LAUNCH_INVASION)
			return HandleInvasion(player);

		dmCommandManager.ChatToPlayer(player, "Неизвестный сценарий: " + parts[1]);
		return false;
	}

	//! "/launch invasion" — 3*N ботов на 700 м (случайный угол на каждого), враждебны ко всем игрокам (размыто 250 м).
	private bool HandleInvasion(PlayerBase player)
	{
		array<PlayerBase> realPlayers = new array<PlayerBase>();
		array<PlayerBase> all = dmEntityRegistry.GetPlayers();
		int i;
		for (i = 0; i < all.Count(); i++)
		{
			PlayerBase p = all[i];
			if (p && p.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
				realPlayers.Insert(p);
		}

		if (realPlayers.Count() == 0)
		{
			dmCommandManager.ChatToPlayer(player, "Нет реальных игроков.");
			return false;
		}

		int count = DM_INVASION_MULT * realPlayers.Count();
		vector origin = player.GetPosition();

		for (i = 0; i < count; i++)
		{
			float angle = Math.RandomFloat01() * 360.0;
			vector dir = Vector(angle, 0.0, 0.0).AnglesToVector();
			vector spawnPos = origin + dir * DM_INVASION_DISTANCE;
			spawnPos = SnapToGroundExactly(spawnPos);

			float facing = angle + 180.0;
			if (facing >= 360.0)
				facing = facing - 360.0;

			ref dmAISurvivor bot = new dmAISurvivor();
			bot.SetModel(dmSurvivor.GetRandom());
			PlayerBase pawn = bot.Spawn(spawnPos, Vector(facing, 0.0, 0.0));
			if (!pawn)
				continue;

			dmLoadoutConfig cfg = dmLoadoutApplier.Load("Stalker");
			if (cfg)
			{
				dmLoadoutApplier.Apply(pawn, cfg);
			}

			GiveWeapon(pawn);

			bot.SetFSM(dmBotTestPreset_Hunting.Create(bot));

			int j;
			for (j = 0; j < realPlayers.Count(); j++)
				bot.RegisterHostile(realPlayers[j], 1.0, DM_INVASION_SPREAD);
		}

		dmCommandManager.ChatToPlayer(player, "Инвейжн: " + count + " ботов против " + realPlayers.Count() + " игроков, 700 м.");
		return true;
	}

	//! Случайное оружие + патроны (эталон — HandleEnemy в dmTestCommand.c).
	private void GiveWeapon(PlayerBase pawn)
	{
		array<string> weapons = {"B95", "Mosin9130", "Izh18", "Repeater"};
		int idx = Math.RandomIntInclusive(0, weapons.Count() - 1);
		string weapon = weapons[idx];

		string ammo = "Ammo_308Win";
		if (weapon == "Mosin9130")
			ammo = "Ammo_762x54";
		else if (weapon == "Izh18")
			ammo = "Ammo_762x39";
		else if (weapon == "Repeater")
			ammo = "Ammo_357";

		Weapon_Base w = Weapon_Base.Cast(pawn.GetHumanInventory().CreateInHands(weapon));
		if (w)
			w.SpawnAmmo(ammo, WeaponWithAmmoFlags.CHAMBER);

		pawn.GetInventory().CreateInInventory(ammo);
		pawn.GetInventory().CreateInInventory(ammo);
	}
}
