//! dmBotState_Kamping — camp a building's exit while the hostile target hides
//! inside (out of sight). Inherits target resolution, combat entry/exit and
//! PREEMPTIVE kind from dmBotState_Shooting, but replaces the aim/fire loop with
//! a search→hide→wait pipeline driven by dmCampCoverFinder (cached, async).
//!
//! SEARCH: find the building near the target, its external doors, then a cover
//! point across from the nearest door (all async, one step per tick).
//! HIDE: walk to the cover point while keeping the weapon on the door.
//! WAIT: crouch behind cover, keep the weapon raised at the door (sticky) and
//! hold until the target shows itself — the top-level m_HasLOS check then EXITs
//! back to Shooting.

enum dmKampPhase
{
	SEARCH,
	HIDE,
	WAIT
};

class dmBotState_Kamping : dmBotState_Shooting
{
	int m_Phase;
	Building m_Building;
	int m_DoorIdx;
	vector m_DoorPos;          // XZ двери + высота груди (для HoldLook)
	ref array<int> m_Doors;
	bool m_DoorsReady;
	ref array<vector> m_CoverPoints;
	bool m_CoverReady;
	vector m_CoverPoint;
	ref dmBotIntent_MoveTo m_Move;
	ref dmBotIntent_Stance m_Stance;
	ref dmBotIntent_HoldLook m_DoorLook;

	override dmBotCondition CreateGate()
	{
		return dmBotConditions.Kamping(true);
	}

	override bool CanEnter()
	{
		dmTarget t = GetOwner().GetHostileTarget();
		if ( !t || !t.m_Entity )
		{
			#ifdef DM_BOT_DEBUG_FSM_KAMPING
			dmBotLog.Debug("[FSM] Kamping: CanEnter=false (нет hostile-цели)");
			#endif
			return false;
		}
		bool ok = super.CanEnter();
		#ifdef DM_BOT_DEBUG_FSM_KAMPING
		dmBotLog.Debug("[FSM] Kamping: CanEnter=" + ok + " видима=" + t.IsVisible() + " цель=" + t.m_Entity);
		#endif
		return ok;
	}

	override void OnEntry(dmBotState from)
	{
		super.OnEntry(from);

		m_Phase = dmKampPhase.SEARCH;
		m_Building = null;
		m_DoorsReady = false;
		m_CoverReady = false;
		m_Move = null;
		m_Stance = null;
		m_DoorLook = null;

		#ifdef DM_BOT_DEBUG_FSM_KAMPING
		dmBotLog.Debug("[FSM] Kamping.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		if (!m_TargetEntity || !m_TargetEntity.IsAlive()) ResolveTarget();
		if (!m_TargetEntity) return EXIT;

		dmTarget t = bot.FindTarget(m_TargetEntity);
		if (t && t.IsVisible())
		{
			#ifdef DM_BOT_DEBUG_FSM_KAMPING
			dmBotLog.Debug("[FSM] Kamping: цель видна — EXIT (уступить Shooting)");
			#endif
			return EXIT; // цель видна — уступить Shooting
		}

		if (!dmAISurvivorBase.IsPointUnderRoofBuilding(m_TargetEntity.GetPosition()))
			return EXIT; // цель вышла из здания
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn && dmAISurvivorBase.IsPointUnderRoofBuilding(pawn.GetPosition()))
			return EXIT; // бот вошёл в здание
		if (t && !bot.CanReach(t))
			return EXIT; // цель вне зоны

		if (m_Phase == dmKampPhase.SEARCH)
			return StepSearch(bot);
		if (m_Phase == dmKampPhase.HIDE)
			return StepHide(bot);
		return StepWait(bot); // WAIT
	}

	//! SEARCH: здание → внешние двери → укрытие (всё async, не блокирует тик).
	int StepSearch(dmAISurvivor bot)
	{
		if (!m_Building)
		{
			m_Building = dmLiveBuildingRegistry.Get().GetNearestBuildingWithDoors(m_TargetEntity.GetPosition(), DM_CAMP_BUILDING_SEARCH_RADIUS, null);
			if (!m_Building)
			{
				dmBotLog.Error("[Camp] Kamping: no building near target — EXIT");
				return EXIT;
			}

			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] Kamping: SEARCH building=" + m_Building.GetType());
			#endif
		}

		if (!m_DoorsReady)
		{
			m_DoorsReady = dmCampCoverFinder.Get().QueryExternalDoors(m_Building, m_Doors);
			if (!m_DoorsReady)
				return CONTINUE; // job в очереди — ждём следующий тик

			if (m_Doors.Count() == 0)
			{
				dmBotLog.Error("[Camp] Kamping: no external doors — EXIT");
				return EXIT;
			}

			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] Kamping: doors count=" + m_Doors.Count());
			#endif

