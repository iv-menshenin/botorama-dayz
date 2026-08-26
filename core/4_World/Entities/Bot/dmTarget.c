//! dmTarget — the bot's goal target (memory, survives FSM transitions).
//!
//! A target is what the bot wants to acquire or destroy. The bot may not know the
//! target's live position (out of visibility) — it then searches by m_LastPosition
//! or by m_ClassEntity (e.g. "BandageDressing" when m_Entity is null).

enum dmTargetType
{
	ACQUIRE,   // завладеть (предмет)
	DESTROY    // уничтожить (существо/игрок)
};

class dmTarget
{
	dmTargetType m_Type = dmTargetType.DESTROY;
	EntityAI m_Entity;          // конкретная сущность (может быть null)
	string m_ClassEntity;       // класс для поиска, если m_Entity == null
	float m_Priority = 0.0;
	vector m_LastPosition;      // последняя известная позиция (поиск)
}
