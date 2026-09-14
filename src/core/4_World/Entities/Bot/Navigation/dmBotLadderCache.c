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
//! точки входа (низ/верх в model-space, отсортированы по высоте Y).
class dmBotLadder
{
	int m_Index;
	string m_Type;
	vector m_Bottom;
	vector m_Top;
	vector m_BottomDir;   // направление входа/выхода у нижней точки
	vector m_TopDir;      // направление входа/выхода у верхней точки
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
		dmBotLog.Debug("[Ladder] building=" + type + " ladders=" + ladders.Count());
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
			dmBotLog.Debug("[Ladder] index=" + lad.m_Index + " type=" + lad.m_Type);
			dmBotLog.Debug("[Ladder] bottom=" + lad.m_Bottom + " top=" + lad.m_Top);
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

	//! Собирает вершины selection'а "_con" в точки входа лестницы, сортируя по Y
	//! (младшая Y — низ, старшая — верх). Позиции остаются в model-space.
	private void CollectConVertices(Selection selection, LOD memory, dmBotLadder ladder)
	{
		int count = 0;
		vector bottom;
		vector top;
		int j;
		for (j = 0; j < selection.GetVertexCount(); j++)
		{
			vector vertex = selection.GetVertexPosition(memory, j);
			if (count == 0)
			{
				bottom = vertex;
				top = vertex;
				count = 1;
				continue;
			}
			if (vertex[1] < bottom[1])
				bottom = vertex;
			if (vertex[1] > top[1])
				top = vertex;
		}
		if (count == 0)
			return;
		ladder.m_Bottom = bottom;
		ladder.m_Top = top;
	}

	//! Собирает вершины selection'а "_con_dir" в направления входа/выхода лестницы,
	//! сортируя по Y (младшая Y — низ, старшая — верх). Позиции остаются в model-space.
	private void CollectConDirVertices(Selection selection, LOD memory, dmBotLadder ladder)
	{
		int count = 0;
		vector bottom;
		vector top;
		int j;
		for (j = 0; j < selection.GetVertexCount(); j++)
		{
			vector vertex = selection.GetVertexPosition(memory, j);
			if (count == 0)
			{
				bottom = vertex;
				top = vertex;
				count = 1;
				continue;
			}
			if (vertex[1] < bottom[1])
				bottom = vertex;
			if (vertex[1] > top[1])
				top = vertex;
		}
		if (count == 0)
			return;
		ladder.m_BottomDir = bottom;
		ladder.m_TopDir = top;
	}
}
