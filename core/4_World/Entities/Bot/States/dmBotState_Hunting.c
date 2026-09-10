//! dmBotState_Hunting — track down a target the bot knows about but can't see.
//!
//! INTERRUPTIBLE search state: the bot knows a target (heard a noise / saw it a
//! moment ago) but !m_HasLOS. It alternates between a random point in the spread
//! circle around the target's last known position and clearing nearby buildings
//! (that could hide the target). EXITs as soon as a hostile target appears
//! (combat takes over) or the hunted target becomes visible.
class dmBotState_Hunting : dmBotState
{
	ref dmTarget m_Target;
	ref dmBotIntent_MoveTo m_Move;
	Building m_CurrentBuilding;
	vector m_SearchCenter;
	bool m_WantBuilding;

	override dmBotStateKind GetKind()
	{
		return dmBotStateKind.INTERRUPTIBLE;
	}

	override bool CanEnter()
	{
		return GetOwner().GetHuntTarget() != null;
	}

	override void OnEntry(dmBotState from)
	{
		super.OnEntry(from);

		m_Target = null;
		m_Move = null;
		m_CurrentBuilding = null;
		m_SearchCenter = vector.Zero;
		m_WantBuilding = false;

		ResolveTarget(GetOwner());

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Hunting.entry");
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		dmAISurvivorBase pawn = dmAISurvivorBase.Cast(bot.GetPawn());
		if (!pawn)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Hunting нет пешки");
			#endif
			return EXIT;
		}

		if (!ResolveTarget(bot))
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Hunting нет цели");
			#endif
			return EXIT;
		}

		dmTarget hostile = bot.GetHostileTarget();
		if ( hostile && hostile != m_Target )
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Hunting Враг обнаружен — бой вытесняет охоту");
			#endif
			return EXIT;
		}

		// Цель стала видимой — охота кончилась.
		if (m_Target.m_HasLOS)
		{
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Hunting цель видима, охота окончена");
			#endif
			return EXIT;
		}

		// Новая позиция цели (новый шум) — искать заново.
		if (m_SearchCenter != m_Target.m_LastPosition)
		{
			m_SearchCenter = m_Target.m_LastPosition;
			ResetMove(bot);
		}

		EnsureMove(bot);

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		if (m_Move) { m_Move.Finish(); m_Move = null; }
		m_CurrentBuilding = null;

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Hunting.exit");
		#endif
	}

	//! Держит m_Target актуальной охотящейся целью; false — охотиться не на кого.
	bool ResolveTarget(dmAISurvivor bot)
	{
		if (m_Target)
		{
			EntityAI e = m_Target.m_Entity;
			if (e && !e.IsAlive())
				m_Target = null;
		}
		if (!m_Target)
		{
			m_Target = bot.GetHuntTarget();
			if (!m_Target)
				return false;
			m_SearchCenter = m_Target.m_LastPosition;
			ResetMove(bot);
		}
		return true;
	}

	void ResetMove(dmAISurvivor bot)
	{
		if (m_Move) { m_Move.Finish(); m_Move = null; }
		m_CurrentBuilding = null;
		m_WantBuilding = false;
	}

	//! Чередование: случайная точка в круге разброса → здание с дверьми → повтор.
	void EnsureMove(dmAISurvivor bot)
	{
		if (m_Move && (m_Move.IsFinished() || m_Move.IsExpired()))
		{
			if (m_CurrentBuilding)
			{
				bot.GetExplorer().MarkVisited(m_CurrentBuilding);
				m_CurrentBuilding = null;
				m_WantBuilding = false;
			}
			else
			{
				m_WantBuilding = true;
			}
			m_Move = null;
		}
		if (m_Move)
			return;

		if (m_WantBuilding)
		{
			Building building = bot.GetExplorer().GetNearestBuildingWithDoors(bot, DM_EXPLORE_EXPLORE_RADIUS);
			if (building)
			{
				m_CurrentBuilding = building;
				m_WantBuilding = false;
				m_Move = new dmBotIntent_MoveTo();
				m_Move.m_Goal = building.GetPosition();
				m_Move.m_ReachDistance = DM_EXPLORE_BUILDING_REACH;
				m_Move.m_Priority = dmBotIntentPriority.DESIRABLE;
				bot.AddFSMIntent(m_Move);

				#ifdef DM_BOT_DEBUG_FSM
				dmBotLog.Debug("[FSM] Hunting: иду к зданию " + building.GetType());
				#endif
				return;
			}
			// Зданий нет — новая случайная точка.
			m_WantBuilding = false;
		}

		vector goal = RollSearchPoint();
		m_Move = new dmBotIntent_MoveTo();
		m_Move.m_Goal = goal;
		m_Move.m_ReachDistance = DM_PATROL_REACH_DISTANCE;
		m_Move.m_Priority = dmBotIntentPriority.DESIRABLE;
		bot.AddFSMIntent(m_Move);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Hunting: случайная точка " + goal);
		#endif
	}

	//! Случайная точка в круге радиуса spread с центром m_SearchCenter, но не
	//! ближе spread/3 к центру (известной позиции цели).
	vector RollSearchPoint()
	{
		float spread = m_Target.m_LastPositionSpread;
		if (spread < DM_HUNT_SPREAD_MIN)
			spread = DM_HUNT_SPREAD_MIN;

		float ang = Math.RandomFloat01() * Math.PI2;
		float minR = spread / 3.0;
		float r = minR + Math.RandomFloat01() * (spread - minR);

		vector goal = m_SearchCenter;
		goal[0] = goal[0] + Math.Cos(ang) * r;
		goal[2] = goal[2] + Math.Sin(ang) * r;

		return goal;
	}
}
