enum dmInventoryDoing
{
	PLACEONGROUND = 0,
	ATTACHTOSLOT,
	TAKEINTOCARGO,
	PUTINTOHANDS
}

class dmInventoryFrame
{
	bool m_Done = false;          // выполнялся ли (не важен результат)
	dmInventoryDoing m_ToDo;      // глагол
	ItemBase m_Item;              // предмет операции
	int m_SlotId = -1;            // слот для ATTACHTOSLOT
	EntityAI m_To;                // контейнер для TAKEINTOCARGO

	ref dmInventoryFrame m_OnSuccess;
	ref dmInventoryFrame m_OnFail;

	//! Выполнить глагол через примитивы пешки. Ставит m_Done=true и возвращает успех.
	bool Execute(dmAISurvivorBase pawn)
	{
		m_Done = true;
		if (!m_Item)
			return false;
		switch (m_ToDo)
		{
		case dmInventoryDoing.PLACEONGROUND:
			return pawn.DropItem(m_Item);
		case dmInventoryDoing.ATTACHTOSLOT:
			return pawn.TakeToAttachmentSlot(m_Item, m_SlotId);
		case dmInventoryDoing.TAKEINTOCARGO:
			return pawn.TakeIntoCargo(m_Item, m_To);
		case dmInventoryDoing.PUTINTOHANDS:
			return pawn.TakeToHands(m_Item);
		}
		return false;
	}

	//! Следующий фрейм по результату выполнения.
	dmInventoryFrame Next(bool success)
	{
		if (success)
			return m_OnSuccess;
		return m_OnFail;
	}

	//! Готово ли ВСЁ дерево (выполненная ветка рекурсивно). ВАЖНО: листовой фрейм
	//! (выполнен, веток нет) — готов.
	bool IsAllDone()
	{
		if (!m_Done)
			return false;
		if (!m_OnSuccess && !m_OnFail)
			return true;
		if (m_OnSuccess && m_OnSuccess.IsAllDone())
			return true;
		if (m_OnFail && m_OnFail.IsAllDone())
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
