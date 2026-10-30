//! dmE2EClientBridge — CLIENT twin of dmE2EBridge for E2E auto-tests.
//!
//! The server bridge (dmE2EBridge) dumps the server-authoritative view of bots.
//! This bridge runs on the OBSERVER CLIENT and dumps the CLIENT-side picture of
//! the same world: net-synced bot state (empty hands after a drop, "can it be
//! tied" observable predicates, pose), items on the ground (GetObjectsAtPosition),
//! and client raycasts (view/LOD geometry, what the player actually sees).
//!
//! Protocol mirrors dmE2EBridge but writes to a SEPARATE directory
//! $profile:dmBotorama/e2e-client/ (in/out/done), gated by the e2e-client/enabled
//! marker file. Jobs reuse the shared dmE2EJob/dmE2EStep/dmE2EResult structs (same
//! module). All ops here are single-tick dumps — no deferred automaton.
//!
//! Tick is a self-rescheduling CallLater (same pattern as dmE2EBridge.E2ETick) —
//! not MissionGameplay.OnUpdate, which is unusable across modules in 1.30 (two
//! modded MissionGameplay blocks both overriding OnUpdate silently drop one).
//!
//! Ops: ping | botdump | scanobj | slotdump | raycast.

//! Singleton executor: picks up e2e-client/in/*.json (one job per tick) and runs
//! it immediately. Ticked by its own self-rescheduling CallLater (Get() schedules
//! the first one-shot; each tick re-schedules the next one explicitly).
class dmE2EClientBridge
{
	static ref dmE2EClientBridge s_Instance;
	static bool s_TickLogged = false;
	static int s_TickCount = 0;

	//! Self-scheduled tick cadence: interval (ms) and per-tick delta (s). Mirrors
	//! the server bridge (1 Hz); the scan interval below paces the in/-scan.
	static const int DM_E2E_CLIENT_TICK_INTERVAL_MS = 1000;
	static const float DM_E2E_CLIENT_TICK_DT = 1.0;

	private float m_ScanAccum;

	static dmE2EClientBridge Get()
	{
		if (!s_Instance)
		{
			s_Instance = new dmE2EClientBridge();
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(s_Instance.E2ETick, DM_E2E_CLIENT_TICK_INTERVAL_MS, false);
		}
		return s_Instance;
	}

	void dmE2EClientBridge()
	{
		m_ScanAccum = 0.0;
	}

	//! Self-scheduled tick entry (CallLater target). Logs once on the first tick
	//! and a heartbeat every 10th tick, then re-schedules the next one explicitly.
	void E2ETick()
	{
		s_TickCount = s_TickCount + 1;

		if (!s_TickLogged)
		{
			s_TickLogged = true;
			#ifdef DM_BOT_DEBUG_E2E
			dmBotLog.Debug("[E2EC] bridge tick active (CallLater)");
			#endif
		}

		#ifdef DM_BOT_DEBUG_E2E
		if (s_TickCount % 10 == 0)
			dmBotLog.Debug("[E2EC] tick N=" + s_TickCount);
		#endif

		Tick(DM_E2E_CLIENT_TICK_DT);

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(s_Instance.E2ETick, DM_E2E_CLIENT_TICK_INTERVAL_MS, false);
	}

	void Tick(float dt)
	{
		if (!FileExist(DM_E2E_CLIENT_ENABLED_FILE))
		{
			#ifdef DM_BOT_DEBUG_E2E
			if (s_TickCount % 10 == 0)
				dmBotLog.Debug("[E2EC] enabled missing");
			#endif
			return;
		}

		m_ScanAccum += dt;
		if (m_ScanAccum < DM_E2E_CLIENT_SCAN_INTERVAL)
			return;
		m_ScanAccum = 0.0;

		if (!FileExist(DM_E2E_CLIENT_IN_DIR))
			return;

		RunNextJob();
	}

	//! Pick the first *.json job in in/ and run it. Only one job per tick (a job
	//! deletes its input while we iterate the directory, so we stop after one).
	private void RunNextJob()
	{
		string fileName;
		FileAttr fileAttr;
		FindFileHandle handle = FindFile(DM_E2E_CLIENT_IN_DIR + "/*.json", fileName, fileAttr, FindFileFlags.DIRECTORIES);

		if (fileName == "")
		{
			CloseFindFile(handle);
			return;
		}

		RunJob(DM_E2E_CLIENT_IN_DIR + "/" + fileName, fileName);
		CloseFindFile(handle);
	}

