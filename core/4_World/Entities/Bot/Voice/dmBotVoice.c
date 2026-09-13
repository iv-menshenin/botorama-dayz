//! dmBotVoice — категоризатор голосовых реплик: категории, количество реплик в
//! категории и маппинг lineId -> имя CfgSoundSets-сета. Звуки описаны в config.cpp,
//! файлы лежат в voices/<category>/<keyword>.ogg (плейсхолдеры до студии).
enum dmVoiceCategory
{
	GREETING,        // Приветствия
	WAKE,            // очнулся после отключки
	PASSENGER_CRASH, // пассажир, водитель врезается
	IDLE,            // покой/скука
	PATROL,          // патруль
	AIMED_AT,        // целятся, но не стреляют
	HEARD_SHOT,      // услышал выстрел
	GOT_SHOT,        // получил пулю
	COMBAT,          // враги, бой
	ESCORT,          // сопровождение
	DM_VOICE_CATEGORY_COUNT
};

class dmBotVoice
{
	//! Имя папки/категории (для логов).
	static string GetCategoryName(dmVoiceCategory cat)
	{
		switch (cat)
		{
		case GREETING:        return "greeting";
		case WAKE:            return "wake";
		case PASSENGER_CRASH: return "passenger";
		case IDLE:            return "idle";
		case PATROL:          return "patrol";
		case AIMED_AT:        return "aimed_at";
		case HEARD_SHOT:      return "heard_shot";
		case GOT_SHOT:        return "got_shot";
		case COMBAT:          return "combat";
		case ESCORT:          return "escort";
		}
		return "";
	}

	//! Число реплик в категории.
	static int GetCategoryCount(dmVoiceCategory cat)
	{
		switch (cat)
		{
		case GREETING:        return 10;
		case WAKE:            return 5;
		case PASSENGER_CRASH: return 5;
		case IDLE:            return 4;
		case PATROL:          return 5;
		case AIMED_AT:        return 5;
		case HEARD_SHOT:      return 5;
		case GOT_SHOT:        return 3;
		case COMBAT:          return 5;
		case ESCORT:          return 3;
		}
		return 0;
	}

	//! lineId = category*100 + index (index 1..count). Пустая строка = нет реплики.
	static string GetSoundSetName(int lineId)
	{
		switch (lineId)
		{
		// greeting
		case 1:  return "dmBotVoice_dandy_SoundSet";
		case 2:  return "dmBotVoice_what_a_fruit_SoundSet";
		case 3:  return "dmBotVoice_sour_face_SoundSet";
		case 4:  return "dmBotVoice_like_a_bum_SoundSet";
		case 5:  return "dmBotVoice_life_worn_SoundSet";
		case 6:  return "dmBotVoice_familiar_face_SoundSet";
		case 7:  return "dmBotVoice_hey_there_SoundSet";
		case 8:  return "dmBotVoice_my_respects_SoundSet";
		case 9:  return "dmBotVoice_glad_to_see_SoundSet";
		case 10: return "dmBotVoice_what_people_SoundSet";
		// wake
		case 101: return "dmBotVoice_why_not_dead_SoundSet";
		case 102: return "dmBotVoice_it_hurts_SoundSet";
		case 103: return "dmBotVoice_screw_it_all_SoundSet";
		case 104: return "dmBotVoice_what_happened_SoundSet";
		case 105: return "dmBotVoice_screw_everyone_SoundSet";
		// passenger
		case 201: return "dmBotVoice_best_driver_SoundSet";
		case 202: return "dmBotVoice_not_firewood_SoundSet";
		case 203: return "dmBotVoice_should_take_bus_SoundSet";
		case 204: return "dmBotVoice_that_hurt_SoundSet";
		case 205: return "dmBotVoice_still_alive_SoundSet";
		// idle
		case 301: return "dmBotVoice_so_sleepy_SoundSet";
		case 302: return "dmBotVoice_no_money_SoundSet";
		case 303: return "dmBotVoice_good_days_work_SoundSet";
		case 304: return "dmBotVoice_shoot_someone_SoundSet";
		// patrol
		case 401: return "dmBotVoice_greedy_bastard_SoundSet";
		case 402: return "dmBotVoice_you_are_the_man_SoundSet";
		case 403: return "dmBotVoice_west_is_fine_SoundSet";
		case 404: return "dmBotVoice_shoot_a_boar_SoundSet";
		case 405: return "dmBotVoice_pissed_himself_SoundSet";
		// aimed_at
		case 501: return "dmBotVoice_put_gun_down_SoundSet";
		case 502: return "dmBotVoice_where_aiming_SoundSet";
		case 503: return "dmBotVoice_you_joking_SoundSet";
		case 504: return "dmBotVoice_careful_SoundSet";
		case 505: return "dmBotVoice_never_point_gun_SoundSet";
		// heard_shot
		case 601: return "dmBotVoice_shooting_SoundSet";
		case 602: return "dmBotVoice_someone_fun_SoundSet";
		case 603: return "dmBotVoice_they_going_hard_SoundSet";
		case 604: return "dmBotVoice_action_without_me_SoundSet";
		case 605: return "dmBotVoice_no_rest_SoundSet";
		// got_shot
		case 701: return "dmBotVoice_what_a_hit_SoundSet";
		case 702: return "dmBotVoice_on_me_on_me_SoundSet";
		case 703: return "dmBotVoice_im_shot_SoundSet";
		// combat
		case 801: return "dmBotVoice_here_we_go_SoundSet";
		case 802: return "dmBotVoice_fun_begins_SoundSet";
		case 803: return "dmBotVoice_contact_SoundSet";
		case 804: return "dmBotVoice_who_else_SoundSet";
		case 805: return "dmBotVoice_future_corpses_SoundSet";
		// escort
		case 901: return "dmBotVoice_lets_go_ready_SoundSet";
		case 902: return "dmBotVoice_anywhere_SoundSet";
		case 903: return "dmBotVoice_no_time_to_smoke_SoundSet";
		}
		return "";
	}

	//! Длительность рта (сек) — из конфига CfgSoundSets <set> duration. Fallback 1.0.
	static float GetTalkDuration(int lineId)
	{
		string name = GetSoundSetName(lineId);
		if (name == "")
			return 1.0;

		string path = "CfgSoundSets " + name + " duration";
		if (GetGame().ConfigIsExisting(path))
			return GetGame().ConfigGetFloat(path);

		return 1.0;
	}
}
