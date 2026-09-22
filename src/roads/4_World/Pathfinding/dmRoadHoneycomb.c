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
//! (DM_GRID_ROWS_PER_TICK per tick) until the road is clear again
//! (DM_GRID_CLEAR_ROWS consecutive all-green road rows). A* runs over SAFE cells
//! (6 doubled neighbours, doubled-distance heuristic), then the path is smoothed
//! (string-pulling: cut collinear points, go straight while the segment does not
//! cross RED cells). Coloring is spread over ticks; mark-safe and pathfinding
//! run in a single tick each.

class dmRoadHoneycomb
{
	static ref dmRoadHoneycomb s_Instance;

	//! Async pipeline states.
	static const int STATE_IDLE = 0;
	static const int STATE_COLORING = 1;
	static const int STATE_MARK_SAFE = 2;
	static const int STATE_PATHFINDING = 3;
	static const int STATE_DONE = 4;
	static const int STATE_FAILED = 5;

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
	//! Territory width in DOUBLED cols across the road (half of them empty slots;
	//! 30 cols each side ≈ ±13 m at 0.5 m hex, S*sqrt(3)/2 = 0.433 per col).
	static const int DM_GRID_WIDTH_CELLS = 30;
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
	//! Road-lane half width (m) for the clear-row check: only cells within ±3 m of
	//! the centerline count; roadside trees outside the lane are ignored.
	static const float DM_GRID_ROAD_HALF_WIDTH = 3.0;
	//! Territory length hard cap (rows, ~82 m at 0.5 m hex) — a safety net if the
	//! road never clears; the adaptive stop normally ends coloring far earlier.
	static const int DM_GRID_MAX_ROWS = 110;
	//! A* sentinel cost.
	static const float DM_GRID_INF = 1000000000.0;

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
	//! 6 doubled neighbours (pointy-top) as parallel dcol/drow offset tables,
	//! shared by MarkSafe/AllNeighborsWalkable/A*/FindNearestSafe:
	//! (+2,0) (-2,0) (+1,-1) (-1,-1) (+1,+1) (-1,+1).
	private ref array<int> m_NeighDCol;
	private ref array<int> m_NeighDRow;
	private ref array<int> m_Cells;
	private ref array<vector> m_Route;
	private int m_RouteIdx;
	private int m_StartCell;
	private int m_GoalCell;
	private int m_GoalRow;
	private int m_GoalCol;
	private ref array<int> m_PathCells;
	private ref array<vector> m_Waypoints;
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
		m_Cells = null;
		m_Route = null;
		m_RouteIdx = 0;
		m_StartCell = -1;
		m_GoalCell = -1;
		m_GoalRow = 0;
		m_GoalCol = 0;
		m_PathCells = null;
		m_Waypoints = null;
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
	//! DM_GRID_WIDTH_CELLS). Диагностический прогон (griddump) передаёт 0/null
	//! в carObj/driverObj — соты работают без машины и водителя.
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
			m_HalfWidth = DM_GRID_WIDTH_CELLS;
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
		m_RouteIdx = routeIdx;
		m_GoalRow = 0;
		m_GoalCol = 0;
		m_StartCell = -1;
		m_GoalCell = -1;
		m_PathCells = null;
		m_Waypoints = null;

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

	//! Advance the pipeline. Coloring runs DM_GRID_ROWS_PER_TICK rows; mark-safe
	//! and pathfinding each run in one tick.
	void Tick(float dt)
	{
		if (m_State == STATE_COLORING)
			TickColor();
		else if (m_State == STATE_MARK_SAFE)
			MarkSafe();
		else if (m_State == STATE_PATHFINDING)
			Pathfind();
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

	//! Color up to DM_GRID_ROWS_PER_TICK rows forward; stop growing the territory
	//! as soon as the road-lane has been clear for DM_GRID_CLEAR_ROWS rows.
	private void TickColor()
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.GridColor");
		#endif

		int rowsDone = 0;
		while (rowsDone < DM_GRID_ROWS_PER_TICK && m_ColorRow < m_RowCount)
		{
			ColorRow(m_ColorRow);
			m_ColorRow = m_ColorRow + 1;
			rowsDone = rowsDone + 1;
			if (!m_FixedLength && m_RoadClearRun >= DM_GRID_CLEAR_ROWS)
			{
				m_RowCount = m_ColorRow;
				break;
			}
		}
		if (m_ColorRow >= m_RowCount)
			m_State = STATE_MARK_SAFE;
	}

