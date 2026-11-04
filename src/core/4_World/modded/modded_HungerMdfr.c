//! modded HungerMdfr — снизить базовый расход энергии (голод) у AI-бота в 2 раза.
//!
//! Полная копия ванильного OnTick (hunger.c); единственное отличие — множитель
//! DM_BOT_CONSUMPTION_COEF на metabolic_speed, только для INSTANCETYPE_AI_SERVER.
//! Порог Health-урона (LOW_ENERGY_THRESHOLD) и звуковое состояние HUNGRY не меняются.

modded class HungerMdfr
{
	override void OnTick(PlayerBase player, float deltaT)
	{
		if (player.GetInstanceType() != DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
		{
			super.OnTick(player, deltaT);
			return;
		}

		player.GetMovementState(m_MovementState);
		float energy = player.GetStatEnergy().Get();
		float metabolic_speed = MiscGameplayFunctions.GetEnergyMetabolicSpeed(m_MovementState.m_iMovement);

		if (player.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
			metabolic_speed *= DM_BOT_CONSUMPTION_COEF;

		float modifier = energy/PlayerConstants.SL_ENERGY_MAX + PlayerConstants.CONSUMPTION_MULTIPLIER_BASE;
		metabolic_speed *= modifier;

		player.GetStatEnergy().Add( -metabolic_speed * deltaT );
		if ( energy <= PlayerConstants.LOW_ENERGY_THRESHOLD )
		{
			player.SetMixedSoundState( eMixedSoundStates.HUNGRY );
			if ((player.GetStomach().GetDigestingType() & PlayerStomach.DIGESTING_ENERGY) == 0)
				player.AddHealth("GlobalHealth", "Health", -PlayerConstants.LOW_ENERGY_DAMAGE_PER_SEC * deltaT );
		}
		else
		{
			player.UnsetMixedSoundState( eMixedSoundStates.HUNGRY );
		}
	}
}
