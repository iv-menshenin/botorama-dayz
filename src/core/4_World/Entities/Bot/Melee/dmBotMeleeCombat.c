//! dmBotMeleeCombat — server-side melee target selection for AI bots.
//!
//! The vanilla DayZPlayerImplementMeleeCombat.Update() only runs TargetSelection
//! on the client (#ifndef SERVER); on the server it just Resets the cached weapon/
//! mode/range. For an AI bot the whole simulation runs on the server, so we
//! override Update() to always select a target — taken from the brain's hostile
//! target, with a single head->torso raycast to resolve a valid hitZone (component
//! index) so the vanilla EvaluateHit_Common path (damage + blood/sound effects +
//! block check) works. Finishers are disabled.

class dmBotMeleeCombat : DayZPlayerImplementMeleeCombat
{
	//! Server-side: always Reset, then pick the target from the brain and resolve its
	//! hitZone via a head->torso raycast. Finishers are disabled (-1).
	override void Update(InventoryItem weapon, EMeleeHitType hitMask, bool wasHitEvent = false)
	{
		super.Update(weapon, hitMask, wasHitEvent);
		TargetSelection();
		SetFinisherType(-1);
	}

	//! Pick the target explicitly chosen by the brain's RequestMeleeAttack (set by
	//! dmBotIntent_HitTo). A single ray from the bot's head/eyes to the target's
	//! default hit position (torso / center of mass) resolves a valid hitZone
	//! (component index) — NOT along the look direction, the bot already knows whom
	//! to hit. With hitZone >= 0 the vanilla EvaluateHit_Common path runs (damage +
	//! blood/sound effects + block check) instead of the old direct-damage fallback.
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

		vector botPos = m_DZPlayer.GetPosition();
		vector rayStart = botPos + Vector(0, DM_EYE_HEIGHT, 0);
		int headBone = m_DZPlayer.GetBoneIndexByName("Head");
		if (headBone >= 0)
			rayStart = m_DZPlayer.GetBonePositionWS(headBone);

		vector rayEnd = target.ModelToWorld(target.GetDefaultHitPosition());

		vector hitPos = rayEnd;
		vector hitNormal;
		int hitZone = -1;
		set<Object> hitObjects = new set<Object>;

		if (DayZPhysics.RaycastRV(rayStart, rayEnd, hitPos, hitNormal, hitZone, hitObjects, null, m_DZPlayer, false, false, ObjIntersectIFire, 0.0, CollisionFlags.ALLOBJECTS) && hitZone >= 0)
		{
			SetHitZoneIdx(hitZone);
			SetHitPos(hitPos);
		}
		else
		{
			//! Should not happen while the target is in reach; keep a sane hit pos
			//! so the native hit detect has a valid point even without a component.
			SetHitZoneIdx(-1);
			SetHitPos(rayEnd);
		}

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug("dmBotMeleeCombat: TargetSelection target=" + target + " hitZone=" + hitZone);
		#endif

		SetFinisherType(-1);
	}

	//! Public wrapper around the protected GetRange() (weapon reach + extender).
	float GetReach()
	{
		return GetRange();
	}

	//! Дальность мили из оружия напрямую (light-режим как представитель), БЕЗ
	//! трогания кэша m_WeaponMode/m_WeaponRange (их читает EvaluateHit для урона —
	//! нельзя сбрасывать на каждый запрос дальности). Эталон — Expansion
	//! Reset(weapon, EMeleeHitType.NONE) (light-режим) на смену оружия.
	float GetWeaponReach(InventoryItem weapon)
	{
		MeleeCombatData data;
		int mode;
		if (weapon)
		{
			data = weapon.GetMeleeCombatData();
			mode = weapon.GetMeleeMode();
		}
		else
		{
			data = m_DZPlayer.GetMeleeCombatData();
			mode = 0;
		}
		return data.GetModeRange(mode) + RANGE_EXTENDER_NORMAL;
	}
}
