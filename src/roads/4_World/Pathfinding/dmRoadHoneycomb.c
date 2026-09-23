//! dmRoadHoneycomb — asynchronous grid detour (honeycomb) for wide/compound road
//! obstructions that the box-obstacle check cannot route around.
//!
//! The honeycomb defines FREE territory and builds a path through it. A cell is
//! a pointy-top hexagon (circumradius DM_GRID_HEX_SIZE) in DOUBLED coordinates:
//! col across (m_Side), row along (m_Dir). col = 2*q + r, row = r in axial terms
//! — the across offset is S*sqrt(3)/2*col, independent of row, so the col=0
//! column stays exactly over the road for any row (no side drift). RED =
//! blocking object (even partially) or slope above ~15° or water; GREEN = clear,
//! solid, level. SAFE = GREEN with all 6 neighbours GREEN. The territory is a
//! band along the route direction `d`, growing forward row-by-row until the road
//! is clear again (DM_GRID_CLEAR_ROWS consecutive all-green road rows). Its
//! half-width is the MEASURED road width (dmRoadProbe.RoadWidth), so the grid is
//! 2× the road. The route is a shortest path on a layered graph over KEYFRAMES:
//! walking every 2nd row (so a stable corridor's center column does not wobble
//! with hex parity), we keep the "green middles" (centers of the SAFE intervals)
//! and emit a vertex only where a middle CHANGES relative to the previous kept
//! row (a shift >±1 column, or a split/merge). Every keyframe vertex connects to
//! every vertex of the previous keyframe (a forward edge, valid iff the straight
//! segment between the cell centers does not cross a RED cell; weight = XZ
//! length), and a forward DP finds the shortest entry→exit path. No A*, no
//! string-pulling — the emitted waypoints ARE the keyframe vertices. The pipeline
//! runs COLORING (one sub-step per tick: slope / water / one
//! raycast) → MARK_SAFE (DM_GRID_ROWS_PER_TICK rows per tick) → a bounded
//! synchronous graph build (O(rows×width²) for the DP edges), so per-tick spikes
//! stay minimal.

class dmRoadHoneycomb
{
	static ref dmRoadHoneycomb s_Instance;

	//! Async pipeline states.
	static const int STATE_IDLE = 0;
	static const int STATE_COLORING = 1;
	static const int STATE_MARK_SAFE = 2;
	static const int STATE_DONE = 3;
	static const int STATE_FAILED = 4;

	//! Cell colors.
	static const int CELL_UNSET = 0;
	static const int CELL_RED = 1;
	static const int CELL_GREEN = 2;
	static const int CELL_SAFE = 3;

	//! Circumradius of a honeycomb hexagon (m) — the single size constant. All
	//! derived geometry (center spacing, neighbor step cost, ray fan) is computed
	//! inline from it (sqrt(3)/2 = 0.8660254, 1.5).
	static const float DM_GRID_HEX_SIZE = 0.5;
	//! Slope: tan(15°) — a cell whose corner heights differ more than tan*hdist is
	//! too steep (red). Avoids a trig call per corner pair.
	static const float DM_GRID_SLOPE_TAN = 0.267949;
	//! Rows colored per tick (the expensive SurfaceY/raycast work is spread).
	static const int DM_GRID_ROWS_PER_TICK = 3;
	//! Верх вертикального луча соты над землёй в O (м) — выше крыши среднего авто.
	static const float DM_GRID_RAY_TOP = 2.0;
	//! Низ вертикального луча соты над землёй в O (м) — над землёй, чтобы не ловить
	//! террейн как препятствие.
	static const float DM_GRID_RAY_BOTTOM = 0.3;
	//! Высота горизонтальных лучей над локальным террейном вершины (м) в исходящей точке.
	static const float DM_GRID_RAY_HORIZ_HEIGHT_BEG = 0.5;
	//! Высота горизонтальных лучей над локальным террейном вершины (м) в конечной точке.
	static const float DM_GRID_RAY_HORIZ_HEIGHT_END = 0.2;
	//! Тип пересечения всех 4 лучей соты: физическая геометрия (ObjIntersectGeom)
	//! ловит baked-static ограждения (бетон/забор) по коллизии.
	static const int DM_GRID_RAY_INTERSECT = ObjIntersectGeom;
	//! 50 м чистого дорожного полотна: подряд все-зелёных дорожных рядов, после
	//! которых территория перестаёт расти ("дорога снова чистая"). 50 м / шаг
	//! ряда 0.75 м = 66.7 → 67.
	static const int DM_GRID_CLEAR_ROWS = 67;
	//! Territory length hard cap (rows, ~82 m at 0.5 m hex) — a safety net if the
	//! road never clears; the adaptive stop normally ends coloring far earlier.
	static const int DM_GRID_MAX_ROWS = 110;
	//! Инфинити-болванка для DP-дистанций и min-dist скана в FindNearestSafe.
	static const float DM_GRID_INF = 1000000000.0;

	//! Coloring sub-steps (one per tick): slope (4 corner SurfaceY), water, then
	//! one raycast per tick for the 4-cell ray fan (top→bot, ul→lr, ur→ll, vertical).
	static const int STEP_SLOPE = 0;
	static const int STEP_WATER = 1;
	static const int STEP_RAY0 = 2;
	static const int STEP_RAY1 = 3;
	static const int STEP_RAY2 = 4;
	static const int STEP_RAY3 = 5;

	static dmRoadHoneycomb Get()
	{
		if (!s_Instance)
			s_Instance = new dmRoadHoneycomb();
		return s_Instance;
	}

