//! modded Restrain — позволить связать AI-бота и записать, кто его связал.
//!
//! Ванильный CanBeRestrained() требует GetActionManager() != null (playerbase.c:1869),
//! которого у INSTANCETYPE_AI_SERVER-бота нет — поэтому человек не может связать бота.
//! Здесь для ИИ пропускаем только этот гейт, сохраняя физические/состояние-проверки.
//!
//! Restrainer'а на цели нет (идентичность видна только в action_data.m_Player), поэтому
//! свой хук в OnFinishProgressServer записывает актора в мозг цели (см. docs/research/body.md §4.1).

modded class PlayerBase
{
	//! Разрешить связывать AI-бота: ваниль требует GetActionManager()!=null,
	//! которого у AI_SERVER нет. Для ИИ пропускаем только этот гейт.
	override bool CanBeRestrained()
	{
		if (GetInstanceType() != DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER)
			return super.CanBeRestrained();

		if (IsInVehicle() || IsRaised() || IsSwimming() || IsClimbing() || IsClimbingLadder() || IsRestrained() || !GetWeaponManager() || GetWeaponManager().IsRunning() || IsMapOpen())
			return false;
		if (GetThrowing() && GetThrowing().IsThrowingModeEnabled())
			return false;
		return true;
	}
}

modded class ActionRestrainTarget
{
	//! Записать restrainer'а в мозг цели. Ваниль ставит SetRestrained(true) только в
	//! лямбде RestrainTargetPlayerLambda.OnSuccess; актор доступен здесь — сохраняем его,
	//! чтобы мозг знал «кто связал» при детекте перехода IsRestrained().
	override void OnFinishProgressServer(ActionData action_data)
	{
		super.OnFinishProgressServer(action_data);

		PlayerBase target = PlayerBase.Cast(action_data.m_Target.GetObject());
		if (target)
		{
			dmAISurvivor brain = dmAISurvivor.Find(target);
			if (brain)
				brain.SetRestrainedBy(action_data.m_Player);
		}
	}
}
