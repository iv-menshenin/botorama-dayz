//! dmE2EBridge — file bridge for E2E auto-tests (hello-world, phase 1).
//!
//! An AI agent drops a JSON job into $profile:dmBotorama/e2e/in/, the bridge
//! executes it on the server and writes the result to e2e/out/, then moves the
//! input to e2e/done/. The bridge is inert unless the e2e/enabled marker file
//! exists, so there is zero overhead when no agent is driving.
//!
//! Ops: ping | spawn | snapshot | clearall (named bots) plus the world/physics
//! probe ops spawnobj | raycast | scanbox | botdump | getpos | setpos | clearobj
//! (named objects) and observe (teleport a connected player). All instantaneous
//! (single-tick).

//! A job (input): an id and the list of steps to run.
class dmE2EJob
{
	string Name;     // job id (= file name)
	float Timeout;   // overall timeout (unused in hello-world, but read)
	autoptr array<ref dmE2EStep> Steps;
}

//! One job step (op plus per-op parameters).
class dmE2EStep
{
	string Op;        // "ping" | "spawn" | "snapshot" | "clearall" | probe op
	string Who;       // bot name (spawn, botdump) / object name (spawnobj)
	vector Pos;       // [x,y,z] world position (spawn, spawnobj, setpos)
	float Yaw;        // orientation in degrees (spawn, spawnobj, setpos)
	string ClassName; // CfgVehicles class (spawnobj)
	vector From;      // raycast start point (world)
	vector To;        // raycast end point (world)
	vector Min;       // scanbox min corner (world)
	vector Max;       // scanbox max corner (world)
	string Obj;       // object name (getpos, setpos, clearobj)
}

//! Per-step outcome.
class dmE2EStepResult
{
	int Index;
	string Op;
	bool Ok;
	string Reason;
	ref array<string> Dump;   // probe-op dump lines (raycast/scanbox/botdump/getpos)
}

//! Snapshot of one named bot.
class dmE2ESnapshot
{
	string Name;
	bool Alive;
	vector Pos;
	string State;
	bool Moving;
}

//! Job result (output): status + step outcomes + snapshot.
class dmE2EResult
{
	string Name;
	string Status;   // "ok" | "error"
	string Error;
	ref array<ref dmE2EStepResult> Steps;
	ref array<ref dmE2ESnapshot> Snapshot;
}

//! Singleton executor: scans e2e/in/*.json and runs one job per tick.
//! Ticked from MissionServer.OnUpdate.
class dmE2EBridge
{
	static ref dmE2EBridge s_Instance;
	private ref map<string, ref dmAISurvivor> m_Named;
	private ref map<string, Object> m_Objects;
	private float m_ScanAccum;

	static dmE2EBridge Get()
	{
		if (!s_Instance)
			s_Instance = new dmE2EBridge();
		return s_Instance;
	}

	void dmE2EBridge()
	{
		m_Named = new map<string, ref dmAISurvivor>();
		m_Objects = new map<string, Object>();
		m_ScanAccum = 0.0;
	}

	void Tick(float dt)
	{
		if (!FileExist(DM_E2E_ENABLED_FILE))
			return;

		m_ScanAccum += dt;
		if (m_ScanAccum < DM_E2E_SCAN_INTERVAL)
			return;
		m_ScanAccum = 0.0;

		if (!FileExist(DM_E2E_IN_DIR))
			return;

		string fileName;
		FileAttr fileAttr;
		FindFileHandle handle = FindFile(DM_E2E_IN_DIR + "/*.json", fileName, fileAttr, FindFileFlags.DIRECTORIES);
		if (fileName != "")
			RunJob(DM_E2E_IN_DIR + "/" + fileName, fileName);
		CloseFindFile(handle);
	}

	void RunJob(string jobPath, string fileName)
	{
		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] job picked up: " + fileName);
		#endif

		dmE2EJob job;
		string loadError;
		if (!JsonFileLoader<dmE2EJob>.LoadFile(jobPath, job, loadError))
		{
			dmBotLog.Error("[E2E] load failed: " + fileName + ": " + loadError);

			dmE2EResult failResult = new dmE2EResult();
			failResult.Steps = new array<ref dmE2EStepResult>();
			failResult.Snapshot = new array<ref dmE2ESnapshot>();
			failResult.Name = BaseName(fileName);
			failResult.Status = "error";
			failResult.Error = loadError;
			SaveResult(failResult);
			MoveToDone(jobPath, fileName);
			return;
		}

		dmE2EResult result = new dmE2EResult();
		result.Steps = new array<ref dmE2EStepResult>();
		result.Snapshot = new array<ref dmE2ESnapshot>();
		result.Name = job.Name;
		if (result.Name == "")
			result.Name = BaseName(fileName);
		result.Status = "ok";

