//! dmNoiseSystem — shared noise signal for bot hearing.
//!
//! The native NoiseSystem is a sink for native zombie/animal sensing and never
//! triggers script callbacks, so the mod publishes its own noise signal here (like
//! Expansion's eAINoiseSystem). Noise generators (gunshots, footsteps, bullet
//! impacts, zombie cries) will call AddNoise in Phase 2; receivers subscribe to
//! SI_OnNoiseAdded (see dmHearing).

class dmNoiseSystem
{
	static ref ScriptInvoker SI_OnNoiseAdded = new ScriptInvoker;

	//! Сгенерировать шум (source может быть null — на приёме пока игнорируем, TODO).
	static void AddNoise(EntityAI source, vector position, float strength)
	{
		SI_OnNoiseAdded.Invoke(source, position, strength);
	}
};