	private int m_State;
	//! Время начала постройки (тик-тайм) — для отчёта buildMs в DONE-логе.
	private float m_BuildStartTime;
	private vector m_CarPos;
	private vector m_Dir;
	private vector m_Side;
	private vector m_Origin;
	private int m_HalfWidth;
	//! Measured road half width (m) — roadWidth * 0.5 from dmRoadProbe.RoadWidth;
	//! used by IsRoadCol for the clear-row check instead of the removed constants.
	private float m_RoadHalfWidth;
	private int m_Width;
	private int m_RowCount;
	//! Фиксированная длина территории (lengthMeters > 0): TickColor раскрашивает
	//! всю длину и не гасит территорию по DM_GRID_CLEAR_ROWS.
	private bool m_FixedLength;
	private int m_ColorRow;
	private int m_RoadClearRun;
	private bool m_SeenRed;
	//! Курсор разметки: текущая колонка и под-шаг (STEP_*) обрабатываемой ячейки.
	private int m_ColorCol;
	private int m_ColorStep;
	//! Высоты 4 углов текущей ячейки (для slope-проверки), накопленные в STEP_SLOPE.
	private float m_CellY0;
	private float m_CellY1;
	private float m_CellY2;
	private float m_CellY3;
	//! Накопленная чистота текущего ряда: true, пока ни одна road-ячейка не RED.
	private bool m_RowRoadClear;
	//! Курсор MarkSafe (текущий ряд).
	private int m_MarkRow;
	//! 6 doubled neighbours (pointy-top) as parallel dcol/drow offset tables,
	//! shared by MarkSafe/AllNeighborsWalkable:
	//! (+2,0) (-2,0) (+1,-1) (-1,-1) (+1,+1) (-1,+1).
	private ref array<int> m_NeighDCol;
	private ref array<int> m_NeighDRow;
	private ref array<int> m_Cells;
	private ref array<vector> m_Route;
	private int m_RouteIdx;
	private int m_GoalRow;
	private int m_GoalCol;
	private ref array<vector> m_Waypoints;
	private ref array<int> m_WaypointCells;
	//! Машина (pIgnore в райкасте клеток) и её водитель: без них соты видят
	//! собственный кузов в первых рядах территории как «препятствие», раньше
	//! времени ставят m_SeenRed и гасят грид на ~10 м, не дойдя до реального барьера.
	private Object m_CarObj;
	private Object m_DriverObj;

	void dmRoadHoneycomb()
	{
		m_State = STATE_IDLE;
		m_BuildStartTime = 0.0;
		m_CarPos = vector.Zero;
		m_Dir = Vector(1.0, 0.0, 0.0);
		m_Side = Vector(0.0, 0.0, 1.0);
		m_Origin = vector.Zero;
		m_HalfWidth = 0;
		m_RoadHalfWidth = 0.0;
		m_Width = 0;
		m_RowCount = 0;
		m_FixedLength = false;
		m_ColorRow = 0;
		m_RoadClearRun = 0;
		m_SeenRed = false;
		m_ColorCol = 0;
		m_ColorStep = STEP_SLOPE;
		m_CellY0 = 0.0;
		m_CellY1 = 0.0;
		m_CellY2 = 0.0;
		m_CellY3 = 0.0;
		m_RowRoadClear = true;
		m_MarkRow = 0;
		m_Cells = null;
		m_Route = null;
		m_RouteIdx = 0;
		m_GoalRow = 0;
		m_GoalCol = 0;
		m_Waypoints = null;
		m_WaypointCells = null;
		m_CarObj = null;
		m_DriverObj = null;
		m_NeighDCol = new array<int>();
		m_NeighDRow = new array<int>();
		m_NeighDCol.Insert(2);
		m_NeighDRow.Insert(0);
		m_NeighDCol.Insert(-2);
		m_NeighDRow.Insert(0);
		m_NeighDCol.Insert(1);
		m_NeighDRow.Insert(-1);
		m_NeighDCol.Insert(-1);
		m_NeighDRow.Insert(-1);
		m_NeighDCol.Insert(1);
		m_NeighDRow.Insert(1);
		m_NeighDCol.Insert(-1);
		m_NeighDRow.Insert(1);
	}

	//! Start a detour: car position, route direction (horizontal), the route
	//! waypoints and the index of the next waypoint to reach. Resets all state.
	//! lengthMeters > 0 — фиксированная длина территории (м, без адаптивного
	//! стопа); widthCells > 0 — явный override ширины территории в doubled-колонках
	//! (иначе измеряется дорога dmRoadProbe.RoadWidth: полуширина = RoundF(roadWidth
	//! / across-step), сетка = 2× дорога). Диагностический прогон (griddump)
	//! передаёт 0/null в carObj/driverObj — соты работают без машины.
	void Start(vector carPos, vector dir, array<vector> route, int routeIdx, Object carObj, Object driverObj, float lengthMeters, int widthCells)
	{
		int i;
		float roadWidth;
		m_State = STATE_IDLE;
		m_BuildStartTime = GetGame().GetTickTime();
		m_CarPos = carPos;
		m_CarObj = carObj;
		m_DriverObj = driverObj;
		m_Dir = dir;
		m_Dir[1] = 0.0;
		if (m_Dir.Length() < 0.01)
			m_Dir = Vector(1.0, 0.0, 0.0);
		else
			m_Dir.Normalize();
		m_Side = Vector(-m_Dir[2], 0.0, m_Dir[0]);
		m_Origin = carPos;
		roadWidth = dmRoadProbe.RoadWidth(carPos, dir);
		if (roadWidth < DM_ROAD_MIN_WIDTH)
			roadWidth = DM_ROAD_MIN_WIDTH;
		m_RoadHalfWidth = roadWidth * 0.5;
		if (widthCells > 0)
			m_HalfWidth = widthCells;
		else
			m_HalfWidth = RoundF(roadWidth / (DM_GRID_HEX_SIZE * 0.8660254));
		m_Width = m_HalfWidth * 2 + 1;
		if (lengthMeters > 0.0)
		{
			m_RowCount = Math.Floor(lengthMeters / (DM_GRID_HEX_SIZE * 1.5));
			m_FixedLength = true;
		}
		else
		{
			m_RowCount = DM_GRID_MAX_ROWS;
			m_FixedLength = false;
		}
		m_ColorRow = 0;
		m_RoadClearRun = 0;
		m_SeenRed = false;
		m_ColorCol = -m_HalfWidth;
		m_ColorStep = STEP_SLOPE;
		m_RowRoadClear = true;
		m_MarkRow = 0;
		m_RouteIdx = routeIdx;
		m_GoalRow = 0;
		m_GoalCol = 0;
		m_Waypoints = null;
		m_WaypointCells = null;

		m_Route = new array<vector>();
		for (i = 0; i < route.Count(); i++)
			m_Route.Insert(route[i]);

		m_Cells = new array<int>();
		int total = m_RowCount * m_Width;
		for (i = 0; i < total; i++)
			m_Cells.Insert(CELL_UNSET);

		m_State = STATE_COLORING;

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[GRID] Start cells=" + total + " rows=" + m_RowCount + " width=" + m_Width);
		#endif
	}

