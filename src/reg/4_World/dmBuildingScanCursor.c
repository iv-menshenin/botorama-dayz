//! dmBuildingScanCursor — курсор чанкованного O(POI)-скана (round-robin слайсинг).
//!
//! Один курсор на скан: размазывает обход большого списка зданий по тикам
//! бюджетом DM_EXPLORE_SCAN_BUDGET записей за тик, устраняя O(POI)-спайк при
//! выборе здания на крупных POI. Два потребителя:
//!  - dmExplorer.ContinueNearestUnvisitedScan — nearest-поиск (все три поля);
//!  - dmLiveBuildingRegistry.ContinueBuildingsOfType — сбор зданий типа (только m_Row).

class dmBuildingScanCursor
{
	int m_Row;         // следующий непросмотренный индекс
	Building m_Best;   // лучший на просмотренной части (null = нет)
	float m_BestSq;    // его дистанция² (для nearest-поиска)
}
