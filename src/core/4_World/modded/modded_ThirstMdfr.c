//! modded ThirstMdfr — снизить базовый расход воды (жажда) у AI-бота в 2 раза.
//!
//! Полная копия ванильного OnTick (thirst.c); единственное отличие — множитель
//! DM_BOT_CONSUMPTION_COEF на metabolic_speed, только для INSTANCETYPE_AI_SERVER.
//! Порог Health-урона (LOW_WATER_THRESHOLD) и звуковое состояние THIRSTY не меняются.

modded class ThirstMdfr
{
	override void OnTick(PlayerBase player, float deltaT)
	{
		if (player.GetInstanceType() != DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
		{
			super.OnTick(player, deltaT);
			return;
		}
		
		player.GetMovementState(m_MovementState);
		float water = player.GetStatWater().Get();
		float metabolic_speed = MiscGameplayFunctions.GetWaterMetabolicSpeed(m_MovementState.m_iMovement);

		if (player.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
			metabolic_speed *= DM_BOT_CONSUMPTION_COEF;

		float modifier = water/PlayerConstants.SL_WATER_MAX + PlayerConstants.CONSUMPTION_MULTIPLIER_BASE;
		metabolic_speed *= modifier;

		player.GetStatWater().Add( (-metabolic_speed * deltaT) );

		if ( water <= PlayerConstants.LOW_WATER_THRESHOLD )
		{
			player.SetMixedSoundState( eMixedSoundStates.THIRSTY );
			if ((player.GetStomach().GetDigestingType() & PlayerStomach.DIGESTING_WATER) == 0)
				player.AddHealth("GlobalHealth", "Health", -PlayerConstants.LOW_WATER_DAMAGE_PER_SEC * deltaT );
		}
		else
		{
			player.UnsetMixedSoundState( eMixedSoundStates.THIRSTY );
		}
	}
}
