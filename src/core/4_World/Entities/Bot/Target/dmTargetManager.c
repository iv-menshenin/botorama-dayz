//! dmTargetManager — коллекция целей бота. Простая механика: массив, поиск по EntityAI,
//! вставка/удаление. Без хуков/триггеров — они живут на мозге (AddTarget/RemoveTarget).
class dmTargetManager
{
	private ref array<ref dmTarget> m_Targets;

	void dmTargetManager()
	{
		m_Targets = new array<ref dmTarget>();
	}

	//! Поиск цели по сущности (null, если нет).
	dmTarget Find(EntityAI entity)
	{
		#ifdef DM_BOT_PROFILE
		dmBotSpan _span = dmBotProfiler.Start("Target.Find");
		#endif

		int i;
		for (i = 0; i < m_Targets.Count(); i++)
		{
			dmTarget t = m_Targets[i];
			if (t.m_Entity == entity)
				return t;
		}
		return null;
	}

	//! Добавить цель (без дедупа — дедуп/замена через ReplaceTarget на мозге).
	void Add(dmTarget target)
	{
		m_Targets.Insert(target);
	}

	//! Удалить цель.
	void Remove(dmTarget target)
	{
		m_Targets.RemoveItem(target);
	}

	//! Очистить все цели (сохраняет массив, опустошает содержимое).
	void Clear()
	{
		m_Targets.Clear();
	}

	//! Итерация по целям (для циклов-потребителей).
	ref array<ref dmTarget> GetTargets()
	{
		return m_Targets;
	}
}