	//! Color one row (across the territory width) and update the road-clear run.
	//! Only valid (even col+row) cells are colored; odd-parity slots stay UNSET.
	private void ColorRow(int r)
	{
		bool roadClear = true;
		int col;
		for (col = -m_HalfWidth; col <= m_HalfWidth; col++)
		{
			if ((col + r) % 2 != 0)
				continue;
			vector center = CellCenter(col, r);
			int color = ColorCell(center[0], center[2]);
			int cidx = CellIndex(col, r);
			m_Cells[cidx] = color;
			if (IsRoadCol(col) && color != CELL_GREEN)
				roadClear = false;
		}
		if (roadClear)
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
	}

	//! Classify one cell (RED/GREEN). The diagonal ray fan catches the vertical
	//! face of a barrier/trunk (horizontal surface normal); a steep slope
	//! or water marks the cell red.
	private int ColorCell(float cx, float cz)
	{
		float h = DM_GRID_HEX_SIZE * 0.5;
		float edgeLimit = DM_GRID_SLOPE_TAN * DM_GRID_HEX_SIZE;
		float diagLimit = edgeLimit * 1.414214;
		float y0 = GetGame().SurfaceY(cx - h, cz - h);
		float y1 = GetGame().SurfaceY(cx + h, cz - h);
		float y2 = GetGame().SurfaceY(cx - h, cz + h);
		float y3 = GetGame().SurfaceY(cx + h, cz + h);

		if (Math.AbsFloat(y0 - y1) > edgeLimit)
			return CELL_RED;
		if (Math.AbsFloat(y0 - y2) > edgeLimit)
			return CELL_RED;
		if (Math.AbsFloat(y3 - y1) > edgeLimit)
			return CELL_RED;
		if (Math.AbsFloat(y3 - y2) > edgeLimit)
			return CELL_RED;
		if (Math.AbsFloat(y0 - y3) > diagLimit)
			return CELL_RED;
		if (Math.AbsFloat(y1 - y2) > diagLimit)
			return CELL_RED;

		if (GetGame().SurfaceIsSea(cx, cz) || GetGame().SurfaceIsPond(cx, cz))
			return CELL_RED;

		if (RaycastBlocked(cx, cz))
			return CELL_RED;

		return CELL_GREEN;
	}

