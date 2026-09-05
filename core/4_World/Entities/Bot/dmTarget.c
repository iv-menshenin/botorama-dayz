//! dmTarget — the bot's goal target: memory + evaluation (survives FSM transitions).
//!
//! A target is what the bot wants to acquire or destroy. The bot may not know the
//! target's live position (out of visibility) — it then searches by m_LastPosition
//! or by m_ClassEntity (e.g. "BandageDressing" when m_Entity is null).
//!
//! Memory (T4): a target that hides behind a wall/brush stays in the list with its
//! last known position until it times out — m_LastPosition / m_HasLOS / m_LastContact.
//! Evaluation: m_Threat / m_Attractiveness / m_Friendly drive combat/loot choices.

enum dmTargetType
{
	ACQUIRE,   // завладеть (предмет)
	DESTROY    // уничтожить (существо/игрок)
};

class dmTarget
{
	dmTargetType m_Type = dmTargetType.DESTROY;
	EntityAI m_Entity;              // конкретная сущность (может быть null)
	string m_ClassEntity;           // класс для поиска/лута, если m_Entity == null (ACQUIRE)

	vector m_LastPosition;          // последняя известная позиция
	float m_LastDistance;           // последнее известное расстояние до цели
	bool m_HasLOS = false;          // видна в последнем скане
	float m_LastContact = 0.0;      // GetGame().GetTickTime() последнего контакта
	float m_LastDamage = 0.0;       // GetGame().GetTickTime() последнего полученного урона

	//! Абсолютное время (GetGame().GetTickTime()) следующей LOS-проверки этой цели.
	//! 0.0 = проверка нужна немедленно (сразу после открытия).
	float m_NextLOSUpdate = 0.0;

	float m_Threat = 0.0;           // опасность (0..1)
	float m_Attractiveness = 0.0;   // привлекательность (0..1)
	bool m_Friendly = false;        // не атаковать
}
