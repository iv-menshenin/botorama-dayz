//! dmBotMeleeCombat — "magic" melee target selection without raycasts.
//!
//! The vanilla DayZPlayerImplementMeleeCombat.Update() only runs TargetSelection
//! on the client (#ifndef SERVER); on the server it just Resets the cached weapon/
//! mode/range. For an AI bot the whole simulation runs on the server, so we
//! override Update() to always select a target — taken straight from the brain's
//! hostile target instead of a raycast (the vanilla three-pass raycast/cone is
//! expensive and pointless when the bot already knows whom to hit).

class dmBotMeleeCombat : DayZPlayerImplementMeleeCombat
{
	//! Server-side "magic": always Reset, then pick the target directly from the
	//! brain (no raycast). Finishers are disabled (-1).
	override void Update(InventoryItem weapon, EMeleeHitType hitMask, bool wasHitEvent = false)
	{
		super.Update(weapon, hitMask, wasHitEvent);
		TargetSelection();
		SetFinisherType(-1);
	}

	//! Pick the brain's hostile target directly, without any raycast.
	override protected void TargetSelection()
	{
		InternalResetTarget();

		PlayerBase pb = PlayerBase.Cast(m_DZPlayer);
		dmAISurvivor bot = dmAISurvivor.Find(pb);
		if (!bot)
			return;

		dmTarget t = bot.GetHostileTarget();
		if (!t || !t.m_Entity)
			return;

		SetTargetObject(t.m_Entity);

		vector hp = t.m_Entity.GetPosition();
		hp[1] = hp[1] + DM_EYE_HEIGHT;
		SetHitPos(hp);

		SetHitZoneIdx(-1);
		SetFinisherType(-1);
	}

	//! Public wrapper around the protected GetRange() (weapon reach + extender).
	float GetReach()
	{
		return GetRange();
	}
}