	//! Load a job, run every step immediately, fold the outcomes into the result,
	//! write out/<Name>.result.json and move the input to done/.
	private void RunJob(string jobPath, string fileName)
	{
		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2EC] job picked up: " + fileName);
		#endif

		dmE2EJob job;
		string loadError;
		if (!JsonFileLoader<dmE2EJob>.LoadFile(jobPath, job, loadError))
		{
			dmBotLog.Error("[E2EC] load failed: " + fileName + ": " + loadError);

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
			RunInstantOp(step, stepResult);
			result.Steps.Insert(stepResult);
			LogStep(stepResult);
		}

		int j;
		for (j = 0; j < result.Steps.Count(); j++)
		{
			if (!result.Steps[j].Ok)
			{
				result.Status = "error";
				break;
			}
		}

		SaveResult(result);
		MoveToDone(jobPath, fileName);

		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2EC] job done: " + result.Name + " status=" + result.Status);
		#endif
	}

	//! Dispatch a single-tick op to its runner.
	private void RunInstantOp(dmE2EStep step, dmE2EStepResult r)
	{
		if (step.Op == DM_E2E_CLIENT_OP_PING)
		{
			RunPing(step, r);
		}
		else if (step.Op == DM_E2E_CLIENT_OP_BOTDUMP)
		{
			RunBotDump(step, r);
		}
		else if (step.Op == DM_E2E_CLIENT_OP_SCANOBJ)
		{
			RunScanObj(step, r);
		}
		else if (step.Op == "slotdump")
		{
			RunSlotDump(step, r);
		}
		else if (step.Op == DM_E2E_CLIENT_OP_RAYCAST)
		{
			RunRaycast(step, r);
		}
		else
		{
			r.Ok = false;
			r.Reason = "unknown op";
		}
	}

	//! "ping" — prove the client channel is alive (and where the observer stands).
	private void RunPing(dmE2EStep step, dmE2EStepResult r)
	{
		r.Ok = true;
		r.Reason = "pong";

		PlayerBase lp = GetGame().GetPlayer();
		string line = "alive";
		if (lp)
			line = "alive clientPos=" + lp.GetPosition();
		AppendDump(r, line);
	}

	//! "botdump" — dump the CLIENT-side picture of every AI_REMOTE pawn. Names are
	//! not replicated to the client, so bots are found via the entity registry
	//! (filter dmAISurvivorBase.Cast(p) != null), optionally narrowed to a radius
	//! around step.Pos (or the local player when Pos is unspecified). Radius == 0
	//! dumps all of them. Only observable predicates are read (net-vars/pose/items)
	//! — never CanBeRestrained()/GetActionManager() (null on AI_REMOTE) and never
	//! the server-only dmAISurvivor brain.
	private void RunBotDump(dmE2EStep step, dmE2EStepResult r)
	{
		bool filter = step.Radius > 0.0;
		vector center = vector.Zero;
		if (filter)
		{
			center = step.Pos;
			if (center[0] == 0.0 && center[1] == 0.0 && center[2] == 0.0)
			{
				PlayerBase lp = GetGame().GetPlayer();
				if (lp)
					center = lp.GetPosition();
			}
			else
			{
				center = ResolveWorldPos(center);
			}
		}

		array<PlayerBase> players = dmEntityRegistry.GetPlayers();
		int i;
		int dumped = 0;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = players[i];
			if (!p)
				continue;
			if (dmAISurvivorBase.Cast(p) == null)
				continue;
			if (filter && vector.Distance(p.GetPosition(), center) > step.Radius)
				continue;

			DumpBot(p, r);
			dumped = dumped + 1;
		}

		r.Ok = true;
		r.Reason = "dumped " + dumped;
	}

	//! Dump one AI_REMOTE pawn's client-observable state as lines.
	private void DumpBot(PlayerBase p, dmE2EStepResult r)
	{
		string line = "bot pos=" + p.GetPosition();
		AppendDump(r, line);

		line = "alive=" + p.IsAlive() + " unconscious=" + p.IsUnconscious() + " stateOnly=" + p.IsUnconsciousStateOnly();
		AppendDump(r, line);

		EntityAI ih = p.GetItemInHands();
		string hands = "null";
		if (ih)
			hands = ih.GetType();
		line = "restrained=" + p.IsRestrained() + " inHands=" + hands;
		AppendDump(r, line);

		HumanMovementState ms = new HumanMovementState();
		p.GetMovementState(ms);
		int stance = ms.m_iStanceIdx;
		if (stance >= DayZPlayerConstants.STANCEIDX_RAISED)
			stance = stance - DayZPlayerConstants.STANCEIDX_RAISED;
		line = "stance=" + stance + " cmd=" + ms.m_CommandTypeId;
		AppendDump(r, line);
	}

	//! "scanobj" — dump items at step.Pos within Radius (GetObjectsAtPosition),
	//! tagging whether each is lying on the ground (GetHierarchyRootPlayer()==null).
	//! Used to verify a dropped machete is actually on the ground.
	private void RunScanObj(dmE2EStep step, dmE2EStepResult r)
	{
		vector center = ResolveWorldPos(step.Pos);
		float radius = step.Radius;
		if (radius <= 0.0)
			radius = 1.0;

		array<Object> objs = new array<Object>();
		array<CargoBase> cargos = new array<CargoBase>();
		GetGame().GetObjectsAtPosition(center, radius, objs, cargos);

		int i;
		Object obj;
		EntityAI ai;
		string onGround;
		string line;
		for (i = 0; i < objs.Count(); i++)
		{
			obj = objs[i];
			onGround = "false";
			ai = EntityAI.Cast(obj);
			if (ai && ai.GetHierarchyRootPlayer() == null)
				onGround = "true";
			line = "obj type=" + obj.GetType() + " pos=" + obj.GetPosition() + " onGround=" + onGround;
			AppendDump(r, line);
		}

		r.Ok = true;
		r.Reason = objs.Count().ToString() + " objects";
	}

	//! "slotdump" — дамп НАДЕТОГО с точки зрения КЛИЕНТА: FindAttachment по слотам
	//! одежды (BODY/LEGS/FEET/BACK/VEST/HIPS/HEADGEAR/MASK/EYEWEAR/GLOVES/ARMBAND/
	//! SHOULDER/MELEE) + GetItemInHands, с именами типов. Это то, что клиент реально
	//! «видит» надетым на пешке. Ищет бота как botdump (реестр + фильтр
	//! dmAISurvivorBase.Cast + радиус вокруг Pos/локального игрока).
	private void RunSlotDump(dmE2EStep step, dmE2EStepResult r)
	{
		bool filter = step.Radius > 0.0;
		vector center = vector.Zero;
		if (filter)
		{
			center = step.Pos;
			if (center[0] == 0.0 && center[1] == 0.0 && center[2] == 0.0)
			{
				PlayerBase lp = GetGame().GetPlayer();
				if (lp)
					center = lp.GetPosition();
			}
			else
			{
				center = ResolveWorldPos(center);
			}
		}

		array<PlayerBase> players = dmEntityRegistry.GetPlayers();
		int i;
		int dumped = 0;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = players[i];
			if (!p)
				continue;
			if (dmAISurvivorBase.Cast(p) == null)
				continue;
			if (filter && vector.Distance(p.GetPosition(), center) > step.Radius)
				continue;

			DumpSlots(p, r);
			dumped = dumped + 1;
		}

		r.Ok = true;
		r.Reason = "dumped " + dumped;
	}

	//! Дамп одетых слотов одного AI_REMOTE бота (клиентская картина).
	private void DumpSlots(PlayerBase p, dmE2EStepResult r)
	{
		string line = "bot pos=" + p.GetPosition();
		AppendDump(r, line);

		array<int> slots = {
			InventorySlots.BODY,
			InventorySlots.LEGS,
			InventorySlots.BACK,
			InventorySlots.VEST,
			InventorySlots.HIPS,
			InventorySlots.FEET,
			InventorySlots.HEADGEAR,
			InventorySlots.MASK,
			InventorySlots.EYEWEAR,
			InventorySlots.GLOVES,
			InventorySlots.ARMBAND,
			InventorySlots.SHOULDER,
			InventorySlots.MELEE
		};

		int i;
		for (i = 0; i < slots.Count(); i++)
		{
			int slotId = slots[i];
			EntityAI att = null;
			if (p.GetInventory().HasAttachmentSlot(slotId))
				att = p.GetInventory().FindAttachment(slotId);
			string attName = "none";
			if (att)
				attName = att.GetType();
			AppendDump(r, "slot=" + InventorySlots.GetSlotName(slotId) + " item=" + attName);
		}

		EntityAI ih = p.GetItemInHands();
		string hands = "none";
		if (ih)
			hands = ih.GetType();
		AppendDump(r, "hands=" + hands);
	}

	//! "raycast" — client raycast (view/LOD geometry) between two points, like the
	//! server raycast but with the geometry the player sees. Each hit is dumped.
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

		RaycastRVParams params = new RaycastRVParams(fromPos, toPos, null);
		params.type = ResolveIntersectType(step.Type);
		params.flags = ResolveCollisionFlags(step.Flags);
		params.radius = step.RayRadius;
		params.sorted = step.Sorted;
		params.groundOnly = step.GroundOnly;

		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>();
		DayZPhysics.RaycastRVProxy(params, hits);

		int i;
		RaycastRVResult hit;
		string objName;
		string parentName;
		string surfName;
		string surfType;
		float dist;
		string line;
		for (i = 0; i < hits.Count(); i++)
		{
			hit = hits[i];
			objName = "null";
			if (hit.obj)
				objName = hit.obj.GetType();
			parentName = "null";
			if (hit.parent)
				parentName = hit.parent.GetType();
			surfName = "null";
			if (hit.surface)
				surfName = hit.surface.GetName();
			surfType = "null";
			if (hit.surface)
				surfType = hit.surface.GetSurfaceType();
			dist = vector.Distance(fromPos, hit.pos);

			line = "hit: obj=" + objName + " parent=" + parentName;
			line += " pos=" + hit.pos;
			line += " dist=" + dist;
			line += " component=" + hit.component;
			line += " surf=" + surfName;
			line += " surfType=" + surfType;
			AppendDump(r, line);
		}

		r.Ok = true;
		if (hits.Count() == 0)
			r.Reason = "0 hits (clear)";
		else
			r.Reason = hits.Count().ToString() + " hits";
	}

	//! Map the raycast "Type" string to an ObjIntersect enum; empty -> ObjIntersectView.
	private int ResolveIntersectType(string t)
	{
		if (t == "fire")
			return ObjIntersectFire;
		if (t == "view")
			return ObjIntersectView;
		if (t == "geom")
			return ObjIntersectGeom;
		if (t == "ifire")
			return ObjIntersectIFire;
		if (t == "none")
			return ObjIntersectNone;
		return ObjIntersectView;
	}

	//! Map the raycast "Flags" string to a CollisionFlags enum; empty -> ALLOBJECTS.
	private CollisionFlags ResolveCollisionFlags(string f)
	{
		if (f == "allobjects")
			return CollisionFlags.ALLOBJECTS;
		if (f == "nearestcontact")
			return CollisionFlags.NEARESTCONTACT;
		if (f == "firstcontact")
			return CollisionFlags.FIRSTCONTACT;
		if (f == "onlystatic")
			return CollisionFlags.ONLYSTATIC;
		if (f == "onlydynamic")
			return CollisionFlags.ONLYDYNAMIC;
		if (f == "onlywater")
			return CollisionFlags.ONLYWATER;
		return CollisionFlags.ALLOBJECTS;
	}

	//! Append a dump line to the step result and echo it to RPT (gated).
	private void AppendDump(dmE2EStepResult r, string line)
	{
		r.Dump.Insert(line);
		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2EC] " + line);
		#endif
	}

	//! Echo a completed step to RPT (gated).
	private void LogStep(dmE2EStepResult r)
	{
		#ifdef DM_BOT_DEBUG_E2E
		dmBotLog.Debug("[E2EC] step " + r.Index + " " + r.Op + " ok=" + r.Ok);
		dmBotLog.Debug("[E2EC] step reason=" + r.Reason);
		#endif
	}

	//! Resolve a world point: Y==0 (unspecified) -> snap to ground; else keep explicit Y.
	private vector ResolveWorldPos(vector p)
	{
		if (p[1] == 0.0)
			return SnapToGroundExactly(p);
		return p;
	}

	//! Create the parent directory chain of a file path (mirrors dmE2EBridge.EnsureDir,
	//! handles the "$profile:" prefix).
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

	//! Write the result to e2e-client/out/<Name>.result.json (creating the directory).
	private void SaveResult(dmE2EResult result)
	{
		string outPath = DM_E2E_CLIENT_OUT_DIR + "/" + result.Name + ".result.json";
		EnsureDir(outPath);
		string saveError;
		if (!JsonFileLoader<dmE2EResult>.SaveFile(outPath, result, saveError))
			dmBotLog.Error("[E2EC] write result failed: " + outPath + ": " + saveError);
	}

	//! Move the processed input to e2e-client/done/ (copy then delete).
	private void MoveToDone(string jobPath, string fileName)
	{
		string donePath = DM_E2E_CLIENT_DONE_DIR + "/" + fileName;
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
