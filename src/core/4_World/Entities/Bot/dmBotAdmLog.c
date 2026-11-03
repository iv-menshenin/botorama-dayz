//! dmBotAdmLog — server-side admin-log journal for bot attacks.
//!
//! One line per bot attack (a firearm shot or a melee hit) is written into the
//! game's admin log via g_Game.AdminLog, so an admin can trace whom a bot
//! attacked and with what. The engine stamps each line's time itself; the bot's
//! name comes from dmAISurvivorBase.GetCachedName() (the vanilla admin-log name
//! mechanism). Server-only: the admin log is a server-side concern, so the whole
//! class is compiled out on the client.

#ifdef SERVER
class dmBotAdmLog
{
	//! Journal one bot attack. Семантика события: isFirearm=true логирует КАЖДЫЙ ВЫСТРЕЛ (включая промахи и выстрел без цели → target=null); isFirearm=false — каждый НАНЕСЁННЫЙ удар (промахи отсечены в EvaluateHit ранними return).
	static void LogAttack(dmAISurvivorBase bot, EntityAI target, EntityAI weapon, bool isFirearm, bool hasOptic)
	{
		string name = "";
		if (bot)
			name = bot.GetCachedName();

		string kind = "мили-удар";
		if (isFirearm)
			kind = "выстрел";

		string targetName = "null";
		vector targetPos = vector.Zero;
		if (target)
		{
			PlayerBase targetPlayer = PlayerBase.Cast(target);
			if (targetPlayer)
				targetName = targetPlayer.GetCachedName();
			else
				targetName = target.GetDisplayName();
			targetPos = target.GetPosition();
		}

		string weaponName = "null";
		if (weapon)
			weaponName = weapon.GetDisplayName();

		vector botPos = vector.Zero;
		if (bot)
			botPos = bot.GetPosition();

		string tag = "[bot:shoot]";
		if (!isFirearm)
			tag = "[bot:melee]";

		string msg = tag + " " + name + " " + kind + ": цель=" + targetName;
		msg = msg + " pos=<" + targetPos[0] + ", " + targetPos[1];
		msg = msg + ", " + targetPos[2] + ">";
		msg = msg + " оружие=" + weaponName;
		msg = msg + " бот pos=<" + botPos[0] + ", " + botPos[1];
		msg = msg + ", " + botPos[2] + ">";
		if (isFirearm)
			msg = msg + " прицел=" + hasOptic;

		g_Game.AdminLog(msg);
	}

	//! Journal a bot's death. Called from dmAISurvivor.OnDeath() on the first
	//! !IsAlive() frame, when modifiers are still active and the stats are
	//! readable. Writes the cause + observed state: an external kill ("убит X (Y)",
	//! killer resolved from EEKilled via GetDeathKiller) or a natural death
	//! ("умер: переохлаждение/…"), plus Water/Energy/HeatComfort/Blood stats,
	//! active disease modifiers and the drowning flag.
	static void LogDeathState(dmAISurvivorBase pawn)
	{
		if (!pawn)
			return;

		string name = pawn.GetCachedName();

		//! Внешний убийца из EEKilled (GetDeathKiller); null = естественная смерть.
		string killerStr = "";
		EntityAI killer = pawn.GetDeathKiller();
		if (killer)
		{
			PlayerBase shooter = PlayerBase.Cast(killer.GetHierarchyRootPlayer());
			if (shooter)
			{
				killerStr = shooter.GetCachedName();
				if (killer != shooter)
					killerStr = killerStr + " (" + killer.GetDisplayName() + ")";
			}
			else
			{
				killerStr = killer.GetDisplayName();
			}
		}

		float water = pawn.GetStatWater().Get();
		float energy = pawn.GetStatEnergy().Get();
		float heat = pawn.GetStatHeatComfort().Get();
		float blood = pawn.GetHealth("", "Blood");

		string causeLabel = "";
		string ammo = pawn.GetLastDamageAmmo();
		if (ammo != "" && ammo.IndexOf("FallDamage") != -1)
			causeLabel = "упал с высоты";
		else if (heat < -0.45)
			causeLabel = "переохлаждение";
		else if (heat > 0.45)
			causeLabel = "перегрев";

		bool drowned = pawn.GetDrowningWaterLevelCheck();

		string diseases = "";
		ModifiersManager mm = pawn.GetModifiersManager();
		if (mm)
		{
			if (mm.IsModifierActive(eModifiers.MDF_CHOLERA)) diseases = AddDisease(diseases, "cholera");
			if (mm.IsModifierActive(eModifiers.MDF_SALMONELLA)) diseases = AddDisease(diseases, "salmonella");
			if (mm.IsModifierActive(eModifiers.MDF_POISONING)) diseases = AddDisease(diseases, "poisoning");
			if (mm.IsModifierActive(eModifiers.MDF_INFLUENZA)) diseases = AddDisease(diseases, "influenza");
			if (mm.IsModifierActive(eModifiers.MDF_COMMON_COLD)) diseases = AddDisease(diseases, "cold");
			if (mm.IsModifierActive(eModifiers.MDF_PNEUMONIA)) diseases = AddDisease(diseases, "pneumonia");
			if (mm.IsModifierActive(eModifiers.MDF_FEVER)) diseases = AddDisease(diseases, "fever");
		}

		vector botPos = pawn.GetPosition();

		string msg = "[bot:dead] " + name;
		if (killerStr != "")
			msg = msg + " убит " + killerStr;
		else
		{
			msg = msg + " умер:";
			if (causeLabel != "")
				msg = msg + " " + causeLabel;
		}
		msg = msg + " pos=<" + botPos[0] + ", " + botPos[1];
		msg = msg + ", " + botPos[2] + ">";
		msg = msg + " Water=" + water;
		msg = msg + " Energy=" + energy;
		msg = msg + " HeatComfort=" + heat;
		msg = msg + " Blood=" + blood;
		if (diseases != "")
			msg = msg + " diseases=" + diseases;
		msg = msg + " drowned=" + drowned;

		g_Game.AdminLog(msg);
	}

	//! Journal a bot spawn: name + world position.
	static void LogSpawn(dmAISurvivorBase pawn)
	{
		if (!pawn)
			return;

		string name = pawn.GetCachedName();
		vector pos = pawn.GetPosition();

		string msg = "[bot:spawn] " + name + " pos=<" + pos[0] + ", " + pos[1];
		msg = msg + ", " + pos[2] + ">";

		g_Game.AdminLog(msg);
	}

	//! Journal a bot's transition to unconscious: name + position + HP + dropped item.
	static void LogUnconscious(dmAISurvivorBase pawn, float hp, EntityAI droppedItem)
	{
		if (!pawn)
			return;

		string name = pawn.GetCachedName();
		vector pos = pawn.GetPosition();

		string msg = "[bot:uncon] " + name + " pos=<" + pos[0] + ", " + pos[1];
		msg = msg + ", " + pos[2] + ">";
		msg = msg + " hp=" + hp;
		if (droppedItem)
			msg = msg + " drop=" + droppedItem.GetType();

		g_Game.AdminLog(msg);
	}

	private static string AddDisease(string diseases, string label)
	{
		if (diseases == "")
			return label;
		return diseases + "," + label;
	}
}
#endif
