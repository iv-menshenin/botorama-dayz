
class dmShowCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_SHOW;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2)
			return false;

		if (parts[1] == DM_CHAT_SHOW_VERSION)
			return HandleShowVersion(player);

		if (parts[1] == DM_CHAT_SHOW_POSITION)
			return HandleShowPosition(player);

		if (parts[1] == DM_CHAT_SHOW_AGRESSION)
			return HandleShowAgression(player);

		if (parts[1] == DM_CHAT_SHOW_RAYCAST)
			return HandleShowRaycast(player, parts);

		return false;
	}

	private bool HandleShowVersion(PlayerBase player)
	{
		dmCommandManager.ChatToPlayer(player, "Botorama version: " + DM_BOTORAMA_VERSION + " [by devalio] ©2026");
		return true;
	}

	private bool HandleShowPosition(PlayerBase player)
	{
		dmCommandManager.ChatToPlayer(player, "Player position: " + player.GetPosition());
		return true;
	}

	private bool HandleShowAgression(PlayerBase player)
	{
		dmAISurvivor bot = dmCommandContext.FindBotForPlayer(player);
		if (!bot)
        {
		    dmCommandManager.ChatToPlayer(player, "У тебя нет своего бота");
            return false;
        }

        dmTarget t = bot.FindTarget(player);
        if ( !t )
        {
		    dmCommandManager.ChatToPlayer(player, "Бот не расценивает тебя как цель");
            return false;
        }
		dmCommandManager.ChatToPlayer(player, "Параметры цели: видит=" + t.m_HasLOS + " контакт=" + t.m_LastContact + " опасность=" + t.m_Threat);
		return true;
	}

	private bool HandleShowRaycast(PlayerBase player, array<string> parts)
	{
		if ( parts.Count() < 8 )
		{
			dmCommandManager.ChatToPlayer(player, "format: rc x1 y1 z1 x2 y2 z2 [f|v|g]");
			return false;
		}

		float x = parts[2].ToFloat();
		float y = parts[3].ToFloat();
		float z = parts[4].ToFloat();
		vector beg = Vector(x, y, z);

		x = parts[5].ToFloat();
		y = parts[6].ToFloat();
		z = parts[7].ToFloat();
		vector end = Vector(x, y, z);

		int cf = ObjIntersectView;
		if ( parts.Count() > 8 )
		{
			switch (parts[8])
			{
				case "f":
					cf = ObjIntersectFire;
					break;
				case "v":
					cf = ObjIntersectView;
					break;
				case "g":
					cf = ObjIntersectGeom;
					break;
				case "i":
					cf = ObjIntersectIFire;
					break;
				case "n":
					cf = ObjIntersectNone;
					break;
			}
		}

		array<string> res1 = HandleShowRaycastRVProxy(player, beg, end, cf);
		foreach(string res: res1)
		{
			dmCommandManager.ChatToPlayer(player, "RaycastRVProxy: " + res);
			dmBotLog.Debug("[RAYCAST] " + beg + " to " + end + " (cf=" + cf + ") Result: " + res);
		}

		string res2 = HandleShowRaycastRV(player, beg, end, cf);
		dmCommandManager.ChatToPlayer(player, "RaycastRV: " + res2);
		dmBotLog.Debug("[RAYCAST] " + beg + " to " + end + " (cf=" + cf + ") Result: " + res2);

		return true;
	}

	private array<string> HandleShowRaycastRVProxy(PlayerBase player, vector beg, vector end, int cf)
	{
		array<string> result = new array<string>();

		RaycastRVParams rp = new RaycastRVParams(beg, end, player, 0.0);
		rp.sorted = true;
		rp.type = cf;
		rp.flags = CollisionFlags.ALLOBJECTS;
		ref array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if ( DayZPhysics.RaycastRVProxy(rp, hits) )
		{
			for (int i = 0; i < hits.Count(); i++)
			{
				string hitsRep;
				RaycastRVResult hit = hits[i];

				hitsRep = hitsRep + " #" + i + " ";

				if (hit.surface)
					hitsRep = hitsRep + "surf: " + hit.surface.GetName() + " ";

				if (hit.obj)
					hitsRep = hitsRep + "objName: " + hit.obj.GetType() + " ";

				result.Insert( hitsRep );
			}
			result.Insert( "HIT count=" + hits.Count() + " " + hitsRep );
		} else {
			result.Insert( "NO HIT count=" + hits.Count() );
		}

		return result;
	}

	private string HandleShowRaycastRV(PlayerBase player, vector beg, vector end, int cf)
	{
		vector contactPos;
		vector contactDir;
		int contactComponent;

		if ( DayZPhysics.RaycastRV(beg, end, contactPos, contactDir, contactComponent, null, null, player, false, false, cf) )
		{
			return "HIT contactPos=" + contactPos + " contactDir=" + contactDir + " contactComponent=" + contactComponent;
		}
		return "NO HIT";
	}
}