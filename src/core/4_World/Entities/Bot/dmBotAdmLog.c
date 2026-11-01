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
			targetName = target.GetDisplayName();
			targetPos = target.GetPosition();
		}

		string weaponName = "null";
		if (weapon)
			weaponName = weapon.GetDisplayName();

		vector botPos = vector.Zero;
		if (bot)
			botPos = bot.GetPosition();

		string msg = name + " " + kind + ": цель=" + targetName;
		msg = msg + " pos=<" + targetPos[0] + ", " + targetPos[1];
		msg = msg + ", " + targetPos[2] + ">";
		msg = msg + " оружие=" + weaponName;
		msg = msg + " бот pos=<" + botPos[0] + ", " + botPos[1];
		msg = msg + ", " + botPos[2] + ">";
		if (isFirearm)
			msg = msg + " прицел=" + hasOptic;

		g_Game.AdminLog(msg);
	}
}
#endif
