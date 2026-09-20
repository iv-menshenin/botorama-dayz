//! dmRoadHoneycomb — asynchronous grid detour (honeycomb) for wide/compound road
//! obstructions that the box-obstacle check cannot route around.
//!
//! The honeycomb defines FREE territory and builds a smooth path through it. A
//! cell is 1/3 of the car's width (DM_GRID_CELL_SIZE). RED = blocking object
//! (even partially) or slope above ~15° or water; GREEN = clear, solid, level.
//! SAFE = GREEN with all 8 neighbours GREEN (three GREEN across = car width).
//! The territory is a rectangle along the route direction `d`, growing forward
//! row-by-row (DM_GRID_ROWS_PER_TICK per tick) until the road is clear again
//! (DM_GRID_CLEAR_ROWS consecutive all-green road rows). A* runs over SAFE cells,
//! then the path is smoothed (string-pulling: cut collinear points, go straight
//! while the segment does not cross RED cells). Coloring is spread over ticks;
//! mark-safe and pathfinding run in a single tick each.

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

	//! Cell size (meters) = 1/3 of the car's width (~2 m).
	static const float DM_GRID_CELL_SIZE = 0.66;
	//! Diagonal step cost (cell * sqrt(2)).
	static const float DM_GRID_DIAG_COST = 0.933;
	//! Slope: tan(15°) — a cell whose corner heights differ more than tan*hdist is
	//! too steep (red). Avoids a trig call per corner pair.
	static const float DM_GRID_SLOPE_TAN = 0.267949;
	//! Territory width in cells (3-4 road widths; ~20 m at 0.66 m/cell).
	static const int DM_GRID_WIDTH_CELLS = 30;
	//! Rows colored per tick (the expensive SurfaceY/raycast work is spread).
	static const int DM_GRID_ROWS_PER_TICK = 3;
	//! Низ диагонального луча соты над землёй (м) — высота колеса: проезжает
	//! низкие бордюры, но цепляет бетонные ограждения и кузов машины.
	static const float DM_GRID_RAY_BOTTOM = 0.15;
	//! Верх диагонального луча соты над землёй (м) — высота среднего авто (~1.5 м):
	//! сноп из 6 диагоналей сходится в центре соты на ~0.825 м.
	static const float DM_GRID_RAY_TOP = 1.5;
	//! Радиус капсулы луча клетки (м) — тонкий (0.2): сноп из 6 диагоналей сам
	//! покрывает площадь соты, толстый радиус размазал бы его в «бочки».
	static const float DM_GRID_RAY_RADIUS = 0.2;
	//! Consecutive all-green road rows after which the territory stops growing
	//! ("the road is clear again").
	static const int DM_GRID_CLEAR_ROWS = 10;
	//! Road-lane half width (m) for the clear-row check: only cells within ±3 m of
	//! the centerline count; roadside trees outside the lane are ignored.
	static const float DM_GRID_ROAD_HALF_WIDTH = 3.0;
	//! Territory length hard cap (rows, ~60 m) — a safety net if the road never
	//! clears; the adaptive stop normally ends coloring far earlier.
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
	}

	//! Start a detour: car position, route direction (horizontal), the route
	//! waypoints and the index of the next waypoint to reach. Resets all state.
	//! lengthMeters > 0 — фиксированная длина территории (м, без адаптивного
	//! стопа); widthCells > 0 — ширина территории в клетках (иначе дефолт
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
			m_HalfWidth = widthCells / 2;
		else
			m_HalfWidth = DM_GRID_WIDTH_CELLS / 2;
		m_Width = m_HalfWidth * 2 + 1;
		if (lengthMeters > 0.0)
		{
			m_RowCount = Math.Floor(lengthMeters / DM_GRID_CELL_SIZE);
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
	int GetCellColor(int row, int col)
	{
		return m_Cells[CellIndex(row, col)];
	}

	//! World XZ center of a cell (Y = territory origin height — dump only).
	vector GetCellCenter(int row, int col)
	{
		return CellCenter(row, col);
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
	private void ColorRow(int row)
	{
		bool roadClear = true;
		int col;
		for (col = -m_HalfWidth; col <= m_HalfWidth; col++)
		{
			vector center = CellCenter(row, col);
			int color = ColorCell(center[0], center[2]);
			int cidx = CellIndex(row, col);
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
		float h = DM_GRID_CELL_SIZE * 0.5;
		float edgeLimit = DM_GRID_SLOPE_TAN * DM_GRID_CELL_SIZE;
		float diagLimit = edgeLimit * 1.414214;
		float y0 = GetGame().SurfaceY(cx - h, cz - h);
		float y1 = GetGame().SurfaceY(cx + h, cz - h);
		float y2 = GetGame().SurfaceY(cx - h, cz + h);
		float y3 = GetGame().SurfaceY(cx + h, cz + h);
		float groundY = (y0 + y1 + y2 + y3) * 0.25;

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

		if (RaycastBlocked(cx, groundY, cz))
			return CELL_RED;

		return CELL_GREEN;
	}

	//! Fan of 6 diagonal rays through the cell's hexagon: each ray runs from a
	//! corner at car-roof height (groundY + DM_GRID_RAY_TOP) DOWN to the opposite
	//! corner at wheel height (groundY + DM_GRID_RAY_BOTTOM), sweeping 0.15 m .. 1.5 m
	//! and converging at ~0.825 m in the center. Starting at the top keeps the ray
	//! OUTSIDE a low collider (0..0.5 m) so it actually reports a hit. Catches low
	//! concrete barriers and a car body the old single horizontal body-height ray
	//! missed. Ground (obj == null) is skipped — the car drives on it; anything
	//! with a script object (barrier, car body, trunk) is a hit.
	private bool RaycastBlocked(float cx, float groundY, float cz)
	{
		//! Сноп из 6 диагоналей через шестигранник соты: углы в XZ вокруг центра
		//! клетки (R = DM_GRID_CELL_SIZE), луч i идёт СВЕРХУ от угла i на высоте крыши
		//! к противоположному углу (i+3)%6 на высоте колеса (стартует снаружи
		//! коллайдера, а не внутри низкого объекта). Земля/вода (obj == null)
		//! пропускается; всё с script-объектом (барьер, кузов, ствол) — препятствие.
		//! pIgnore = m_CarObj: машина и водитель исключаются.
		ref array<vector> corners = new array<vector>();
		ref array<ref RaycastRVResult> vHits;
		RaycastRVParams vParams;
		RaycastRVResult vHit;
		vector vFrom;
		vector vTo;
		float radius = DM_GRID_CELL_SIZE;
		float ang;
		float ca;
		float sa;
		float dx;
		float dz;
		int k;
		int opp;
		int i;
		for (k = 0; k < 6; k++)
		{
			ang = (float)k * 60.0 * Math.DEG2RAD;
			ca = radius * Math.Cos(ang);
			sa = radius * Math.Sin(ang);
			dx = m_Dir[0] * ca + m_Side[0] * sa;
			dz = m_Dir[2] * ca + m_Side[2] * sa;
			corners.Insert(Vector(cx + dx, 0.0, cz + dz));
		}
		for (k = 0; k < 6; k++)
		{
			opp = (k + 3) % 6;
			vFrom = Vector(corners[k][0], groundY + DM_GRID_RAY_TOP, corners[k][2]);
			vTo = Vector(corners[opp][0], groundY + DM_GRID_RAY_BOTTOM, corners[opp][2]);
			vParams = new RaycastRVParams(vFrom, vTo, m_CarObj, DM_GRID_RAY_RADIUS);
			vParams.flags = CollisionFlags.ALLOBJECTS;
			vParams.type = ObjIntersectGeom;
			vHits = new array<ref RaycastRVResult>();
			if (!DayZPhysics.RaycastRVProxy(vParams, vHits))
				continue;
			for (i = 0; i < vHits.Count(); i++)
			{
				vHit = vHits[i];
				if (vHit.obj == m_CarObj || vHit.parent == m_CarObj)
					continue;
				if (m_DriverObj && (vHit.obj == m_DriverObj || vHit.parent == m_DriverObj))
					continue;
				if (vHit.obj == null)
					continue;
				#ifdef DM_BOT_DEBUG_ROADS
				dmBotLog.Debug("[GRID] blocked: pos=" + vHit.pos + " ray=" + k);
				#endif
				return true;
			}
		}
		return false;
	}

	//! Mark SAFE: a GREEN cell with all 8 neighbours walkable (GREEN or SAFE).
	private void MarkSafe()
	{
		int row;
		int col;
		for (row = 0; row < m_RowCount; row++)
		{
			for (col = -m_HalfWidth; col <= m_HalfWidth; col++)
			{
				int idx = CellIndex(row, col);
				if (m_Cells[idx] != CELL_GREEN)
					continue;
				if (AllNeighborsWalkable(row, col))
					m_Cells[idx] = CELL_SAFE;
			}
		}
		m_State = STATE_PATHFINDING;
	}

	private bool AllNeighborsWalkable(int row, int col)
	{
		int dr;
		int dc;
		for (dr = -1; dr <= 1; dr++)
		{
			for (dc = -1; dc <= 1; dc++)
			{
				if (dr == 0 && dc == 0)
					continue;
				int nr = row + dr;
				int nc = col + dc;
				if (nr < 0 || nr >= m_RowCount || nc < -m_HalfWidth || nc > m_HalfWidth)
					return false;
				if (!IsWalkable(m_Cells[CellIndex(nr, nc)]))
					return false;
			}
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
			int row = WorldToRow(wp);
			int col = WorldToCol(wp);
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
		m_GoalCell = FindNearestSafe(goalRow, goalCol);
		if (m_GoalCell < 0)
			return false;
		m_StartCell = FindNearestSafe(0, 0);
		if (m_StartCell < 0)
			return false;
		return true;
	}

	//! A* over SAFE cells (8-directional) from the start (nearest safe to the car)
	//! to the goal, using a binary min-heap and an octile heuristic.
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
		int r;
		int c;
		int dr;
		int dc;
		int nr;
		int nc;
		int nIdx;
		int nb;
		float fPop;
		float gu;
		float nd;
		float stepCost;
		while (heap.Count() > 0)
		{
			heap.Pop(cur, fPop);
			if (settled[cur])
				continue;
			settled[cur] = true;
			if (cur == m_GoalCell)
				break;
			gu = g[cur];
			r = CellRow(cur);
			c = CellCol(cur);
			for (dr = -1; dr <= 1; dr++)
			{
				for (dc = -1; dc <= 1; dc++)
				{
					if (dr == 0 && dc == 0)
						continue;
					nr = r + dr;
					nc = c + dc;
					if (nr < 0 || nr >= m_RowCount || nc < -m_HalfWidth || nc > m_HalfWidth)
						continue;
					nIdx = CellIndex(nr, nc);
					if (m_Cells[nIdx] != CELL_SAFE)
						continue;
					if (settled[nIdx])
						continue;
					if (dr != 0 && dc != 0)
						stepCost = DM_GRID_DIAG_COST;
					else
						stepCost = DM_GRID_CELL_SIZE;
					nd = gu + stepCost;
					if (nd < g[nIdx])
					{
						g[nIdx] = nd;
						parent[nIdx] = cur;
						heap.Push(nIdx, nd + Heuristic(nIdx));
					}
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

	//! Octile heuristic (admissible for 8-directional): min(dr,dc) diagonal steps
	//! at DM_GRID_DIAG_COST plus the remainder at DM_GRID_CELL_SIZE.
	private float Heuristic(int idx)
	{
		int r = CellRow(idx);
		int c = CellCol(idx);
		int dr = r - m_GoalRow;
		int dc = c - m_GoalCol;
		if (dr < 0)
			dr = -dr;
		if (dc < 0)
			dc = -dc;
		int dmin;
		int dmax;
		if (dr < dc)
		{
			dmin = dr;
			dmax = dc;
		}
		else
		{
			dmin = dc;
			dmax = dr;
		}
		return (float)dmin * DM_GRID_DIAG_COST + (float)(dmax - dmin) * DM_GRID_CELL_SIZE;
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
	//! cell (sampled at half-cell steps in cell space).
	private bool SegmentCrossesRed(int a, int b)
	{
		float ra = CellRow(a);
		float ca = CellCol(a);
		float rb = CellRow(b);
		float cb = CellCol(b);
		float dr = rb - ra;
		float dc = cb - ca;
		float len = Math.Sqrt(dr * dr + dc * dc);
		int steps = Math.Floor(len * 2.0 + 0.5) + 1;
		float stepT = 1.0 / (float)steps;
		float t = stepT;
		int i;
		for (i = 1; i < steps; i++)
		{
			float fr = ra + dr * t;
			float fc = ca + dc * t;
			int r = Math.Floor(fr + 0.5);
			int c = Math.Floor(fc + 0.5);
			if (m_Cells[CellIndex(r, c)] == CELL_RED)
				return true;
			t = t + stepT;
		}
		return false;
	}

	//! Nearest SAFE cell to (row, col), searched by expanding rings.
	private int FindNearestSafe(int row, int col)
	{
		int radius;
		int dr;
		int dc;
		for (radius = 0; radius <= m_RowCount + m_HalfWidth; radius++)
		{
			for (dr = -radius; dr <= radius; dr++)
			{
				for (dc = -radius; dc <= radius; dc++)
				{
					if (dr != radius && dc != radius && dr != -radius && dc != -radius)
						continue;
					int nr = row + dr;
					int nc = col + dc;
					if (nr < 0 || nr >= m_RowCount || nc < -m_HalfWidth || nc > m_HalfWidth)
						continue;
					if (m_Cells[CellIndex(nr, nc)] == CELL_SAFE)
						return CellIndex(nr, nc);
				}
			}
		}
		return -1;
	}

	//! World XZ position of a cell center (Y is the car's height; callers that need
	//! ground height use CellWorld).
	private vector CellCenter(int row, int col)
	{
		float along = (float)row * DM_GRID_CELL_SIZE;
		float across = (float)col * DM_GRID_CELL_SIZE;
		return m_Origin + m_Dir * along + m_Side * across;
	}

	//! World position of a cell center snapped to the ground (SurfaceY).
	private vector CellWorld(int idx)
	{
		vector p = CellCenter(CellRow(idx), CellCol(idx));
		p[1] = GetGame().SurfaceY(p[0], p[2]);
		return p;
	}

	private int CellIndex(int row, int col)
	{
		return row * m_Width + col + m_HalfWidth;
	}

	private int CellRow(int idx)
	{
		return idx / m_Width;
	}

	private int CellCol(int idx)
	{
		int r = idx / m_Width;
		return idx - r * m_Width - m_HalfWidth;
	}

	//! True when the column is within the road lane (for the clear-row check).
	private bool IsRoadCol(int col)
	{
		return Math.AbsFloat((float)col * DM_GRID_CELL_SIZE) <= DM_GRID_ROAD_HALF_WIDTH;
	}

	private int WorldToRow(vector wp)
	{
		float dx = wp[0] - m_Origin[0];
		float dz = wp[2] - m_Origin[2];
		float along = dx * m_Dir[0] + dz * m_Dir[2];
		int row = Math.Floor(along / DM_GRID_CELL_SIZE + 0.5);
		return row;
	}

	private int WorldToCol(vector wp)
	{
		float dx = wp[0] - m_Origin[0];
		float dz = wp[2] - m_Origin[2];
		float across = dx * m_Side[0] + dz * m_Side[2];
		int col = Math.Floor(across / DM_GRID_CELL_SIZE + 0.5);
		return col;
	}
}
