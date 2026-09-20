//! dmBotState_Travel — кочёвка между локациями (Nomad).
//! Вход: «делать больше нечего» ИЛИ бот вне какой-либо локации. Выбирает ближайшую
//! непосещённую локацию (dmExplorer.GetNearestUnvisitedLocation), бежит к ней
//! (прибытие = GetArrivalRadius типа). По прибытии — ArriveAtLocation + сброс флагов.
class dmBotState_Travel : dmBotState
{
	ref dmBotIntent_MoveTo m_Move;

	override dmBotStateKind GetKind() { return dmBotStateKind.INTERRUPTIBLE; }

	override bool CanEnter()
	{
		dmAISurvivor bot = GetOwner();
		dmExplorer e = bot.GetExplorer();
		if (e.IsInTransit())
			return false;
		if (!e.IsNothingToDo() && dmWorldPOIRegistry.Get().GetLocationAt(bot.GetPosition()) != null)
			return false;
		return e.GetNearestUnvisitedLocation(bot) != null;
	}

	override void OnEntry(dmBotState from)
	{
		m_Move = null;
		dmAISurvivor bot = GetOwner();
		dmExplorer e = bot.GetExplorer();
		dmWorldPoiLocation dest = e.GetDestination();
		if (!dest)
			dest = e.GetNearestUnvisitedLocation(bot);
		if (!dest)
			return;

		e.SetDestination(dest);
		e.SetInTransit(true);

		vector goal = dest.Position;
		vector sampled;
		if (bot.SampleNavmesh(goal, sampled))
			goal = sampled;
		else
			goal[1] = GetGame().SurfaceY(goal[0], goal[2]);

		m_Move = new dmBotIntent_MoveTo();
		m_Move.m_Goal = goal;
		m_Move.m_ReachDistance = dmWorldPOIRegistry.GetArrivalRadius(dest.Type);
		m_Move.m_Priority = dmBotIntentPriority.DESIRABLE;
		bot.AddFSMIntent(m_Move);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("[FSM] Travel.entry -> " + dest.Name + " (" + dest.Type + ") goal=" + goal);
		#endif
	}

	override int OnUpdate(float pDt)
	{
		dmAISurvivor bot = GetOwner();
		dmExplorer e = bot.GetExplorer();

		if (bot.GetHostileTarget() != null)
			return EXIT;

		if (!m_Move)
			return EXIT;

		if (m_Move.IsFailed())
		{
			dmWorldPoiLocation dead = e.GetDestination();
			if (dead)
				e.RememberLocation(dead.Id);   // анти-цикл: не выбирать недостижимую снова
			e.ClearDestination();
			e.SetInTransit(false);
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Travel.failed, give up destination");
			#endif
			return EXIT;
		}

		if (m_Move.IsFinished())
		{
			e.SetInTransit(false);
			dmWorldPoiLocation arrived = e.GetDestination();
			e.ClearDestination();
			if (arrived)
				e.ArriveAtLocation(arrived);
			#ifdef DM_BOT_DEBUG_FSM
			dmBotLog.Debug("[FSM] Travel.arrived");
			#endif
			return EXIT;
		}

		if (m_Move.IsExpired())
		{
			e.ClearDestination();
			e.SetInTransit(false);   // ретрай позже (локация НЕ помечается посещённой)
			return EXIT;
		}

		return CONTINUE;
	}

	override void OnExit(dmBotState to)
	{
		if (m_Move) { m_Move.Finish(); m_Move = null; }
		GetOwner().GetExplorer().SetInTransit(false);
	}
}
