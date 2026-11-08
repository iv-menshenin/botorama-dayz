//! modded CarScript (core) — детект урона от ДТП для ботов-членов экипажа.
//! DamageCrew бьёт экипаж мимо EEHitBy (через AddHealth), поэтому ловим здесь и
//! передаём флаг пешке-боту. НЕ оверрайдим системный AddHealth.
modded class CarScript
{
	override void DamageCrew(float dmg)
	{
		super.DamageCrew(dmg);

		for (int c = 0; c < CrewSize(); ++c)
		{
			Human crew = CrewMember(c);
			if (!crew)
				continue;
			dmAISurvivorBase pawn = dmAISurvivorBase.Cast(crew);
			if (pawn && pawn.IsAlive())
				pawn.OnCrashHit(dmg);
		}
	}
}
