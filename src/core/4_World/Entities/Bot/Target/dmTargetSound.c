//! dmTargetSound — abstract base всех «звуковых» целей: бот слышит звук, но ещё не
//! знает, что его производит. Здесь живут поля внимания (m_Attraction/
//! m_AttentionCooldownUntil) — интерес разузнать свежую цель (0..1), затухает; не-звуковые
//! цели имеют нулевой аттракшен через виртуальные акцессоры базового dmTarget.
class dmTargetSound : dmTarget
{
	float m_Attraction = 0.0;
	float m_AttentionCooldownUntil = 0.0;

	override float GetAttraction() { return m_Attraction; }
	override void SetAttraction(float v) { m_Attraction = v; }
	override float GetAttentionCooldownUntil() { return m_AttentionCooldownUntil; }
	override void SetAttentionCooldownUntil(float v) { m_AttentionCooldownUntil = v; }
}
