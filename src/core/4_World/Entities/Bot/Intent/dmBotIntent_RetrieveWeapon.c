//! dmBotIntent_RetrieveWeapon — after a knockout the bot dropped its weapon from
//! its hands; once awake it "humanly" scans the ground around itself and picks the
//! dropped item back up.
//!
//! CRITICAL + EXCLUSIVE, MOVE channel. The bot sweeps a full 360° circle over
//! DM_RETRIEVE_SCAN_TIME seconds (random direction), looking at the ground
//! DM_RETRIEVE_LOOK_DIST ahead through a paired HoldLook (FULL) intent whose
//! m_Point is advanced along the circle each tick. Every tick it raycasts from its
//! face to the item; when the item is within the DM_RETRIEVE_FOV half-cone AND
//! there is a clear line of sight, it delegates the pickup to a dmBotIntent_PickUp
//! and finishes early. A full sweep without a find fails.
class dmBotIntent_RetrieveWeapon : dmBotIntent
{
	EntityAI m_Item;                    // выпавшее оружие (цель поиска)
	float m_Angle;                      // текущий угол свипа (0..360)
	float m_Direction;                  // ±1 (направление вращения)
	float m_StartYaw;                   // стартовый яу корпуса (мир), якорь свипа
	ref dmBotIntent_HoldLook m_Look;    // круговой взгляд (точка на земле)

	void dmBotIntent_RetrieveWeapon()
	{
		m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
		m_Priority = dmBotIntentPriority.CRITICAL;
		m_Manage = dmBotIntentsChannel.MOVE;
	}

	override string GetIntentName()
	{
		return "RetrieveWeapon";
	}

	override void OnStart(dmAISurvivor bot)
	{
		super.OnStart(bot);

		m_Angle = 0.0;
		if (Math.RandomFloat(0.0, 1.0) < 0.5)
			m_Direction = -1.0;
		else
			m_Direction = 1.0;

		//! Якорь свипа — стартовый яу корпуса в мировых координатах. HoldLook (FULL)
		//! доворачивает корпус к точке взгляда, поэтому живой GetOrientation()[0]
		//! дал бы положительную обратную связь (тело догоняет точку, круга нет).
		m_StartYaw = bot.GetOrientation()[0];

		m_Look = new dmBotIntent_HoldLook();
		m_Look.m_Turn = dmBotLookTurn.FULL;
		m_Look.m_Priority = dmBotIntentPriority.CRITICAL;
		m_Look.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
		bot.AddFSMIntent(m_Look);
	}

	override void OnUpdate(dmAISurvivor bot, float pDt)
	{
		super.OnUpdate(bot, pDt);

		if (!m_Item || m_Item.IsDamageDestroyed() || m_Item.IsSetForDeletion())
		{
			Fail();
			return;
		}

		m_Angle += (360.0 / DM_RETRIEVE_SCAN_TIME) * pDt;
		if (m_Angle >= 360.0)
		{
			#ifdef DM_BOT_DEBUG_BODY
			dmBotLog.Debug("[Body] RetrieveWeapon: прошёл полный круг, оружие не найдено");
			#endif
			Fail();
			return;
		}

		vector botPos = bot.GetPosition();
		float lookYaw = m_StartYaw + m_Direction * m_Angle;
		vector lookDir = Vector(lookYaw, 0.0, 0.0).AnglesToVector();
		vector lookPoint = botPos + lookDir * DM_RETRIEVE_LOOK_DIST;
		lookPoint[1] = GetGame().SurfaceY(lookPoint[0], lookPoint[2]);

		if (m_Look)
			m_Look.m_Point = lookPoint;

		vector itemPos = m_Item.GetPosition();
		vector toItem = itemPos - botPos;
		toItem[1] = 0.0;
		if (toItem.Length() > 0.01)
		{
			float itemYaw = toItem.VectorToAngles()[0];
			float ang = Math.AbsFloat(dmAISurvivor.AngleDiff(itemYaw, lookYaw));
			if (ang < DM_RETRIEVE_FOV)
			{
				if (HasClearLine(bot, botPos, itemPos))
				{
					#ifdef DM_BOT_DEBUG_BODY
					dmBotLog.Debug("[Body] RetrieveWeapon: оружие видно, подбираю " + m_Item.GetType());
					#endif
					dmBotIntent_PickUp pick = new dmBotIntent_PickUp();
					pick.m_Item = m_Item;
					pick.m_Priority = dmBotIntentPriority.CRITICAL;
					pick.m_Concurrency = dmBotIntentConcurrency.EXCLUSIVE;
					bot.AddPersonalityIntent(pick);
					Finish();
					return;
				}
			}
		}
	}

	override void OnCancel(dmAISurvivor bot)
	{
		if (m_Look)
		{
			m_Look.Finish();
			m_Look = null;
		}
		super.OnCancel(bot);
	}

	//! Raycast from the bot's face to the item; visible when nothing blocks, or
	//! the closest hit is the item itself (mirrors dmBotIntent_Flank.HasClearLine).
	private bool HasClearLine(dmAISurvivor bot, vector from, vector to)
	{
		vector beg = from + Vector(0.0, DM_EYE_HEIGHT, 0.0);
		RaycastRVParams rp = new RaycastRVParams(beg, to, bot.GetPawn());
		rp.sorted = true;
		rp.type = ObjIntersectView;
		rp.flags = CollisionFlags.NEARESTCONTACT;

		ref array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
		if (!DayZPhysics.RaycastRVProxy(rp, hits) || hits.Count() == 0)
			return true;

		Object o = hits[0].obj;
		Object parent = hits[0].parent;
		if (o == m_Item || parent == m_Item)
			return true;
		return false;
	}
}
