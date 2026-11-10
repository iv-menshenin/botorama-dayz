//! dmBotMeleeCombat — server-side melee target selection for AI bots.
//!
//! The vanilla DayZPlayerImplementMeleeCombat.Update() only runs TargetSelection
//! on the client (#ifndef SERVER); on the server it just Resets the cached weapon/
//! mode/range. For an AI bot the whole simulation runs on the server, so we
//! override Update() to always select a target — taken from the brain's explicit
//! melee target, with a single head->aim-point raycast to resolve a valid hitZone
//! (component index) so the vanilla EvaluateHit_Common path (damage + blood/sound
//! effects + block check) works. Finishers are disabled.

class dmBotMeleeCombat : DayZPlayerImplementMeleeCombat
{
	//! Server-side: always Reset, then pick the target from the brain and resolve its
	//! hitZone via a head->aim-point raycast. Finishers are disabled (-1).
	override void Update(InventoryItem weapon, EMeleeHitType hitMask, bool wasHitEvent = false)
	{
		super.Update(weapon, hitMask, wasHitEvent);
		TargetSelection();
		SetFinisherType(-1);
	}

	//! Pick the target explicitly chosen by the brain's RequestMeleeAttack (set by
	//! dmBotIntent_HitTo). A single ray from the bot's head/eyes to the target's
	//! aim point (GetAimPosition, world coords) resolves a valid hitZone
	//! (component index) — NOT along the look direction, the bot already knows whom
	//! to hit. With hitZone >= 0 the vanilla EvaluateHit_Common path runs (damage +
	//! blood/sound effects + block check) instead of the old direct-damage fallback.
	override protected void TargetSelection()
	{
		InternalResetTarget();

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(m_DZPlayer);
		if (!pawn)
			return;

		dmTarget target = pawn.GetMeleeAttackTarget();
		if (!target || !target.IsValidTarget())
			return;

		EntityAI targetEntity = target.m_Entity;
		SetTargetObject(targetEntity);

		vector botPos = m_DZPlayer.GetPosition();
		vector rayStart = botPos + Vector(0, DM_EYE_HEIGHT, 0);
		int headBone = m_DZPlayer.GetBoneIndexByName("Head");
		if (headBone >= 0)
			rayStart = m_DZPlayer.GetBonePositionWS(headBone);

		//! Точка удара — точка AIM (мировые координаты). При !m_HasLOS она =
		//! m_LastPosition (последняя известная) — «бей туда, где последний раз
		//! видел» (осознанное поведение, не побочный эффект). Guard IsValidTarget()
		//! выше гарантирует, что точка не нулевая.
		vector aimPos = target.GetAimPosition();
		vector rayEnd = ExtendRayToAim(rayStart, aimPos);

		vector hitPos = rayEnd;
		vector hitNormal;
		int hitZone = -1;
		set<Object> hitObjects = new set<Object>;

		if (dmPhysicsProbe.CastRV(rayStart, rayEnd, hitPos, hitNormal, hitZone, hitObjects, null, m_DZPlayer, false, false, ObjIntersectIFire, 0.0, CollisionFlags.ALLOBJECTS) && hitZone >= 0)
		{
			SetHitZoneIdx(hitZone);
			SetHitPos(hitPos);
		}
		else
		{
			//! Should not happen while the target is in reach; keep a sane hit pos
			//! so the native hit detect has a valid point even without a component.
			SetHitZoneIdx(-1);
			SetHitPos(aimPos);
		}

		#ifdef DM_BOT_DEBUG_FIGHT
		dmBotLog.Debug(pawn.GetCachedName(), "dmBotMeleeCombat: TargetSelection target=" + targetEntity.GetType() + " hitZone=" + hitZone);
		#endif

		SetFinisherType(-1);
	}

	//! Общий хелпер aim-луча: направление rayStart→aimPos нормализуется, итог =
	//! aimPos + dir * DM_MELEE_AIM_RAY_EXTEND (продлевает луч за поверхность тела).
	//! При вырожденном направлении (aimPos == rayStart) возвращает aimPos как есть.
	//! Используется из TargetSelection (rayStart = голова) и EvaluateHit (грудь).
	static vector ExtendRayToAim(vector rayStart, vector aimPos)
	{
		vector dir = aimPos - rayStart;
		if (dir.LengthSq() > 0.0001)
		{
			dir.Normalize();
			return aimPos + dir * DM_MELEE_AIM_RAY_EXTEND;
		}
		return aimPos;
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
