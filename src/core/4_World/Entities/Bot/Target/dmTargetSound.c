//! dmTargetSound — abstract base всех «звуковых» целей: бот слышит звук, но ещё не
//! знает, что его производит. Здесь живут поля внимания (m_Attraction/
//! m_AttentionCooldownUntil) — интерес разузнать свежую цель (0..1), затухает; не-звуковые
//! цели имеют нулевой аттракшен через виртуальные акцессоры базового dmTarget.
class dmTargetSound : dmTarget
{
	float m_Attraction = 0.0;
	float m_AttentionCooldownUntil = 0.0;
	float m_LastNoiseTime = 0.0;   // GetGame().GetTickTime() последнего шума

	override float GetAttraction() { return m_Attraction; }
	override void SetAttraction(float v) { m_Attraction = v; }
	override float GetAttentionCooldownUntil() { return m_AttentionCooldownUntil; }
	override void SetAttentionCooldownUntil(float v) { m_AttentionCooldownUntil = v; }

	override float InfoUpdatedTimePassed()
	{
		if (m_LastNoiseTime <= 0.0)
			return -1.0;
		return GetGame().GetTickTime() - m_LastNoiseTime;
	}
	override void SetLastNoiseTime(float v) { m_LastNoiseTime = v; }
}
