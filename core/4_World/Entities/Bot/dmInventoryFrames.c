enum dmInventoryDoing
{
	PLACEONGROUND = 0,
	ATTACHTOSLOT,
	TAKEINTOCARGO,
	PUTINTOHANDS
}

class dmInventoryFrame
{
	bool m_Done = false;          // выполнялся ли
	bool m_Success = false;       // результат последнего Execute
	dmInventoryDoing m_ToDo;      // глагол
	ItemBase m_Item;              // предмет операции
	int m_SlotId = -1;            // слот для ATTACHTOSLOT
	EntityAI m_To;                // контейнер для TAKEINTOCARGO
	InventoryLocation m_Loc;      // InventoryLocation для действия

	ref dmInventoryFrame m_OnSuccess;
	ref dmInventoryFrame m_OnFail;

	//! Выполнить глагол через примитивы dmLoot. Ставит m_Done=true, записывает m_Success.
	bool Execute(dmAISurvivorBase pawn)
	{
		m_Done = true;
		m_Success = false;
		if (!m_Item)
			return false;
		switch (m_ToDo)
		{
		case dmInventoryDoing.PLACEONGROUND:
			m_Success = pawn.DropItem(m_Item);
			break;
		case dmInventoryDoing.ATTACHTOSLOT:
			m_Success = dmLoot.TakeToAttachmentSlot(pawn, m_Item, m_SlotId);
			break;
		case dmInventoryDoing.TAKEINTOCARGO:
			if ( m_Loc )
				m_Success = dmLoot.TakeIntoDestination(pawn, m_Item, m_Loc);
			else
				m_Success = dmLoot.TakeIntoCargo(pawn, m_Item, m_To);
			break;
		case dmInventoryDoing.PUTINTOHANDS:
			m_Success = dmLoot.TakeToHands(pawn, m_Item);
			break;
		}
		return m_Success;
	}

	//! Следующий фрейм по результату выполнения.
	dmInventoryFrame Next(bool success)
	{
		if (success)
			return m_OnSuccess;
		return m_OnFail;
	}

	//! «Всё успех»: каждый лист выполненной ветки вернул успех (листовой фрейм — m_Success).
	//! НЕ то же, что «доигралось» (см. IsAllFinished).
	bool IsAllDone()
	{
		if (!m_Done)
			return false;
		if (!m_OnSuccess && !m_OnFail)
			return m_Success;
		if (m_OnSuccess && m_OnSuccess.IsAllDone())
			return true;
		if (m_OnFail && m_OnFail.IsAllDone())
			return true;
		return false;
	}

	//! «Доигралось»: выполненная ветка доигралась, НЕ важно успех или нет (листовой фрейм —
	//! m_Done). НЕ то же, что «всё успех» (см. IsAllDone).
	bool IsAllFinished()
	{
		if (!m_Done)
			return false;
		if (!m_OnSuccess && !m_OnFail)
			return m_Done;
		if (m_OnSuccess && m_OnSuccess.IsAllFinished())
			return true;
		if (m_OnFail && m_OnFail.IsAllFinished())
			return true;
		return false;
	}

	//! Фабрики построения цепочек.
	static dmInventoryFrame SuccessFail(dmInventoryFrame s, dmInventoryFrame f)
	{
		dmInventoryFrame frame = new dmInventoryFrame();
		frame.m_OnSuccess = s;
		frame.m_OnFail = f;
		return frame;
	}

	static dmInventoryFrame SuccessOnly(dmInventoryFrame s)
	{
		dmInventoryFrame frame = new dmInventoryFrame();
		frame.m_OnSuccess = s;
		return frame;
	}

	static dmInventoryFrame FailOnly(dmInventoryFrame f)
	{
		dmInventoryFrame frame = new dmInventoryFrame();
		frame.m_OnFail = f;
		return frame;
	}

	//! Безусловный следующий (и при успехе, и при фейле).
	static dmInventoryFrame Then(dmInventoryFrame a)
	{
		dmInventoryFrame frame = new dmInventoryFrame();
		frame.m_OnSuccess = a;
		frame.m_OnFail = a;
		return frame;
	}

	//! Фабрика листового action-фрейма (verb + item + slot + target).
	static dmInventoryFrame Make(dmInventoryDoing verb, ItemBase item, int slotId = -1, EntityAI to = null)
	{
		dmInventoryFrame frame = new dmInventoryFrame();
		frame.m_ToDo = verb;
		frame.m_Item = item;
		frame.m_SlotId = slotId;
		frame.m_To = to;
		return frame;
	}
}

class dmInventoryFrames
{
	dmAISurvivorBase m_Pawn;
	ref array<ref dmInventoryFrame> m_Queue;

	void dmInventoryFrames(dmAISurvivorBase pawn)
	{
		m_Pawn = pawn;
		m_Queue = new array<ref dmInventoryFrame>();
	}

	//! Поставить цепочку (её корень) в очередь.
	void Enqueue(dmInventoryFrame head)
	{
		if (head)
			m_Queue.Insert(head);
	}

	//! Выполнить одну голову за тик: head.Execute(pawn) → head.Next(success);
	//! если next != null — подменить голову, иначе — удалить (цепочка закончена).
	void Tick()
	{
		if (m_Queue.Count() == 0)
			return;
		dmInventoryFrame head = m_Queue[0];
		bool success = head.Execute(m_Pawn);
		dmInventoryFrame next = head.Next(success);
		if (next)
			m_Queue[0] = next;
		else
			m_Queue.Remove(0);
	}

	//! Отменить все запланированные цепочки.
	void Clear()
	{
		m_Queue.Clear();
	}

	bool IsEmpty()
	{
		return m_Queue.Count() == 0;
	}
}