		int i;
		for (i = 0; i < job.Steps.Count(); i++)
		{
			dmE2EStep step = job.Steps[i];
			dmE2EStepResult stepResult = new dmE2EStepResult();
			stepResult.Index = i;
			stepResult.Op = step.Op;
			stepResult.Dump = new array<string>();

			if (step.Op == "ping")
			{
				stepResult.Ok = true;
				stepResult.Reason = "";
			}
			else if (step.Op == "spawn")
			{
				ref dmAISurvivor bot = new dmAISurvivor();
				PlayerBase pawn = bot.Spawn(ResolveWorldPos(step.Pos), Vector(step.Yaw, 0, 0));
				if (pawn)
				{
					m_Named.Insert(step.Who, bot);
					stepResult.Ok = true;
					stepResult.Reason = "spawned";
				}
				else
				{
					stepResult.Ok = false;
					stepResult.Reason = "spawn failed";
				}
			}
			else if (step.Op == "snapshot")
			{
				TStringArray names = m_Named.GetKeyArray();
				int j;
				for (j = 0; j < names.Count(); j++)
				{
					dmAISurvivor snapBot;
					if (!m_Named.Find(names[j], snapBot))
						continue;

					dmE2ESnapshot snap = new dmE2ESnapshot();
					snap.Name = names[j];
					snap.Pos = snapBot.GetPosition();
					if (snapBot.GetPawn() && snapBot.GetPawn().IsAlive())
						snap.Alive = true;
					else
						snap.Alive = false;
					dmBotFSM fsm = snapBot.GetFSM();
					if (fsm && fsm.GetCurrentState())
						snap.State = fsm.GetCurrentState().GetName();
					snap.Moving = false;
					result.Snapshot.Insert(snap);
				}
				stepResult.Ok = true;
				stepResult.Reason = "";
			}
			else if (step.Op == "clearall")
			{
				int cleared = dmAISurvivor.ClearAll();
				m_Named.Clear();
				stepResult.Ok = true;
				stepResult.Reason = "cleared " + cleared;
			}
			else if (step.Op == "spawnobj")
			{
				RunSpawnObj(step, stepResult);
			}
			else if (step.Op == "raycast")
			{
				RunRaycast(step, stepResult);
			}
			else if (step.Op == "scanbox")
			{
				RunScanBox(step, stepResult);
			}
			else if (step.Op == "botdump")
			{
				RunBotDump(step, stepResult);
			}
			else if (step.Op == "getpos")
			{
				RunGetPos(step, stepResult);
			}
			else if (step.Op == "setpos")
			{
				RunSetPos(step, stepResult);
			}
			else if (step.Op == "clearobj")
			{
				RunClearObj(step, stepResult);
			}
			else if (step.Op == "observe")
			{
				RunObserve(step, stepResult);
			}
			else
			{
				stepResult.Ok = false;
				stepResult.Reason = "unknown op";
			}

			result.Steps.Insert(stepResult);

			#ifdef DM_BOT_DEBUG_E2E
			dmBotLog.Debug("[E2E] step " + i + " " + step.Op + " ok=" + stepResult.Ok);
			dmBotLog.Debug("[E2E] step reason=" + stepResult.Reason);
			#endif
		}

		int stepIdx;
		for (stepIdx = 0; stepIdx < result.Steps.Count(); stepIdx++)
		{
			if (!result.Steps[stepIdx].Ok)
				result.Status = "error";
		}

