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

	//! Pick the target explicitly chosen by the brain's RequestMeleeAttack (set by
	//! dmBotIntent_HitTo), without any raycast.
	override protected void TargetSelection()
	{
		InternalResetTarget();

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_DZPlayer);
		if (!pawn)
			return;

		EntityAI target = pawn.GetMeleeAttackTarget();
		if (!target)
			return;

		SetTargetObject(target);

		//! Hit position = the target's default hit point (chest/torso) in world space,
		//! like the vanilla EvaluateHit_Common. Do NOT look up a "Spine3" bone — zombies
		//! (ZombieBase is DayZCreature, not Human) have no Spine3, so it fell back to
		//! GetPosition() (feet/ground) and the hit effect became a surface impact
		//! (leaves/sparks/snow) instead of blood on the body.
		SetHitPos(target.ModelToWorld(target.GetDefaultHitPosition()));

		SetHitZoneIdx(-1);
		SetFinisherType(-1);
	}

	//! Public wrapper around the protected GetRange() (weapon reach + extender).
	float GetReach()
	{
		return GetRange();
	}
}
