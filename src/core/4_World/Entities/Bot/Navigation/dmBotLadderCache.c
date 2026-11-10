//! dmBotLadderCache — перечисление лестниц здания через memory LOD.
//!
//! Ванильный Building.GetLaddersCount() у серверного ИИ возвращает 0, поэтому
//! лестницы парсим сами: берём memory LOD здания, перебираем selection'ы
//! "ladderN_con" (точки входа) и сортируем их по высоте Y в низ/верх; тип
//! анимации читаем из geometry LOD (property "laddertype"). Аналог Expansion
//! BuildingBase.Expansion_GetLaddersCount().
//!
//! Точки входа хранятся в model-space (здание может стоять в любом месте мира),
//! при использовании их конвертируют через Building.ModelToWorld(). Кэш ключуется
//! по типу здания: у всех экземпляров одного типа одинаковый memory LOD, поэтому
//! парсинг выполняется один раз на тип.

//! Одна лестница здания: индекс для StartCommand_Ladder, тип анимации и
//! точки входа в model-space. m_Vertices/m_VertDirs — ВСЕ вершины selections
//! "ladderN_con"/"ladderN_con_dir" (по одной на ярус многоярусной вышки),
//! отсортированы по высоте Y; m_Bottom/m_Top/m_BottomDir/m_TopDir — мин/макс
//! (совместимость с прежним кодом), заполняются из полного списка.
class dmBotLadder
{
	int m_Index;
	string m_Type;
	vector m_Bottom;
	vector m_Top;
	vector m_BottomDir;   // направление входа/выхода у нижней точки
	vector m_TopDir;      // направление входа/выхода у верхней точки
	ref array<vector> m_Vertices;   // все вершины _con (model-space), по Y
	ref array<vector> m_VertDirs;   // все вершины _con_dir (model-space), по Y

	//! Точка входа лестницы (world-space) на уровне worldPos: ближайшая вершина
	//! _con по Y. Если она дальше DM_FALL_DANGER_DROP по вертикали, но worldPos[1]
	//! внутри [bottomY, topY] — синтезируем проекцию worldPos на прямую лестницы
	//! (вышка, смоделированная 2 вершинами без промежуточной точки входа).
	vector GetEntryWorld(Building building, vector worldPos)
	{
		vector entry = vector.Zero;
		ref array<vector> verts = m_Vertices;
		if (verts && verts.Count() > 0)
		{
			vector bestWorld = building.ModelToWorld(verts[0]);
			float bestDy = Math.AbsFloat(bestWorld[1] - worldPos[1]);
			int vi;
			for (vi = 1; vi < verts.Count(); vi++)
			{
				vector w = building.ModelToWorld(verts[vi]);
				float dy = Math.AbsFloat(w[1] - worldPos[1]);
				if (dy < bestDy)
				{
					bestDy = dy;
					bestWorld = w;
				}
			}
			if (bestDy <= DM_FALL_DANGER_DROP)
				return bestWorld;
		}
		vector bottom = building.ModelToWorld(m_Bottom);
		vector top = building.ModelToWorld(m_Top);
		float span = top[1] - bottom[1];
		if (Math.AbsFloat(span) < 0.001)
			return bottom;
		float t = (worldPos[1] - bottom[1]) / span;
		entry = bottom + (top - bottom) * t;
		return entry;
	}

	//! Направление входа/выхода (world-space), парное точке entryWorld: ближайшая
	//! вершина _con_dir по дистанции (парность по близости, как у ванили).
	vector GetDirWorld(Building building, vector entryWorld)
	{
		vector dirPoint = vector.Zero;
		ref array<vector> dirs = m_VertDirs;
		if (dirs && dirs.Count() > 0)
		{
			vector bestWorld = building.ModelToWorld(dirs[0]);
			float bestDist = vector.DistanceSq(bestWorld, entryWorld);
			int vi;
			for (vi = 1; vi < dirs.Count(); vi++)
			{
				vector w = building.ModelToWorld(dirs[vi]);
				float d = vector.DistanceSq(w, entryWorld);
				if (d < bestDist)
				{
					bestDist = d;
					bestWorld = w;
				}
			}
			dirPoint = bestWorld;
		}
		else
		{
			dirPoint = building.ModelToWorld(m_BottomDir);
		}
		return dirPoint;
	}
}

class dmBotLadderCache
{
	static ref dmBotLadderCache s_Instance;
	ref map<string, ref array<ref dmBotLadder>> m_Cache;

	static dmBotLadderCache GetInstance()
	{
		if (!s_Instance)
			s_Instance = new dmBotLadderCache();
		return s_Instance;
	}

	void dmBotLadderCache()
	{
		m_Cache = new map<string, ref array<ref dmBotLadder>>;
	}

	//! Список лестниц здания (парсит memory LOD, кэширует по типу). Пустой массив,
	//! если лестниц нет; null, если building == null.
	ref array<ref dmBotLadder> GetLadders(Building building)
	{
		if (!building)
			return null;

		string type = building.GetType();
		array<ref dmBotLadder> cached;
		if (m_Cache.Find(type, cached))
			return cached;

		ref array<ref dmBotLadder> ladders = ParseLadders(building);
		m_Cache.Set(type, ladders);

		#ifdef DM_BOT_DEBUG_FSM
		dmBotLog.Debug("", "[Ladder] building=" + type + " ladders=" + ladders.Count());
		#endif

		return ladders;
	}

