//! dmBotState_Idle — stand still and scan around (head, optionally body). Does
//! NOT touch the follow target: it just idles until the FSM transitions away.
class dmBotState_Idle : dmBotState
{
	ref dmBotIntent_LookAround m_Scan;
	ref dmBotIntent_SitByFireplace m_SitIntent;
	ref dmBotIntent_TidyInventory m_TidyIntent;
	float m_SitCooldown = 0.0;
	float m_TidyCooldown = 0.0;
	float m_TotalTimer;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.INTERRUPTIBLE;
	}

	override void OnEntry(dmBotState from)
	{
		m_Scan = null;
		m_SitIntent = null;
		m_TidyCooldown = 5.0;
		m_SitCooldown = 0.0;
		m_TotalTimer = 0.0;
		CreateScan();

		//! В Idle привести режим огня к предпочтительному (если в руках оружие).
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(GetOwner().GetPawn());
		if (pawn)
			pawn.RefreshPreferredFireMode();

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Idle.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		if (m_Scan && (m_Scan.IsFinished() || m_Scan.IsExpired()))
			m_Scan = null;
		if (!m_Scan)
			CreateScan();

		if (m_SitIntent && (m_SitIntent.IsFinished() || m_SitIntent.IsExpired()))
		{
			m_SitIntent = null;
			m_SitCooldown = DM_SIT_BY_FIRE_COOLDOWN;
		}

		if (m_SitCooldown > 0.0)
		{
			m_SitCooldown -= pDt;
			if (m_SitCooldown < 0.0)
				m_SitCooldown = 0.0;
		}

		if (!m_SitIntent && m_SitCooldown <= 0.0)
		{
			FireplaceBase fire = FindNearbyFireplace();
			if (fire)
			{
				m_SitIntent = new dmBotIntent_SitByFireplace();
				m_SitIntent.m_Fireplace = fire;
				if (fire.IsBaseFireplace() || fire.IsBarrelWithHoles())
				{
					m_SitIntent.m_SitDist = DM_SIT_BY_FIRE_DIST_CLOSE;
					dmAISurvivorBase pawn = GetOwner().GetPawn();
					if (pawn && pawn.GetItemInHands())
						m_SitIntent.m_EmoteID = EmoteConstants.ID_EMOTE_SITA;      // «Привал» — с оружием
					else
						m_SitIntent.m_EmoteID = EmoteConstants.ID_EMOTE_CAMPFIRE;  // «Сесть прямо» — пустые руки
				}
				else
				{
					m_SitIntent.m_SitDist = DM_SIT_BY_FIRE_DIST_FAR;
					m_SitIntent.m_EmoteID = EmoteConstants.ID_EMOTE_SITA;
				}
				GetOwner().AddFSMIntent(m_SitIntent);
			}
		}

		m_TotalTimer += pDt;
		m_TidyCooldown -= pDt;
		if (!m_SitIntent && !m_TidyIntent && m_TidyCooldown <= 0.0)
		{
			m_TidyIntent = new dmBotIntent_TidyInventory();
			GetOwner().AddFSMIntent(m_TidyIntent);
			m_TidyCooldown = 15.0;
		}

		if (m_TotalTimer >= 15.0) return EXIT;

		return CONTINUE;
	}

	FireplaceBase FindNearbyFireplace()
	{
		vector pos = GetOwner().GetPosition();
		array<Object> objects = new array<Object>;
		array<CargoBase> proxyCargos = new array<CargoBase>;
		GetGame().GetObjectsAtPosition(pos, DM_SIT_BY_FIRE_RADIUS, objects, proxyCargos);

		FireplaceBase nearest = null;
		float best = DM_SIT_BY_FIRE_RADIUS * DM_SIT_BY_FIRE_RADIUS;
		int i;
		for (i = 0; i < objects.Count(); i++)
		{
			FireplaceBase fire = FireplaceBase.Cast(objects[i]);
			if (!fire || !fire.IsBurning())
				continue;
			vector fp = fire.GetPosition();
			vector d = fp - pos;
			d[1] = 0.0;
			float distSq = d.LengthSq();
			if (distSq < best)
			{
				best = distSq;
				nearest = fire;
			}
		}
		return nearest;
	}

	override bool CanExit()
	{
		if (m_SitIntent)
			return false;
		return super.CanExit();
	}

	void CreateScan()
	{
		m_Scan = new dmBotIntent_LookAround();
		m_Scan.m_AllowBodyTurn = true;
		m_Scan.m_Turn = dmBotLookTurn.NONE;
		m_Scan.m_Priority = dmBotIntentPriority.DESIRABLE;
		GetOwner().AddFSMIntent(m_Scan);
	}
}
