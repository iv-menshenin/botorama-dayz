//! dmRoadHoneycomb — asynchronous grid detour (honeycomb) for wide/compound road
//! obstructions that the box-obstacle check cannot route around.
//!
//! The honeycomb defines FREE territory and builds a smooth path through it. A
//! cell is a pointy-top hexagon (circumradius DM_GRID_HEX_SIZE) in DOUBLED
//! coordinates: col across (m_Side), row along (m_Dir). col = 2*q + r, row = r
//! in axial terms — the across offset is S*sqrt(3)/2*col, independent of row, so
//! the col=0 column stays exactly over the road for any row (no side drift).
//! RED = blocking object (even partially) or slope above ~15° or water;
//! GREEN = clear, solid, level. SAFE = GREEN with all 6 neighbours GREEN. The
//! territory is a band along the route direction `d`, growing forward row-by-row
//! until the road is clear again (DM_GRID_CLEAR_ROWS consecutive all-green road
//! rows). The route is a CENTERLINE: one waypoint per row, snapped to the middle
//! of the widest walkable (GREEN/SAFE) corridor across the road ("effective
//! center"), so a partially blocked road pushes the line toward the open side.
//! The centerline is then smoothed (string-pulling: cut collinear points, go
//! straight while the segment does not cross RED cells). The pipeline runs
//! COLORING (one sub-step per tick: slope / water / one raycast) → MARK_SAFE
//! (DM_GRID_ROWS_PER_TICK rows per tick) → a bounded synchronous centerline +
//! smoothing pass (O(rows×width) + O(n) string-pulling), so per-tick spikes stay
//! minimal.

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
	//! Road-lane half width (m) for the clear-row check: only cells within ±3 m of
	//! the centerline count; roadside trees outside the lane are ignored.
	static const float DM_GRID_ROAD_HALF_WIDTH = 3.0;
	//! Shoulder half width (m) — half of the road's full width added on each side
	//! as the driveable gutter. Territory half width = road + shoulder = 6 m.
	static const float DM_GRID_SHOULDER_HALF_WIDTH = 3.0;
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
	//! Consecutive all-green road rows after which the territory stops growing
	//! ("the road is clear again").
	static const int DM_GRID_CLEAR_ROWS = 10;
	//! Territory length hard cap (rows, ~82 m at 0.5 m hex) — a safety net if the
	//! road never clears; the adaptive stop normally ends coloring far earlier.
	static const int DM_GRID_MAX_ROWS = 110;

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
	private vector m_CarPos;
	private vector m_Dir;
	private vector m_Side;
	private vector m_Origin;
	private int m_HalfWidth;
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
	private ref array<int> m_PathCells;
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
		m_CarPos = vector.Zero;
		m_Dir = Vector(1.0, 0.0, 0.0);
		m_Side = Vector(0.0, 0.0, 1.0);
		m_Origin = vector.Zero;
		m_HalfWidth = 0;
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
		m_PathCells = null;
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
	//! стопа); widthCells > 0 — ширина территории в doubled-колонках (иначе дефолт
	//! из метров: (road + shoulder) / across-step). Диагностический прогон
	//! (griddump) передаёт 0/null в carObj/driverObj — соты работают без машины.
	void Start(vector carPos, vector dir, array<vector> route, int routeIdx, Object carObj, Object driverObj, float lengthMeters, int widthCells)
	{
		int i;
		m_State = STATE_IDLE;
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
		if (widthCells > 0)
			m_HalfWidth = widthCells;
		else
			m_HalfWidth = RoundF((DM_GRID_ROAD_HALF_WIDTH + DM_GRID_SHOULDER_HALF_WIDTH) / (DM_GRID_HEX_SIZE * 0.8660254));
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
		m_PathCells = null;
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

	//! The smoothed path (world coordinates), or false when not DONE.
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

	//! Final smoothed waypoint cells (cell indices, same order as GetPath) — the
	//! effective-center line after string-pulling, for the E2E grid dump.
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
			BuildCenterlineAndSmooth();
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

	//! Goal = the deepest route waypoint row inside the territory; fallback: the
	//! far row (m_RowCount - 1). Only the row matters — the centerline walks row
	//! by row and picks each row's effective center.
	private bool FindGoalWaypoint()
	{
		if (!m_Route)
			return false;
		int goalRow = m_RowCount - 1;
		int bestRow = -1;
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
				bestRow = row;
		}
		if (bestRow >= 0)
			goalRow = bestRow;
		m_GoalRow = goalRow;
		return true;
	}

	//! Build the centerline, then smooth it into waypoints. Goal = the deepest
	//! route waypoint inside the territory (FindGoalWaypoint), snapped per-row to
	//! the effective center of the walkable corridor; on any failure the detour
	//! is marked FAILED.
	private void BuildCenterlineAndSmooth()
	{
		if (!FindGoalWaypoint())
		{
			m_State = STATE_FAILED;
			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[GRID] FAILED goal");
			#endif
			return;
		}
		if (!BuildCenterline())
		{
			m_State = STATE_FAILED;
			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[GRID] FAILED centerline");
			#endif
			return;
		}
		SmoothPath();
	}

	//! One cell per row (0..m_GoalRow), each at the effective center of that row's
	//! walkable corridor. prevCol carries continuity from the previous row (start
	//! at col=0, the road centerline).
	private bool BuildCenterline()
	{
		m_PathCells = new array<int>();
		int prevCol = 0;
		int centerCol;
		int row;
		for (row = 0; row <= m_GoalRow; row++)
		{
			if (!CenterColForRow(row, prevCol, centerCol))
				return false;
			m_PathCells.Insert(CellIndex(centerCol, row));
			prevCol = centerCol;
		}
		return true;
	}

	//! Effective center of one row: split the walkable (GREEN/SAFE) valid slots
	//! into connected intervals (step 2 apart), pick the interval containing
	//! prevCol (else the interval whose middle is closest), then step toward that
	//! interval's middle by at most one column. prevCol is never a valid cell of
	//! this row (adjacent rows have opposite column parity), so a ±1 step is the
	//! forward-diagonal neighbour — this keeps the centerline a connected hex path.
	//! If the gradual ±1 step is blocked (RED / out of bounds — the corridor gap is
	//! wider than one cell, e.g. a barrier's end face), we jump to the interval
	//! middle instead; that jump's segment is validated by SmoothPath.
	private bool CenterColForRow(int row, int prevCol, out int centerCol)
	{
		centerCol = prevCol;
		int col0 = -m_HalfWidth;
		if ((col0 + row) % 2 != 0)
			col0 = col0 + 1;

		bool haveInterval = false;
		int chosenFirst = 0;
		int chosenLast = 0;
		float chosenDist = 0.0;

		int col = col0;
		int first;
		int last;
		int c;
		float mid;
		float dist;
		int target;
		int cand;
		while (col <= m_HalfWidth)
		{
			if (!IsWalkable(m_Cells[CellIndex(col, row)]))
			{
				col = col + 2;
				continue;
			}
			first = col;
			last = col;
			c = col + 2;
			while (c <= m_HalfWidth && IsWalkable(m_Cells[CellIndex(c, row)]))
			{
				last = c;
				c = c + 2;
			}
			if (prevCol >= first && prevCol <= last)
			{
				chosenFirst = first;
				chosenLast = last;
				haveInterval = true;
				break;
			}
			mid = (float)(first + last) * 0.5;
			dist = Math.AbsFloat(mid - (float)prevCol);
			if (!haveInterval || dist < chosenDist)
			{
				haveInterval = true;
				chosenFirst = first;
				chosenLast = last;
				chosenDist = dist;
			}
			col = c;
		}
		if (!haveInterval)
		{
			centerCol = prevCol;
			return false;
		}
		target = SnapInterval(chosenFirst, chosenLast, prevCol);
		//! Постепенный шаг к середине коридора (≤1 колонки): связный hex-путь.
		if (Math.AbsFloat((float)(target - prevCol)) <= 1.0)
			cand = target;
		else if (target > prevCol)
			cand = prevCol + 1;
		else
			cand = prevCol - 1;
		//! Если постепенный шаг заблокирован (RED/за границей — разрыв коридора шире
		//! 1 соты, напр. торцевая грань барьера), прыгаем к середине интервала target
		//! (target всегда walkable — он внутри выбранного интервала). Отрезок прыжка
		//! валидирует SmoothPath (SegmentCrossesRed).
		if (cand >= -m_HalfWidth && cand <= m_HalfWidth && IsWalkable(m_Cells[CellIndex(cand, row)]))
			centerCol = cand;
		else
			centerCol = target;
		return true;
	}

	//! The valid cell of [first..last] nearest the interval's middle (step 2);
	//! when the middle falls between two cells, pick the one closer to prevCol.
	private int SnapInterval(int first, int last, int prevCol)
	{
		int span = (last - first) / 2;
		int lowK = span / 2;
		int highK = span - lowK;
		if (lowK == highK)
			return first + 2 * lowK;
		int lowCol = first + 2 * lowK;
		int highCol = first + 2 * highK;
		float dLow = Math.AbsFloat((float)lowCol - (float)prevCol);
		float dHigh = Math.AbsFloat((float)highCol - (float)prevCol);
		if (dLow <= dHigh)
			return lowCol;
		return highCol;
	}

	//! String-pulling over the walkable centerline: cut collinear points, and
	//! advance each segment straight while it does not cross a RED cell.
	private void SmoothPath()
	{
		m_Waypoints = new array<vector>();
		m_WaypointCells = new array<int>();
		int n = m_PathCells.Count();
		int anchor;
		int i;
		int emit;
		if (n == 0)
		{
			m_State = STATE_FAILED;
			return;
		}
		m_Waypoints.Insert(CellWorld(m_PathCells[0]));
		m_WaypointCells.Insert(m_PathCells[0]);
		if (n == 1)
		{
			m_State = STATE_DONE;
			return;
		}
		anchor = 0;
		i = 1;
		while (i < n)
		{
			//! Тянем прямую от anchor, пока отрезок anchor→i не пересекает RED.
			while (i < n && !SegmentCrossesRed(m_PathCells[anchor], m_PathCells[i]))
				i = i + 1;
			//! anchor→i теперь пересекает RED (или i==n). Выпускаем path[i-1] — последнюю чистую точку.
			emit = i - 1;
			if (emit == anchor)
				emit = anchor + 1;
			m_Waypoints.Insert(CellWorld(m_PathCells[emit]));
			m_WaypointCells.Insert(m_PathCells[emit]);
			anchor = emit;
			i = emit + 1;
		}
		m_State = STATE_DONE;

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[GRID] DONE path=" + m_Waypoints.Count());
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
		return Math.AbsFloat(across) <= DM_GRID_ROAD_HALF_WIDTH;
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