			PickDoor(bot);
		}

		if (!m_CoverReady)
		{
			m_CoverReady = dmCampCoverFinder.Get().QueryCover(bot, m_Building, m_DoorIdx, m_CoverPoints);
			if (!m_CoverReady)
			{
				EnsureDoorLook(bot); // дверь известна — держим взгляд, пока ищем укрытие
				return CONTINUE;
			}

			if (m_CoverPoints.Count() == 0)
			{
				dmBotLog.Error("[Camp] Kamping: no cover points — EXIT");
				return EXIT;
			}

			m_CoverPoint = m_CoverPoints[0];
			m_Phase = dmKampPhase.HIDE;

			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] Kamping: SEARCH -> HIDE cover=" + m_CoverPoint);
			#endif
		}

		return CONTINUE;
	}

	//! HIDE: идти к укрытию, глядя на дверь.
	int StepHide(dmAISurvivor bot)
	{
		EnsureDoorLook(bot);

		if (m_Move && m_Move.IsFinished())
		{
			if (m_Move.IsFailed())
			{
				dmBotLog.Error("[Camp] Kamping: cover unreachable — EXIT");
				return EXIT;
			}
			m_Move = null;
			m_Phase = dmKampPhase.WAIT;

			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] Kamping: HIDE -> WAIT");
			#endif
			return CONTINUE;
		}

		if (m_Move && m_Move.IsExpired())
			m_Move = null;
		if (!m_Move)
		{
			m_Move = new dmBotIntent_MoveTo();
			m_Move.m_Goal = m_CoverPoint;
			m_Move.m_ReachDistance = 1.0;
			m_Move.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Move.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Move);
		}
		return CONTINUE;
	}

	//! WAIT: сидеть за укрытием, оружие (sticky) на дверь.
	int StepWait(dmAISurvivor bot)
	{
		EnsureStance(bot);
		EnsureDoorLook(bot);

		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (pawn)
			pawn.RaiseWeaponSticky(DM_CAMP_HOLD_SECONDS);

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		#ifdef DM_BOT_DEBUG_FSM_KAMPING
		dmBotLog.Debug("[FSM] Kamping.exit");
		#endif
		if (m_Move) m_Move.Finish();
		if (m_Stance) m_Stance.Finish();
		if (m_DoorLook) m_DoorLook.Finish();
		super.OnExit(to); // Shooting.OnExit опустит оружие и очистит hold
	}

	//! Выбрать внешнюю дверь, чья ось «наружу» смотрит на бота (максимум facing),
	//! и её точку (XZ + высота груди). Дегенерат/пусто → safety m_Doors[0].
	void PickDoor(dmAISurvivor bot)
	{
		vector botPos = bot.GetPosition();
		vector center;
		vector doorPos;
		vector outDir;
		vector botDir;
		float lenSq;
		float botLenSq;
		float facing;
		float bestScore = -2.0;
		float groundY;
		int i;

		center = m_Building.GetWorldPosition();
		center[1] = 0.0;

		m_DoorIdx = -1;
		for (i = 0; i < m_Doors.Count(); i++)
		{
			doorPos = m_Building.GetDoorSoundPos(m_Doors[i]);
			doorPos[1] = 0.0;

			outDir = doorPos - center;
			lenSq = outDir.LengthSq();
			if (lenSq < 0.001)
			{
				#ifdef DM_BOT_DEBUG_CAMP
				dmBotLog.Debug("[Camp] Kamping: door idx=" + m_Doors[i] + " degenerate outDir");
				#endif
				continue;
			}
			outDir.Normalize();

			botDir = botPos - doorPos;
			botDir[1] = 0.0;
			botLenSq = botDir.LengthSq();
			if (botLenSq < 0.001)
			{
				m_DoorIdx = m_Doors[i];
				bestScore = 2.0;
				facing = 2.0;
			}
			else
			{
				botDir.Normalize();
				facing = outDir[0] * botDir[0] + outDir[2] * botDir[2];
				if (facing > bestScore)
				{
					bestScore = facing;
					m_DoorIdx = m_Doors[i];
				}
			}

			#ifdef DM_BOT_DEBUG_CAMP
			dmBotLog.Debug("[Camp] Kamping: door idx=" + m_Doors[i] + " doorPos=" + doorPos);
			dmBotLog.Debug("[Camp] Kamping: door botPos=<" + botPos[0] + "," + botPos[2] + "> facing=" + facing);
			#endif
		}

		if (m_DoorIdx < 0)
			m_DoorIdx = m_Doors[0];

		doorPos = m_Building.GetDoorSoundPos(m_DoorIdx);
		groundY = GetGame().SurfaceY(doorPos[0], doorPos[2]);
		doorPos[1] = groundY + DM_CAMP_DOOR_EYE_HEIGHT;
		m_DoorPos = doorPos;

		#ifdef DM_BOT_DEBUG_CAMP
		dmBotLog.Debug("[Camp] Kamping: door chosen idx=" + m_DoorIdx + " of=" + m_Doors.Count() + " facing=" + bestScore);
		#endif
	}

	void EnsureStance(dmAISurvivor bot)
	{
		if (m_Stance && (m_Stance.IsFinished() || m_Stance.IsExpired()))
			m_Stance = null;
		if (!m_Stance)
		{
			m_Stance = new dmBotIntent_Stance();
			m_Stance.m_Stance = DayZPlayerConstants.STANCEIDX_CROUCH;
			m_Stance.m_Priority = dmBotIntentPriority.CRITICAL;
			m_Stance.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_Stance);
		}
	}

	//! Держать взгляд (и ствол) на двери; m_Point обновляется каждый тик.
	void EnsureDoorLook(dmAISurvivor bot)
	{
		if (m_DoorLook && (m_DoorLook.IsFinished() || m_DoorLook.IsExpired()))
			m_DoorLook = null;
		if (!m_DoorLook)
		{
			m_DoorLook = new dmBotIntent_HoldLook();
			m_DoorLook.m_Turn = dmBotLookTurn.FULL;
			m_DoorLook.m_Priority = dmBotIntentPriority.CRITICAL;
			m_DoorLook.m_Concurrency = dmBotIntentConcurrency.PARALLEL;
			bot.AddFSMIntent(m_DoorLook);
		}
		m_DoorLook.m_Point = m_DoorPos;
	}
}