	//! Advance the pipeline: each state advances a bounded slice of work per tick.
	void Tick(float dt)
	{
		if (m_State == STATE_COLORING)
			TickColor();
		else if (m_State == STATE_MARK_SAFE)
			MarkSafe();
	}

	bool IsDone()
	{
		return m_State == STATE_DONE;
	}

	bool IsFailed()
	{
		return m_State == STATE_FAILED;
	}

	//! The path (world coordinates), or false when not DONE.
	bool GetPath(out array<vector> waypoints)
	{
		if (m_State != STATE_DONE || !m_Waypoints)
			return false;
		waypoints = new array<vector>();
		int i;
		for (i = 0; i < m_Waypoints.Count(); i++)
			waypoints.Insert(m_Waypoints[i]);
		return true;
	}

	//! Final waypoint cells (cell indices, same order as GetPath) — the keyframe
	//! vertices of the corridor-center trace, for the E2E grid dump.
	int GetWaypointCount()
	{
		if (!m_WaypointCells)
			return 0;
		return m_WaypointCells.Count();
	}

	bool GetWaypointCell(int i, out int col, out int row)
	{
		if (!m_WaypointCells || i < 0 || i >= m_WaypointCells.Count())
			return false;
		col = CellCol(m_WaypointCells[i]);
		row = CellRow(m_WaypointCells[i]);
		return true;
	}

	//! Territory geometry for the E2E grid dump (griddump op).

	int GetRowCount()
	{
		return m_RowCount;
	}

	int GetWidth()
	{
		return m_Width;
	}

	int GetHalfWidth()
	{
		return m_HalfWidth;
	}

	vector GetOrigin()
	{
		return m_Origin;
	}

	vector GetDir()
	{
		return m_Dir;
	}

	vector GetSide()
	{
		return m_Side;
	}

	//! Текущее состояние пайплайна (STATE_*): 0 idle / 1 coloring / 2 mark /
	//! 3 done / 4 failed. Для E2E-дампа текущего состояния (op dumpgrid).
	int GetStatusId()
	{
		return m_State;
	}

	//! Маршрут (точки дороги), переданный в Start, или null если сетка ещё не
	//! запускалась. Для E2E-дампа текущего состояния (op dumpgrid).
	array<vector> GetRoute()
	{
		return m_Route;
	}

	//! Cell color value (CELL_UNSET/CELL_RED/CELL_GREEN/CELL_SAFE).
	int GetCellColor(int col, int row)
	{
		return m_Cells[CellIndex(col, row)];
	}

	//! World XZ center of a cell (Y = territory origin height — dump only).
	vector GetCellCenter(int col, int row)
	{
		return CellCenter(col, row);
	}

	//! Color one sub-step (slope / water / one raycast) for the current cell; then
	//! advance the cursor. The invalid (odd col+row) slots are skipped as cursor
	//! movement, leaving them UNSET.
	private void TickColor()
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.GridColor");
		#endif