		SaveResult(result);
		MoveToDone(jobPath, fileName);
		ClearNamed();

		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] job done: " + result.Name + " status=" + result.Status);
		#endif
	}

	//! Drop both registries after a job: named bots and named probe objects
	//! (objects are deleted from the world so they never leak across jobs).
	void ClearNamed()
	{
		m_Named.Clear();
		ClearObjects();
	}

	//! Delete every probe object from the world and empty the registry.
	//! Returns the number of objects deleted.
	private int ClearObjects()
	{
		TStringArray keys = m_Objects.GetKeyArray();
		int cleared = 0;
		int i;
		for (i = 0; i < keys.Count(); i++)
		{
			Object obj;
			if (m_Objects.Find(keys[i], obj) && obj)
			{
				EntityAI entity = EntityAI.Cast(obj);
				if (entity)
					entity.DeleteSafe();
				cleared = cleared + 1;
			}
		}
		m_Objects.Clear();
		return cleared;
	}

	//! Resolve a world point: Y==0 (unspecified) -> snap to ground; else keep explicit Y.
	private vector ResolveWorldPos(vector p)
	{
		if (p[1] == 0.0)
			return SnapToGroundExactly(p);
		return p;
	}

	//! Spawn an arbitrary CfgVehicles object at a ground-snapped position,
	//! registered under the step's Who name (in m_Objects, not m_Named).
	private void RunSpawnObj(dmE2EStep step, dmE2EStepResult r)
	{
		Object obj = GetGame().CreateObject(step.ClassName, ResolveWorldPos(step.Pos), false);
		if (obj)
		{
			obj.SetOrientation(Vector(step.Yaw, 0, 0));
			m_Objects.Insert(step.Who, obj);
			r.Ok = true;
			r.Reason = "spawned " + step.ClassName;
		}
		else
		{
			r.Ok = false;
			r.Reason = "spawn failed";
		}
	}

	//! Append a dump line to the step result and echo it to RPT (gated).
	private void AppendDump(dmE2EStepResult r, string line)
	{
		r.Dump.Insert(line);
		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2E] " + line);
		#endif
	}

	//! Raycast between two eye-height points (ground-snapped, raised by
	//! DM_E2E_EYE_HEIGHT). Every hit is dumped (obj/parent/pos/dist/component).
	private void RunRaycast(dmE2EStep step, dmE2EStepResult r)
	{
		vector fromPos = step.From;
		vector toPos = step.To;
		if (fromPos[1] == 0.0)
		{
			fromPos = SnapToGroundExactly(fromPos);
			fromPos[1] = fromPos[1] + DM_E2E_EYE_HEIGHT;
		}
		if (toPos[1] == 0.0)
		{
			toPos = SnapToGroundExactly(toPos);
			toPos[1] = toPos[1] + DM_E2E_EYE_HEIGHT;
		}

		RaycastRVParams params = new RaycastRVParams(fromPos, toPos);
		params.flags = CollisionFlags.ALLOBJECTS;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>();
		DayZPhysics.RaycastRVProxy(params, hits);

		int i;
		for (i = 0; i < hits.Count(); i++)
		{
			RaycastRVResult hit = hits[i];
			string objName = "null";
			if (hit.obj)
				objName = hit.obj.GetType();
			string parentName = "null";
			if (hit.parent)
				parentName = hit.parent.GetType();
			float dist = vector.Distance(fromPos, hit.pos);

			string line = "hit: obj=" + objName + " parent=" + parentName;
			line += " pos=" + hit.pos;
			line += " dist=" + dist;
			line += " component=" + hit.component;
			AppendDump(r, line);
		}

		r.Ok = true;
		if (hits.Count() == 0)
			r.Reason = "0 hits (clear)";
		else
			r.Reason = hits.Count().ToString() + " hits";
	}

	//! Scene box query (static + dynamic) with a per-entity dump line. Static
	//! and dynamic entities are fetched in two separate calls (one flag each).
	private void RunScanBox(dmE2EStep step, dmE2EStepResult r)
	{
		array<EntityAI> dynamics = new array<EntityAI>();
		DayZPlayerUtils.SceneGetEntitiesInBox(ResolveWorldPos(step.Min), ResolveWorldPos(step.Max), dynamics, QueryFlags.DYNAMIC);

		array<EntityAI> statics = new array<EntityAI>();
		DayZPlayerUtils.SceneGetEntitiesInBox(ResolveWorldPos(step.Min), ResolveWorldPos(step.Max), statics, QueryFlags.STATIC);

		int i;
		string line;
		for (i = 0; i < dynamics.Count(); i++)
		{
			line = "ent[D] " + dynamics[i].GetType() + " pos=" + dynamics[i].GetPosition();
			AppendDump(r, line);
		}
		for (i = 0; i < statics.Count(); i++)
		{
			line = "ent[S] " + statics[i].GetType() + " pos=" + statics[i].GetPosition();
			AppendDump(r, line);
		}

		r.Ok = true;
		r.Reason = (dynamics.Count() + statics.Count()).ToString() + " entities";
	}

	//! Dump the named bot's body/motion/brain state as a set of lines.
	private void RunBotDump(dmE2EStep step, dmE2EStepResult r)
	{
		dmAISurvivor bot;
		if (!m_Named.Find(step.Who, bot))
		{
			r.Ok = false;
			r.Reason = "no such bot";
			return;
		}

		PlayerBase pawn = bot.GetPawn();
		if (!pawn)
		{
			r.Ok = false;
			r.Reason = "no pawn";
			return;
		}

		string line = "pos=" + bot.GetPosition();
		AppendDump(r, line);

		line = "alive=" + pawn.IsAlive() + " unconscious=" + pawn.IsUnconscious();
		AppendDump(r, line);

		line = "restrained=" + pawn.IsRestrained() + " bleeding=" + pawn.IsBleeding();
		AppendDump(r, line);

		line = "health=" + pawn.GetHealth01() + " blood=" + pawn.GetHealth("", "Blood") + " shock=" + pawn.GetHealth("", "Shock");
		AppendDump(r, line);

		float stamina = -1.0;
		StaminaHandler sh = pawn.GetStaminaHandler();
		if (sh)
			stamina = sh.GetStaminaNormalized();
		line = "stamina=" + stamina;
		AppendDump(r, line);

		vector vel = GetVelocity(pawn);
		line = "vel=" + vel;
		AppendDump(r, line);

		line = "orient=" + bot.GetOrientation();
		AppendDump(r, line);

		dmBotFSM fsm = bot.GetFSM();
		string fsmName = "none";
		if (fsm && fsm.GetCurrentState())
			fsmName = fsm.GetCurrentState().GetName();
		line = "fsm=" + fsmName;
		AppendDump(r, line);

		line = "fsmIntents=" + bot.GetFSMIntents().Count();
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "dumped";
	}

	//! Report the named probe object's position and yaw.
	private void RunGetPos(dmE2EStep step, dmE2EStepResult r)
	{
		Object obj;
		if (!m_Objects.Find(step.Obj, obj) || !obj)
		{
			r.Ok = false;
			r.Reason = "no such object";
			return;
		}

		string line = "pos=" + obj.GetPosition() + " yaw=" + obj.GetOrientation()[0];
		AppendDump(r, line);

		r.Ok = true;
		r.Reason = "";
	}

	//! Reposition the named probe object (ground-snapped) and set its yaw.
	private void RunSetPos(dmE2EStep step, dmE2EStepResult r)
	{
		Object obj;
		if (!m_Objects.Find(step.Obj, obj) || !obj)
		{
			r.Ok = false;
			r.Reason = "no such object";
			return;
		}

		obj.SetPosition(ResolveWorldPos(step.Pos));
		obj.SetOrientation(Vector(step.Yaw, 0, 0));

		r.Ok = true;
		r.Reason = "";
	}

	//! Delete one probe object (by name) or all of them ("*" or empty).
	private void RunClearObj(dmE2EStep step, dmE2EStepResult r)
	{
		int cleared = 0;
		if (step.Obj == "*" || step.Obj == "")
		{
			cleared = ClearObjects();
		}
		else
		{
			Object obj;
			if (m_Objects.Find(step.Obj, obj) && obj)
			{
				EntityAI entity = EntityAI.Cast(obj);
				if (entity)
					entity.DeleteSafe();
				m_Objects.Remove(step.Obj);
				cleared = 1;
			}
		}

		r.Ok = true;
		r.Reason = "cleared " + cleared;
	}

	//! Teleport the first HUMAN player (observer) to Pos and face Yaw.
	//! AI survivors (dmAISurvivorBase) are PlayerBase too and pollute the registry,
	//! so they must be skipped.
	private void RunObserve(dmE2EStep step, dmE2EStepResult r)
	{
		array<PlayerBase> players = dmEntityRegistry.GetPlayers();
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = players[i];
			if (p && !dmAISurvivorBase.Cast(p))
			{
				p.SetPosition(ResolveWorldPos(step.Pos));
				p.SetOrientation(Vector(step.Yaw, 0, 0));
				r.Ok = true;
				r.Reason = "teleported player";
				return;
			}
		}

		r.Ok = false;
		r.Reason = "no player connected";
	}

	//! Create the parent directory chain of a file path (mirrors dmJsonFile.EnsureDirectory,
	//! but non-generic so it works for versionless e2e structs too).
	private void EnsureDir(string path)
	{
		int lastSlash = path.LastIndexOf("/");
		if (lastSlash < 0)
			return;

		TStringArray comps = new TStringArray();
		path.Substring(0, lastSlash).Split("/", comps);

		int startFrom = 0;
		string dir = "";
		if (comps.Count() > 0 && comps[0] == "$profile:")
		{
			dir = "$profile:";
			startFrom = 1;
		}

		for (int i = startFrom; i < comps.Count(); i++)
		{
			if (dir != "")
				dir += "/";
			dir += comps[i];
			if (!FileExist(dir))
				MakeDirectory(dir);
		}
	}

	//! Write the result to e2e/out/<Name>.result.json (creating the directory).
	private void SaveResult(dmE2EResult result)
	{
		string outPath = DM_E2E_OUT_DIR + "/" + result.Name + ".result.json";
		EnsureDir(outPath);
		string saveError;
		if (!JsonFileLoader<dmE2EResult>.SaveFile(outPath, result, saveError))
			dmBotLog.Error("[E2E] write result failed: " + outPath + ": " + saveError);
	}

	//! Move the processed input to e2e/done/ (copy then delete).
	private void MoveToDone(string jobPath, string fileName)
	{
		string donePath = DM_E2E_DONE_DIR + "/" + fileName;
		EnsureDir(donePath);
		CopyFile(jobPath, donePath);
		DeleteFile(jobPath);
	}

	//! Job id from a file name (strip the ".json" suffix).
	private string BaseName(string fileName)
	{
		string name = fileName;
		if (name.Length() > 5)
			name = name.Substring(0, name.Length() - 5);
		return name;
	}
}