	//! Гексагональный веер тонких лучей (pointy-top hexagon в плоскости m_Dir/m_Side,
	//! центр O=(cx,cz), циркумрадиус DM_GRID_HEX_SIZE): 3 диагонали top→bottom,
	//! upper-left→lower-right, upper-right→lower-left (верхние вершины начинаются
	//! на высоте DM_GRID_RAY_HORIZ_HEIGHT_BEG над локальным террейном вершины,
	//! нижние — на высоте DM_GRID_RAY_HORIZ_HEIGHT_END) + 1 вертикаль
	//! в O сверху (groundY + DM_GRID_RAY_TOP) вниз до (groundY + DM_GRID_RAY_BOTTOM).
	//! Все лучи ТОНКИЕ (radius 0), type = ObjIntersectGeom (ловит baked-static
	//! ограждения). Хит с физической коллизией (hit.obj != null) — препятствие;
	//! null-хиты (без коллизии) отбрасываются.
	private bool RaycastBlocked(float cx, float cz)
	{
		float s = DM_GRID_HEX_SIZE;
		float sCos = s * 0.8660254;
		float sHalf = s * 0.5;
		vector o = Vector(cx, 0.0, cz);
		vector top = o + m_Dir * s;
		vector bot = o - m_Dir * s;
		vector ur = o + m_Side * sCos + m_Dir * sHalf;
		vector ul = o - m_Side * sCos + m_Dir * sHalf;
		vector lr = o + m_Side * sCos - m_Dir * sHalf;
		vector ll = o - m_Side * sCos - m_Dir * sHalf;
		top[1] = GetGame().SurfaceY(top[0], top[2]) + DM_GRID_RAY_HORIZ_HEIGHT_BEG;
		ur[1] = GetGame().SurfaceY(ur[0], ur[2]) + DM_GRID_RAY_HORIZ_HEIGHT_BEG;
		ul[1] = GetGame().SurfaceY(ul[0], ul[2]) + DM_GRID_RAY_HORIZ_HEIGHT_BEG;
		bot[1] = GetGame().SurfaceY(bot[0], bot[2]) + DM_GRID_RAY_HORIZ_HEIGHT_END;
		lr[1] = GetGame().SurfaceY(lr[0], lr[2]) + DM_GRID_RAY_HORIZ_HEIGHT_END;
		ll[1] = GetGame().SurfaceY(ll[0], ll[2]) + DM_GRID_RAY_HORIZ_HEIGHT_END;
		float groundY = GetGame().SurfaceY(cx, cz);
		vector vFrom = Vector(cx, groundY + DM_GRID_RAY_TOP, cz);
		vector vTo = Vector(cx, groundY + DM_GRID_RAY_BOTTOM, cz);
		if (RayHitsObstacle(top, bot))
			return true;
		if (RayHitsObstacle(ul, lr))
			return true;
		if (RayHitsObstacle(ur, ll))
			return true;
		if (RayHitsObstacle(vFrom, vTo))
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

	//! Mark SAFE: a GREEN cell with all 6 neighbours walkable (GREEN or SAFE).
	//! Only valid (even col+row) cells are considered.
	private void MarkSafe()
	{
		int r;
		int col;
		for (r = 0; r < m_RowCount; r++)
		{
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
		m_State = STATE_PATHFINDING;
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

	//! Goal + A* + smoothing.
	private void Pathfind()
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Road.GridPath");
		#endif

		if (!FindGoal())
		{
			m_State = STATE_FAILED;
			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[GRID] FAILED goal");
			#endif
			return;
		}
		if (!FindPathAStar())
		{
			m_State = STATE_FAILED;
			#ifdef DM_BOT_DEBUG_ROADS
			dmBotLog.Debug("[GRID] FAILED astar");
			#endif
			return;
		}
		SmoothPath();
	}

	//! Goal = the route waypoint deepest inside the territory (the "far side"),
	//! snapped to the nearest SAFE cell. Fallback: the far-center cell.
	private bool FindGoal()
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
			if (col < -m_HalfWidth || col > m_HalfWidth)
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
		m_GoalCell = FindNearestSafe(goalCol, goalRow);
		if (m_GoalCell < 0)
			return false;
		m_StartCell = FindNearestSafe(0, 0);
		if (m_StartCell < 0)
			return false;
		return true;
	}

	//! A* over SAFE cells (6-directional) from the start (nearest safe to the car)
	//! to the goal, using a binary min-heap and a doubled-distance heuristic.
	private bool FindPathAStar()
	{
		int cellCount = m_RowCount * m_Width;
		ref array<float> g = new array<float>();
		ref array<int> parent = new array<int>();
		ref array<bool> settled = new array<bool>();
		int i;
		for (i = 0; i < cellCount; i++)
		{
			g.Insert(DM_GRID_INF);
			parent.Insert(-1);
			settled.Insert(false);
		}
		g[m_StartCell] = 0.0;
		ref dmRouterMinHeap heap = new dmRouterMinHeap();
		heap.Push(m_StartCell, Heuristic(m_StartCell));

		int cur;
		int col;
		int row;
		int nb;
		int ncol;
		int nrow;
		int nIdx;
		float fPop;
		float gu;
		float nd;
		while (heap.Count() > 0)
		{
			heap.Pop(cur, fPop);
			if (settled[cur])
				continue;
			settled[cur] = true;
			if (cur == m_GoalCell)
				break;
			gu = g[cur];
			col = CellCol(cur);
			row = CellRow(cur);
			for (nb = 0; nb < m_NeighDCol.Count(); nb++)
			{
				ncol = col + m_NeighDCol[nb];
				nrow = row + m_NeighDRow[nb];
				if (ncol < -m_HalfWidth || ncol > m_HalfWidth || nrow < 0 || nrow >= m_RowCount)
					continue;
				nIdx = CellIndex(ncol, nrow);
				if (m_Cells[nIdx] != CELL_SAFE)
					continue;
				if (settled[nIdx])
					continue;
				nd = gu + DM_GRID_HEX_SIZE;
				if (nd < g[nIdx])
				{
					g[nIdx] = nd;
					parent[nIdx] = cur;
					heap.Push(nIdx, nd + Heuristic(nIdx));
				}
			}
		}

		if (!settled[m_GoalCell])
			return false;

		ref array<int> rev = new array<int>();
		cur = m_GoalCell;
		while (true)
		{
			rev.Insert(cur);
			if (cur == m_StartCell)
				break;
			nb = parent[cur];
			if (nb < 0)
			{
				dmBotLog.Error("[GRID] A* broken parent chain");
				return false;
			}
			cur = nb;
		}
		m_PathCells = new array<int>();
		for (i = rev.Count() - 1; i >= 0; i--)
			m_PathCells.Insert(rev[i]);
		return true;
	}

	//! Doubled hex distance heuristic (admissible for 6-directional): hex steps
	//! = |drow| + max(0, (|dcol| - |drow|)/2), times DM_GRID_HEX_SIZE (uniform
	//! step cost — the scale cancels and does not affect optimality).
	private float Heuristic(int idx)
	{
		int col = CellCol(idx);
		int row = CellRow(idx);
		int dcol = col - m_GoalCol;
		int drow = row - m_GoalRow;
		int adc = dcol;
		int adr = drow;
		if (adc < 0)
			adc = -adc;
		if (adr < 0)
			adr = -adr;
		float steps = (float)adr;
		if (adc > adr)
			steps = steps + ((float)adc - (float)adr) * 0.5;
		return steps * DM_GRID_HEX_SIZE;
	}

	//! String-pulling over the SAFE cell path: cut collinear points, and advance
	//! each segment straight while it does not cross a RED cell.
	private void SmoothPath()
	{
		m_Waypoints = new array<vector>();
		int n = m_PathCells.Count();
		if (n == 0)
		{
			m_State = STATE_FAILED;
			return;
		}
		m_Waypoints.Insert(CellWorld(m_PathCells[0]));
		if (n == 1)
		{
			m_State = STATE_DONE;
			return;
		}
		int anchor = 0;
		int front = 1;
		while (front < n)
		{
			while (front + 1 < n && !SegmentCrossesRed(m_PathCells[anchor], m_PathCells[front + 1]))
				front = front + 1;
			m_Waypoints.Insert(CellWorld(m_PathCells[front]));
			anchor = front;
			front = front + 1;
		}
		m_State = STATE_DONE;

		#ifdef DM_BOT_DEBUG_ROADS
		dmBotLog.Debug("[GRID] DONE path=" + m_Waypoints.Count());
		#endif
	}

	//! True when the straight segment between two SAFE cell centers crosses a RED
	//! cell (sampled at half-cell steps in doubled float space).
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

	//! Nearest SAFE cell to (col, row), searched by BFS over the 6-neighbor
	//! doubled adjacency (BFS distance == doubled distance on a uniform hex grid).
	private int FindNearestSafe(int col, int row)
	{
		int startIdx = CellIndex(col, row);
		if (m_Cells[startIdx] == CELL_SAFE)
			return startIdx;
		ref array<bool> visited = new array<bool>();
		int cellCount = m_RowCount * m_Width;
		int i;
		for (i = 0; i < cellCount; i++)
			visited.Insert(false);
		ref array<int> queue = new array<int>();
		queue.Insert(startIdx);
		visited[startIdx] = true;
		int head = 0;
		int cur;
		int ccol;
		int crow;
		int nb;
		int ncol;
		int nrow;
		int nIdx;
		while (head < queue.Count())
		{
			cur = queue[head];
			head = head + 1;
			ccol = CellCol(cur);
			crow = CellRow(cur);
			for (nb = 0; nb < m_NeighDCol.Count(); nb++)
			{
				ncol = ccol + m_NeighDCol[nb];
				nrow = crow + m_NeighDRow[nb];
				if (ncol < -m_HalfWidth || ncol > m_HalfWidth || nrow < 0 || nrow >= m_RowCount)
					continue;
				nIdx = CellIndex(ncol, nrow);
				if (visited[nIdx])
					continue;
				visited[nIdx] = true;
				if (m_Cells[nIdx] == CELL_SAFE)
					return nIdx;
				queue.Insert(nIdx);
			}
		}
		return -1;
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