		while (m_ColorRow < m_RowCount)
		{
			while (m_ColorCol <= m_HalfWidth && (m_ColorCol + m_ColorRow) % 2 != 0)
				m_ColorCol = m_ColorCol + 1;
			if (m_ColorCol <= m_HalfWidth)
				break;
			EndRow();
		}
		if (m_ColorRow >= m_RowCount)
		{
			m_MarkRow = 0;
			m_State = STATE_MARK_SAFE;
			return;
		}
		StepColorCell();
	}

	//! One sub-step for the cell at (m_ColorCol, m_ColorRow).
	private void StepColorCell()
	{
		int col = m_ColorCol;
		int row = m_ColorRow;
		vector center = CellCenter(col, row);
		float cx = center[0];
		float cz = center[2];
		int cidx = CellIndex(col, row);
		float s = DM_GRID_HEX_SIZE;
		float sCos = s * 0.8660254;
		float sHalf = s * 0.5;
		vector o = Vector(cx, 0.0, cz);
		vector from;
		vector to;
		float h;
		float groundY;

		if (m_ColorStep == STEP_SLOPE)
		{
			h = DM_GRID_HEX_SIZE * 0.5;
			m_CellY0 = GetGame().SurfaceY(cx - h, cz - h);
			m_CellY1 = GetGame().SurfaceY(cx + h, cz - h);
			m_CellY2 = GetGame().SurfaceY(cx - h, cz + h);
			m_CellY3 = GetGame().SurfaceY(cx + h, cz + h);
			if (SlopeTooSteep())
			{
				MarkCellRed(col, cidx);
				return;
			}
			m_ColorStep = STEP_WATER;
			return;
		}
		if (m_ColorStep == STEP_WATER)
		{
			if (GetGame().SurfaceIsSea(cx, cz) || GetGame().SurfaceIsPond(cx, cz))
			{
				MarkCellRed(col, cidx);
				return;
			}
			m_ColorStep = STEP_RAY0;
			return;
		}
		if (m_ColorStep == STEP_RAY0)
		{
			from = o + m_Dir * s;
			to = o - m_Dir * s;
			from[1] = GetGame().SurfaceY(from[0], from[2]) + DM_GRID_RAY_HORIZ_HEIGHT_BEG;
			to[1] = GetGame().SurfaceY(to[0], to[2]) + DM_GRID_RAY_HORIZ_HEIGHT_END;
			if (RayHitsObstacle(from, to))
			{
				MarkCellRed(col, cidx);
				return;
			}
			m_ColorStep = STEP_RAY1;
			return;
		}
		if (m_ColorStep == STEP_RAY1)
		{
			from = o - m_Side * sCos + m_Dir * sHalf;
			to = o + m_Side * sCos - m_Dir * sHalf;
			from[1] = GetGame().SurfaceY(from[0], from[2]) + DM_GRID_RAY_HORIZ_HEIGHT_BEG;
			to[1] = GetGame().SurfaceY(to[0], to[2]) + DM_GRID_RAY_HORIZ_HEIGHT_END;
			if (RayHitsObstacle(from, to))
			{
				MarkCellRed(col, cidx);
				return;
			}
			m_ColorStep = STEP_RAY2;
			return;
		}
		if (m_ColorStep == STEP_RAY2)
		{
			from = o + m_Side * sCos + m_Dir * sHalf;
			to = o - m_Side * sCos - m_Dir * sHalf;
			from[1] = GetGame().SurfaceY(from[0], from[2]) + DM_GRID_RAY_HORIZ_HEIGHT_BEG;
			to[1] = GetGame().SurfaceY(to[0], to[2]) + DM_GRID_RAY_HORIZ_HEIGHT_END;
			if (RayHitsObstacle(from, to))
			{
				MarkCellRed(col, cidx);
				return;
			}
			m_ColorStep = STEP_RAY3;
			return;
		}
		// STEP_RAY3: vertical ray through O.
		groundY = GetGame().SurfaceY(cx, cz);
		from = Vector(cx, groundY + DM_GRID_RAY_TOP, cz);
		to = Vector(cx, groundY + DM_GRID_RAY_BOTTOM, cz);
		if (RayHitsObstacle(from, to))
		{
			MarkCellRed(col, cidx);
			return;
		}
		MarkCellGreen(cidx);
	}

	//! Cell is RED (any of the four rays / slope / water): record it, fold the
	//! road-clear flag if it is inside the lane, advance the cursor.
	private void MarkCellRed(int col, int cidx)
	{
		m_Cells[cidx] = CELL_RED;
		if (IsRoadCol(col))
			m_RowRoadClear = false;
		m_ColorCol = m_ColorCol + 1;
		m_ColorStep = STEP_SLOPE;
	}

	//! Cell is GREEN (all checks passed): record it, advance the cursor.
	private void MarkCellGreen(int cidx)
	{
		m_Cells[cidx] = CELL_GREEN;
		m_ColorCol = m_ColorCol + 1;
		m_ColorStep = STEP_SLOPE;
	}

	//! End of a row: update the road-clear run from the accumulated row-clear flag,
	//! then advance to the next row and apply the adaptive stop.
	private void EndRow()
	{
		if (m_RowRoadClear)
		{
			//! Считаем чистые ряды только ПОСЛЕ препятствия (m_SeenRed): иначе
			//! территория гасится на первых же чистых рядах ДО барьера (дорога
			//! чистая от старта) и барьер не попадает в грид.
			if (m_SeenRed)
				m_RoadClearRun = m_RoadClearRun + 1;
		}
		else
		{
			m_SeenRed = true;
			m_RoadClearRun = 0;
		}
		m_ColorCol = -m_HalfWidth;
		m_ColorRow = m_ColorRow + 1;
		m_ColorStep = STEP_SLOPE;
		m_RowRoadClear = true;
		if (!m_FixedLength && m_RoadClearRun >= DM_GRID_CLEAR_ROWS)
			m_RowCount = m_ColorRow;
	}

	//! Slope check over the 4 accumulated corner heights: any edge/diagonal pair
	//! differing by more than tan*hdist is too steep.
	private bool SlopeTooSteep()
	{
		float edgeLimit = DM_GRID_SLOPE_TAN * DM_GRID_HEX_SIZE;
		float diagLimit = edgeLimit * 1.414214;
		if (Math.AbsFloat(m_CellY0 - m_CellY1) > edgeLimit)
			return true;
		if (Math.AbsFloat(m_CellY0 - m_CellY2) > edgeLimit)
			return true;
		if (Math.AbsFloat(m_CellY3 - m_CellY1) > edgeLimit)
			return true;
		if (Math.AbsFloat(m_CellY3 - m_CellY2) > edgeLimit)
			return true;
		if (Math.AbsFloat(m_CellY0 - m_CellY3) > diagLimit)
			return true;
		if (Math.AbsFloat(m_CellY1 - m_CellY2) > diagLimit)
			return true;
		return false;
	}

	//! Один ТОНКИЙ луч (radius 0, type DM_GRID_RAY_INTERSECT = ObjIntersectGeom):
	//! true, если на луче есть хит с физической коллизией (hit.obj != null).
	//! Машина/водитель (m_CarObj/m_DriverObj) пропускаются; null-хиты (без коллизии)
	//! пропускаются молча.
	private bool RayHitsObstacle(vector from, vector to)
	{
		RaycastRVParams params = new RaycastRVParams(from, to, m_CarObj);
		params.type = DM_GRID_RAY_INTERSECT;
		params.flags = CollisionFlags.ALLOBJECTS;
		params.sorted = true;
		array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>();
		if (!DayZPhysics.RaycastRVProxy(params, hits))
			return false;

		#ifdef DM_BOT_DEBUG_ROADS
		{
			int _di;
			for (_di = 0; _di < hits.Count(); _di++)
			{
				RaycastRVResult _h = hits[_di];
				string _t = "null";
				if (_h.obj)
					_t = _h.obj.GetType();
				dmBotLog.Debug("[GRID] rawHit: ray=" + from + "->" + to + " i=" + _di);
				dmBotLog.Debug("[GRID] rawHit: obj=" + _t + " pos=" + _h.pos);
				dmBotLog.Debug("[GRID] rawHit: comp=" + _h.component + " entry=" + _h.entry + " exit=" + _h.exit);
			}
		}
		#endif

		int i;
		RaycastRVResult hit;
		for (i = 0; i < hits.Count(); i++)
		{
			hit = hits[i];
			if (m_CarObj && (hit.obj == m_CarObj || hit.parent == m_CarObj))
				continue;
			if (m_DriverObj && (hit.obj == m_DriverObj || hit.parent == m_DriverObj))
				continue;
			if (hit.obj != null)
			{
				if (hit.obj.IsClothing())
					continue;
				if (hit.obj.IsInherited(DeployableContainer_Base))
					continue;
				#ifdef DM_BOT_DEBUG_ROADS
				dmBotLog.Debug("[GRID] blocked: pos=" + hit.pos + " obj=" + hit.obj.GetType());
				#endif
				return true;
			}
		}
		return false;
	}

	//! Mark SAFE (a GREEN cell with all 6 neighbours walkable) — advances
	//! DM_GRID_ROWS_PER_TICK rows per tick, then enters pathfinding.
	private void MarkSafe()
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.MarkSafe");
		#endif

		int rowsDone = 0;
		while (rowsDone < DM_GRID_ROWS_PER_TICK && m_MarkRow < m_RowCount)
		{
			MarkRow(m_MarkRow);
			m_MarkRow = m_MarkRow + 1;
			rowsDone = rowsDone + 1;
		}
		if (m_MarkRow >= m_RowCount)
			BuildGraphRoute();
	}

	//! Mark one row's GREEN cells as SAFE when all 6 neighbours are walkable.
	private void MarkRow(int r)
	{
		int col;
		for (col = -m_HalfWidth; col <= m_HalfWidth; col++)
		{
			if ((col + r) % 2 != 0)
				continue;
			int idx = CellIndex(col, r);
			if (m_Cells[idx] != CELL_GREEN)
				continue;
			if (AllNeighborsWalkable(col, r))
				m_Cells[idx] = CELL_SAFE;
		}
	}

	private bool AllNeighborsWalkable(int col, int row)
	{
		int i;
		for (i = 0; i < m_NeighDCol.Count(); i++)
		{
			int ncol = col + m_NeighDCol[i];
			int nrow = row + m_NeighDRow[i];
			if (ncol < -m_HalfWidth || ncol > m_HalfWidth || nrow < 0 || nrow >= m_RowCount)
				return false;
			if (!IsWalkable(m_Cells[CellIndex(ncol, nrow)]))
				return false;
		}
		return true;
	}

	private bool IsWalkable(int color)
	{
		return color == CELL_GREEN || color == CELL_SAFE;
	}

	//! Goal = the deepest route waypoint cell inside the territory; fallback: the
	//! far row (m_RowCount - 1) at col 0. Stores both m_GoalRow and m_GoalCol —
	//! the graph route snaps the exit to the SAFE cell nearest this goal.
	private bool FindGoalWaypoint()
	{
		if (!m_Route)
			return false;
		int goalRow = m_RowCount - 1;
		int goalCol = 0;
		int bestRow = -1;
		int bestCol = 0;
		int i;
		for (i = m_RouteIdx; i < m_Route.Count(); i++)
		{
			vector wp = m_Route[i];
			int col;
			int row;
			WorldToCell(wp, col, row);
			if (row < 0 || row >= m_RowCount)
				continue;
			if (row > bestRow)
			{
				bestRow = row;
				bestCol = col;
			}
		}
		if (bestRow >= 0)
		{
			goalRow = bestRow;
			goalCol = bestCol;
		}
		m_GoalRow = goalRow;
		m_GoalCol = goalCol;
		return true;
	}

	//! Nearest SAFE cell to (col,row): a single linear pass over all valid cells
	//! picks the SAFE cell with the minimal squared doubled-distance to (col,row).
	//! Returns false when the grid has no SAFE cell at all (degenerate).
	private bool FindNearestSafe(int col, int row, out int outCell)
	{
		int bestIdx;
		float bestDist2;
		int c;
		int r;
		int idx;
		int dcol;
		int drow;
		float dist2;
		bestIdx = -1;
		bestDist2 = DM_GRID_INF;
		for (r = 0; r < m_RowCount; r++)
		{
			for (c = -m_HalfWidth; c <= m_HalfWidth; c++)
			{
				if ((c + r) % 2 != 0)
					continue;
				idx = CellIndex(c, r);
				if (m_Cells[idx] != CELL_SAFE)
					continue;
				dcol = c - col;
				drow = r - row;
				dist2 = (float)(dcol * dcol + drow * drow);
				if (dist2 < bestDist2)
				{
					bestDist2 = dist2;
					bestIdx = idx;
				}
			}
		}
		if (bestIdx < 0)
			return false;
		outCell = bestIdx;
		return true;
	}

	//! "Green middles" of one row: split the valid SAFE columns into contiguous
	//! intervals (step 2 apart) and emit the center of each. The rule is global
	//! per row: if the row's total road-lane cells (SAFE && IsRoadCol) exceed 5,
	//! emit only the centers of the road-lane sub-segments and ignore intervals
	//! fully on the shoulder (0 road cells); otherwise emit the center of every
	//! SAFE interval (shoulder included), as before. For an even count the right
	//! of the two middle cells is taken (deterministic).
	private void GreenMiddles(int row, out array<int> middles)
	{
		int col0;
		int col;
		int first;
		int last;
		int c;
		int count;
		int mid;
		int roadFirst;
		int roadLast;
		int roadCount;
		int rc;
		int totalRoadCells;
		int i;
		array<int> firstArr;
		array<int> lastArr;
		array<int> roadFirstArr;
		array<int> roadLastArr;
		array<int> roadCountArr;
		middles = new array<int>();
		firstArr = new array<int>();
		lastArr = new array<int>();
		roadFirstArr = new array<int>();
		roadLastArr = new array<int>();
		roadCountArr = new array<int>();
		totalRoadCells = 0;
		col0 = -m_HalfWidth;
		if ((col0 + row) % 2 != 0)
			col0 = col0 + 1;
		col = col0;
		while (col <= m_HalfWidth)
		{
			if (m_Cells[CellIndex(col, row)] != CELL_SAFE)
			{
				col = col + 2;
				continue;
			}
			first = col;
			last = col;
			c = col + 2;
			while (c <= m_HalfWidth && m_Cells[CellIndex(c, row)] == CELL_SAFE)
			{
				last = c;
				c = c + 2;
			}
			roadFirst = 0;
			roadLast = 0;
			roadCount = 0;
			rc = first;
			while (rc <= last)
			{
				if (IsRoadCol(rc))
				{
					if (roadCount == 0)
						roadFirst = rc;
					roadLast = rc;
					roadCount = roadCount + 1;
				}
				rc = rc + 2;
			}
			firstArr.Insert(first);
			lastArr.Insert(last);
			roadFirstArr.Insert(roadFirst);
			roadLastArr.Insert(roadLast);
			roadCountArr.Insert(roadCount);
			totalRoadCells = totalRoadCells + roadCount;
			col = c;
		}
		if (totalRoadCells > 5)
		{
			for (i = 0; i < firstArr.Count(); i++)
			{
				if (roadCountArr[i] == 0)
					continue;
				count = (roadLastArr[i] - roadFirstArr[i]) / 2 + 1;
				mid = roadFirstArr[i] + 2 * (count / 2);
				middles.Insert(CellIndex(mid, row));
			}
		}
		else
		{
			for (i = 0; i < firstArr.Count(); i++)
			{
				count = (lastArr[i] - firstArr[i]) / 2 + 1;
				mid = firstArr[i] + 2 * (count / 2);
				middles.Insert(CellIndex(mid, row));
			}
		}
	}

	//! True when the current row's middle columns differ from the previous
	//! processed row's (a split/merge changes the count; a shift of more than ±1
	//! column counts as a change — ±1 wobble from hex parity is ignored).
	private bool MidsChanged(array<int> aCols, array<int> bCols)
	{
		if (aCols.Count() != bCols.Count())
			return true;
		int i;
		int diff;
		for (i = 0; i < aCols.Count(); i++)
		{
			diff = aCols[i] - bCols[i];
			if (diff < 0)
				diff = -diff;
			if (diff > 1)
				return true;
		}
		return false;
	}

	//! Number of SAFE cells on the road lane (IsRoadCol) in a row — the shoulder
	//! does not count. Used to detect corridor width changes for waypoint nudging.
	private int RoadLaneWidth(int row)
	{
		int count;
		int col;
		count = 0;
		for (col = -m_HalfWidth; col <= m_HalfWidth; col++)
		{
			if ((col + row) % 2 != 0)
				continue;
			if (m_Cells[CellIndex(col, row)] == CELL_SAFE && IsRoadCol(col))
				count = count + 1;
		}
		return count;
	}

	//! Build the graph route as a deterministic centerline trace over every 2nd
	//! row: entry = nearest SAFE to (0,0), exit = nearest SAFE to the goal
	//! waypoint. A KEYFRAME is emitted only when a row's "green middles" (centers
	//! of the SAFE intervals, columns) change vs the previous processed row (shift
	//! > ±1 column, or a split/merge): the entry layer, then the keyframe layers,
	//! then the exit layer. Edges are full bipartite forward between adjacent
	//! layers (valid iff SegmentCrossesRed is false; weight = XZ length), and a
	//! forward DP picks the shortest entry→exit path. The keyframes themselves are
	//! the waypoints (no string-pulling). Post-DP, each non-terminal waypoint is
	//! nudged 2 rows earlier/later where the road-lane width changed by more than
	//! 4 cells (narrowing earlier, widening later), preserving its column, only
	//! when the target row is free and SAFE. On any failure — FAILED.
	private void BuildGraphRoute()
	{
		int entryCell;
		int exitCell;
		int entryRow;
		int exitRow;
		ref array<int> vertCells;
		ref array<int> layerStart;
		int r;
		int mi;
		array<int> middles;
		array<int> prevCols;
		array<int> curCols;
		int vcount;
		int layerCount;
		ref array<float> dist;
		ref array<int> parent;
		int i;
		int k;
		int u;
		int v;
		vector centerU;
		vector centerV;
		float dx;
		float dz;
		float len;
		int exitIdx;
		ref array<int> pathIdx;
		int cur;
		int pi;
		int cell;
		int nWp;
		int wi;
		int wr;
		int wc;
		int wb;
		int wa;
		int delta;
		int newRow;
		int newCell;
		bool occupied;
		int wj;

		if (!FindGoalWaypoint())
		{
			m_State = STATE_FAILED;
			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[GRID] FAILED goal");
			#endif
			return;
		}
		if (!FindNearestSafe(0, 0, entryCell))
		{
			m_State = STATE_FAILED;
			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[GRID] FAILED entry");
			#endif
			return;
		}
		if (!FindNearestSafe(m_GoalCol, m_GoalRow, exitCell))
		{
			m_State = STATE_FAILED;
			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[GRID] FAILED exit");
			#endif
			return;
		}
		if (entryCell == exitCell)
		{
			m_Waypoints = new array<vector>();
			m_WaypointCells = new array<int>();
			m_WaypointCells.Insert(entryCell);
			m_Waypoints.Insert(CellWorld(entryCell));
			m_State = STATE_DONE;
			LogGridDone();
			return;
		}

		entryRow = CellRow(entryCell);
		exitRow = CellRow(exitCell);

		vertCells = new array<int>();
		layerStart = new array<int>();

		vertCells.Insert(entryCell);
		layerStart.Insert(0);

		prevCols = null;
		for (r = entryRow; r <= exitRow; r = r + 2)
		{
			GreenMiddles(r, middles);
			if (middles.Count() == 0)
			{
				m_State = STATE_FAILED;
				#ifdef DM_BOT_DEBUG_ROADS
				dmBotLog.Debug("[GRID] FAILED corridor ended");
				#endif
				return;
			}
			curCols = new array<int>();
			for (mi = 0; mi < middles.Count(); mi++)
			{
				curCols.Insert(CellCol(middles[mi]));
			}
			if (prevCols)
			{
				if (MidsChanged(curCols, prevCols))
				{
					layerStart.Insert(vertCells.Count());
					for (mi = 0; mi < middles.Count(); mi++)
					{
						vertCells.Insert(middles[mi]);
					}
				}
			}
			prevCols = curCols;
		}
		layerStart.Insert(vertCells.Count());
		vertCells.Insert(exitCell);
		layerStart.Insert(vertCells.Count());

		vcount = vertCells.Count();
		layerCount = layerStart.Count() - 1;

		dist = new array<float>();
		parent = new array<int>();
		for (i = 0; i < vcount; i++)
		{
			dist.Insert(DM_GRID_INF);
			parent.Insert(-1);
		}
		dist[0] = 0.0;

		for (k = 0; k < layerCount - 1; k++)
		{
			for (u = layerStart[k]; u < layerStart[k + 1]; u++)
			{
				if (dist[u] >= DM_GRID_INF)
					continue;
				for (v = layerStart[k + 1]; v < layerStart[k + 2]; v++)
				{
					if (SegmentCrossesRed(vertCells[u], vertCells[v]))
						continue;
					centerU = CellCenter(CellCol(vertCells[u]), CellRow(vertCells[u]));
					centerV = CellCenter(CellCol(vertCells[v]), CellRow(vertCells[v]));
					dx = centerU[0] - centerV[0];
					dz = centerU[2] - centerV[2];
					len = Math.Sqrt(dx * dx + dz * dz);
					if (dist[u] + len < dist[v])
					{
						dist[v] = dist[u] + len;
						parent[v] = u;
					}
				}
			}
		}

		exitIdx = vcount - 1;
		if (dist[exitIdx] >= DM_GRID_INF)
		{
			m_State = STATE_FAILED;
			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[GRID] FAILED no path");
			#endif
			return;
		}

		pathIdx = new array<int>();
		cur = exitIdx;
		while (cur >= 0)
		{
			pathIdx.Insert(cur);
			cur = parent[cur];
		}

		m_Waypoints = new array<vector>();
		m_WaypointCells = new array<int>();
		for (pi = pathIdx.Count() - 1; pi >= 0; pi--)
		{
			cell = vertCells[pathIdx[pi]];
			m_WaypointCells.Insert(cell);
			m_Waypoints.Insert(CellWorld(cell));
		}

		//! Пост-обработка: сдвигаем вершины по изменению ширины полотна-коридора
		//! (сужение → на 2 ряда раньше, расширение → на 2 позже), если изменение
		//! больше 4 клеток, целевой ряд свободен и клетка SAFE. Вход/выход не трогаем.
		nWp = m_WaypointCells.Count();
		if (nWp > 2)
		{
			for (wi = 1; wi < nWp - 1; wi++)
			{
				wr = CellRow(m_WaypointCells[wi]);
				wc = CellCol(m_WaypointCells[wi]);
				wb = RoadLaneWidth(wr - 2);
				wa = RoadLaneWidth(wr);
				delta = wa - wb;
				newRow = -1;
				if (delta < -4)
					newRow = wr - 2;
				else if (delta > 4)
					newRow = wr + 2;
				if (newRow < 0 || newRow >= m_RowCount)
					continue;
				if ((wc + newRow) % 2 != 0)
					continue;
				occupied = false;
				for (wj = 0; wj < nWp; wj++)
				{
					if (wj == wi)
						continue;
					if (CellRow(m_WaypointCells[wj]) == newRow)
					{
						occupied = true;
						break;
					}
				}
				if (occupied)
					continue;
				newCell = CellIndex(wc, newRow);
				if (m_Cells[newCell] != CELL_SAFE)
					continue;
				m_WaypointCells[wi] = newCell;
				m_Waypoints[wi] = CellWorld(newCell);
			}
		}
		m_State = STATE_DONE;
		LogGridDone();
	}

	//! Единый отчётный лог о завершении постройки сетки: путь / длина территории
	//! / время постройки / причина остановки роста. Причина: fixed — фиксированная
	//! длина; cap — дошло до DM_GRID_MAX_ROWS без очистки дороги; clean —
	//! адаптивный стоп по DM_GRID_CLEAR_ROWS чистых рядов.
	private void LogGridDone()
	{
		#ifdef DM_BOT_DEBUG_ROADS
		float meters = (float)m_RowCount * DM_GRID_HEX_SIZE * 1.5;
		float buildMs = (GetGame().GetTickTime() - m_BuildStartTime) * 1000.0;
		string stopReason = "clean";
		if (m_FixedLength)
			stopReason = "fixed";
		else if (m_RowCount == DM_GRID_MAX_ROWS)
			stopReason = "cap";
		dmBotLog.Debug("[GRID] DONE path=" + m_Waypoints.Count() + " rows=" + m_RowCount + " meters=" + meters + " buildMs=" + buildMs + " stop=" + stopReason);
		#endif
	}

	//! True when the straight segment between two walkable cell centers crosses a
	//! RED cell (sampled at half-cell steps in doubled float space).
	private bool SegmentCrossesRed(int a, int b)
	{
		float cola = (float)CellCol(a);
		float rowa = (float)CellRow(a);
		float colb = (float)CellCol(b);
		float rowb = (float)CellRow(b);
		float dcol = colb - cola;
		float drow = rowb - rowa;
		float len = Math.Sqrt(dcol * dcol + drow * drow);
		int steps = Math.Floor(len * 2.0 + 0.5) + 1;
		float stepT = 1.0 / (float)steps;
		float t = stepT;
		int i;
		for (i = 1; i < steps; i++)
		{
			float fcol = cola + dcol * t;
			float frow = rowa + drow * t;
			int col;
			int row;
			SnapToCell(fcol, frow, col, row);
			if (m_Cells[CellIndex(col, row)] == CELL_RED)
				return true;
			t = t + stepT;
		}
		return false;
	}

	//! World XZ position of a cell center (Y is the car's height; callers that need
	//! ground height use CellWorld). Pointy-top doubled: across = S*sqrt(3)/2*col,
	//! along = S*1.5*row — across does not depend on row, so col=0 stays over the
	//! road for any row.
	private vector CellCenter(int col, int row)
	{
		float across = DM_GRID_HEX_SIZE * 0.8660254 * (float)col;
		float along = DM_GRID_HEX_SIZE * 1.5 * (float)row;
		return m_Origin + m_Side * across + m_Dir * along;
	}

	//! World position of a cell center snapped to the ground (SurfaceY).
	private vector CellWorld(int idx)
	{
		vector p = CellCenter(CellCol(idx), CellRow(idx));
		p[1] = GetGame().SurfaceY(p[0], p[2]);
		return p;
	}

	private int CellIndex(int col, int row)
	{
		return row * m_Width + col + m_HalfWidth;
	}

	private int CellCol(int idx)
	{
		int row = idx / m_Width;
		return idx - row * m_Width - m_HalfWidth;
	}

	private int CellRow(int idx)
	{
		return idx / m_Width;
	}

	//! True when the cell's across offset (col) is within the road lane (for the
	//! clear-row check).
	private bool IsRoadCol(int col)
	{
		float across = DM_GRID_HEX_SIZE * 0.8660254 * (float)col;
		return Math.AbsFloat(across) <= m_RoadHalfWidth;
	}

	//! Doubled rounding world→cell: row = Round(along / (S*1.5)),
	//! col = Round(across / (S*0.8660254)), then a parity correction snaps to the
	//! nearest valid (even col+row) cell.
	private void WorldToCell(vector wp, out int col, out int row)
	{
		float dx = wp[0] - m_Origin[0];
		float dz = wp[2] - m_Origin[2];
		float along = dx * m_Dir[0] + dz * m_Dir[2];
		float across = dx * m_Side[0] + dz * m_Side[2];
		float rowFrac = along / (DM_GRID_HEX_SIZE * 1.5);
		float colFrac = across / (DM_GRID_HEX_SIZE * 0.8660254);
		SnapToCell(colFrac, rowFrac, col, row);
	}

	//! Round fractional doubled coords to the nearest valid (even col+row) cell.
	//! A plain Round of col/row can land on an odd-parity "gap" slot (~half the
	//! plane); the correction moves the coordinate with the larger rounding error
	//! by one toward the true fractional position so the result is a real cell.
	private void SnapToCell(float colFrac, float rowFrac, out int col, out int row)
	{
		col = RoundF(colFrac);
		row = RoundF(rowFrac);
		float dcol = colFrac - (float)col;
		float drow = rowFrac - (float)row;
		if ((col + row) % 2 == 0)
			return;
		if (Math.AbsFloat(dcol) > Math.AbsFloat(drow))
		{
			if (dcol > 0.0)
				col = col + 1;
			else
				col = col - 1;
		}
		else
		{
			if (drow > 0.0)
				row = row + 1;
			else
				row = row - 1;
		}
	}

	private int RoundF(float v)
	{
		return Math.Floor(v + 0.5);
	}
}
