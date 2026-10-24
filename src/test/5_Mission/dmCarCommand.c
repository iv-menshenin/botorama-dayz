
//! dmCarCommand — команда "/car repair": полностью чинит машину, на которую
//! смотрит игрок (корпус + все damage-зоны + все attachments + oil/brake).
//! Топливо/воду НЕ трогаем, недостающие детали НЕ добавляем.
class dmCarCommand : dmCommandModule
{
	override string GetName()
	{
		return DM_CHAT_CMD_CAR;
	}

	override bool Handle(PlayerBase player, array<string> parts)
	{
		if (parts.Count() < 2 || parts[1] != DM_CHAT_CAR_REPAIR)
		{
			dmCommandManager.ChatToPlayer(player, "Укажи: /car repair");
			return false;
		}

		return HandleRepair(player);
	}

	//! Чинит машину в прицеле: зоны, attachments, oil/brake.
	private bool HandleRepair(PlayerBase player)
	{
		CarScript car = FindLookedAtCar(player);
		if (!car)
		{
			#ifdef DM_BOT_DEBUG_CAR
			dmBotLog.Debug("[CAR] /car repair: нет машины в прицеле");
			#endif
			dmCommandManager.ChatToPlayer(player, "Нет машины в прицеле.");
			return false;
		}

		int zonesCount;
		int attCount;
		dmCarRepair.Repair(car, zonesCount, attCount);

		dmCommandManager.ChatToPlayer(player, "Машина отремонтирована. Зон: " + zonesCount + ", деталей: " + attCount);
		return true;
	}

	//! Машина, на которую смотрит игрок (рейкаст вдоль взгляда головы).
	private CarScript FindLookedAtCar(PlayerBase player)
	{
		vector beg;
		vector dir;

		int headBone = player.GetBoneIndexByName("Head");
		if (headBone != -1)
		{
			vector headTransform[4];
			player.GetBoneTransformWS(headBone, headTransform);
			beg = player.GetBonePositionWS(headBone);
			dir = headTransform[1];
		}
		else
		{
			vector playerPos = player.GetPosition();
			beg = playerPos + Vector(0, DM_EYE_HEIGHT, 0);
			dir = MiscGameplayFunctions.GetHeadingVector(player);
		}

		vector end = beg + dir * DM_LOOK_RAYCAST_DISTANCE;

		RaycastRVParams rp = new RaycastRVParams(beg, end, player, 0.0);
		rp.sorted = true;
		rp.type = ObjIntersectView;
		rp.flags = CollisionFlags.ALLOBJECTS;
		ref array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (DayZPhysics.RaycastRVProxy(rp, hits))
		{
			int i;
			for (i = 0; i < hits.Count(); i++)
			{
				CarScript parentCar = CarScript.Cast(hits[i].parent);
				if (parentCar)
					return parentCar;

				EntityAI e = EntityAI.Cast(hits[i].obj);
				while (e)
				{
					CarScript car = CarScript.Cast(e);
					if (car)
						return car;
					e = e.GetHierarchyParent();
				}
			}
		}

		return null;
	}
}