	//! Парсит memory LOD здания в список лестниц. Никогда не возвращает null.
	private ref array<ref dmBotLadder> ParseLadders(Building building)
	{
		ref array<ref dmBotLadder> ladders = new array<ref dmBotLadder>;

		LOD memory = building.GetLODByName(LOD.NAME_MEMORY);
		if (!memory)
			return ladders;

		array<Selection> selections = new array<Selection>;
		if (!memory.GetSelections(selections))
			return ladders;

		string ladderType = GetLadderType(building);

		//! Уникальные лестницы по индексу (из имени "ladderN...").
		ref map<int, ref dmBotLadder> byIndex = new map<int, ref dmBotLadder>;

		int i;
		for (i = 0; i < selections.Count(); i++)
		{
			Selection selection = selections[i];
			string name = selection.GetName();
			if (name.IndexOf("ladder") != 0)
				continue;

			string ladderName;
			int ladderIndex = ParseLadderName(name, ladderName);
			if (ladderIndex < 0)
				continue;

			dmBotLadder ladder;
			if (!byIndex.Find(ladderIndex, ladder))
			{
				ladder = new dmBotLadder;
				ladder.m_Index = ladderIndex;
				ladder.m_Type = ladderType;
				byIndex.Set(ladderIndex, ladder);
				ladders.Insert(ladder);
			}

			if (name == ladderName + "_con")
				CollectConVertices(selection, memory, ladder);
			if (name == ladderName + "_con_dir")
				CollectConDirVertices(selection, memory, ladder);
		}

		#ifdef DM_BOT_DEBUG_FSM
		for (i = 0; i < ladders.Count(); i++)
		{
			dmBotLadder lad = ladders[i];
			dmBotLog.Debug("", "[Ladder] index=" + lad.m_Index + " type=" + lad.m_Type);
			dmBotLog.Debug("", "[Ladder] bottom=" + lad.m_Bottom + " top=" + lad.m_Top);
			string ys = "";
			int vi;
			if ( lad.m_Vertices )
			{
				for (vi = 0; vi < lad.m_Vertices.Count(); vi++)
					ys = ys + lad.m_Vertices[vi][1] + ",";
				dmBotLog.Debug("", "[Ladder] index=" + lad.m_Index + " vertices=" + lad.m_Vertices.Count() + " y=" + ys);
			}
		}
		#endif

		return ladders;
	}

	//! Тип лестницы из geometry LOD (property "laddertype"), по умолчанию "metal".
	private string GetLadderType(Building building)
	{
		string type = "metal";
		LOD geometry = building.GetLODByName(LOD.NAME_GEOMETRY);
		if (!geometry)
			return type;

		int i;
		for (i = 0; i < geometry.GetPropertyCount(); i++)
		{
			if (geometry.GetPropertyName(i) == "laddertype")
			{
				type = geometry.GetPropertyValue(i);
				break;
			}
		}
		return type;
	}

	//! Из имени selection'а "ladderN[_suffix]" выделяет базовое имя "ladderN" (в
	//! ladderName) и номер N (индекс лестницы). Возвращает -1, если имя не похоже
	//! на лестницу (например "ladder" без номера).
	private int ParseLadderName(string name, out string ladderName)
	{
		int underscoreIndex = name.IndexOf("_");
		if (underscoreIndex > 6)
		{
			ladderName = name.Substring(0, underscoreIndex);
			return name.Substring(6, underscoreIndex - 6).ToInt();
		}
		if (name.Length() > 6)
		{
			ladderName = name;
			return name.Substring(6, name.Length() - 6).ToInt();
		}
		return -1;
	}

	//! Собирает ВСЕ вершины selection'а "_con" в точки входа лестницы. Вершины с
	//! GetLODVertexIndex == -1 пропускаем (ванильная готча: GetVertexPosition при
	//! -1 вернёт vector.Zero и сорвёт min/max). Массив сортируется по Y (младшая —
	//! низ), min/max выставляются в m_Bottom/m_Top. Позиции в model-space.
	private void CollectConVertices(Selection selection, LOD memory, dmBotLadder ladder)
	{
		ladder.m_Vertices = new array<vector>;
		int j;
		for (j = 0; j < selection.GetVertexCount(); j++)
		{
			if (selection.GetLODVertexIndex(j) < 0)
				continue;
			vector vertex = selection.GetVertexPosition(memory, j);
			ladder.m_Vertices.Insert(vertex);
		}
		if (ladder.m_Vertices.Count() == 0)
			return;
		SortByY(ladder.m_Vertices);
		ladder.m_Bottom = ladder.m_Vertices[0];
		ladder.m_Top = ladder.m_Vertices[ladder.m_Vertices.Count() - 1];
	}

	//! Собирает ВСЕ вершины selection'а "_con_dir" в направления входа/выхода.
	//! Фильтр GetLODVertexIndex и сортировка — как в CollectConVertices.
	private void CollectConDirVertices(Selection selection, LOD memory, dmBotLadder ladder)
	{
		ladder.m_VertDirs = new array<vector>;
		int j;
		for (j = 0; j < selection.GetVertexCount(); j++)
		{
			if (selection.GetLODVertexIndex(j) < 0)
				continue;
			vector vertex = selection.GetVertexPosition(memory, j);
			ladder.m_VertDirs.Insert(vertex);
		}
		if (ladder.m_VertDirs.Count() == 0)
			return;
		SortByY(ladder.m_VertDirs);
		ladder.m_BottomDir = ladder.m_VertDirs[0];
		ladder.m_TopDir = ladder.m_VertDirs[ladder.m_VertDirs.Count() - 1];
	}

	//! Сортирует вершины по возрастанию Y (insertion sort; число вершин мало).
	private void SortByY(array<vector> verts)
	{
		int n = verts.Count();
		int a;
		int b;
		vector key;
		for (a = 1; a < n; a++)
		{
			key = verts[a];
			b = a - 1;
			while (b >= 0 && verts[b][1] > key[1])
			{
				verts[b + 1] = verts[b];
				b = b - 1;
			}
			verts[b + 1] = key;
		}
	}
}
