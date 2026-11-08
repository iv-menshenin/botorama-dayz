//! dmBotVoice — категоризатор голосовых реплик: категории, число реплик, голос из
//! конфига модели (dmBotVoice) и lineId -> имя CfgSoundSets-сета. Звуки описаны в
//! config.cpp, файлы лежат в voices/<voice>/<category>/<keyword>.ogg (плейсхолдеры).
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
	//! Ключи реплик по категориям: s_Keys[cat] — имена файлов в порядке index 1..N.
	//! cat совпадает с dmVoiceCategory (0..9); lineId = cat*100 + index.
	static ref array<ref TStringArray> s_Keys = {
		{ "dandy", "what_a_fruit", "sour_face", "like_a_bum", "life_worn", "familiar_face", "hey_there", "my_respects", "glad_to_see", "what_people" },
		{ "why_not_dead", "it_hurts", "screw_it_all", "what_happened", "screw_everyone" },
		{ "best_driver", "not_firewood", "should_take_bus", "that_hurt", "still_alive" },
		{ "so_sleepy", "no_money", "good_days_work", "shoot_someone" },
		{ "greedy_bastard", "you_are_the_man", "west_is_fine", "shoot_a_boar", "pissed_himself" },
		{ "put_gun_down", "where_aiming", "you_joking", "careful", "never_point_gun" },
		{ "shooting", "someone_fun", "they_going_hard", "action_without_me", "no_rest" },
		{ "what_a_hit", "on_me_on_me", "im_shot", "bandaging", "bleeding" },
		{ "here_we_go", "fun_begins", "contact", "who_else", "future_corpses" },
		{ "lets_go_ready", "anywhere", "no_time_to_smoke" }
	};

	//! Имя папки/категории (для логов).
	static string GetCategoryName(dmVoiceCategory cat)
	{
		switch (cat)
		{
		case dmVoiceCategory.GREETING:        return "greeting";
		case dmVoiceCategory.WAKE:            return "wake";
		case dmVoiceCategory.PASSENGER_CRASH: return "passenger";
		case dmVoiceCategory.IDLE:            return "idle";
		case dmVoiceCategory.PATROL:          return "patrol";
		case dmVoiceCategory.AIMED_AT:        return "aimed_at";
		case dmVoiceCategory.HEARD_SHOT:      return "heard_shot";
		case dmVoiceCategory.GOT_SHOT:        return "got_shot";
		case dmVoiceCategory.COMBAT:          return "combat";
		case dmVoiceCategory.ESCORT:          return "escort";
		}
		return "";
	}

	//! Число реплик в категории — из таблицы ключей.
	static int GetCategoryCount(dmVoiceCategory cat)
	{
		if (cat < 0 || cat >= s_Keys.Count())
			return 0;
		TStringArray keys = s_Keys[cat];
		return keys.Count();
	}

	//! Имя голоса из конфига модели (CfgVehicles dmBotVoice). Fallback "default".
	static string GetVoiceName(notnull EntityAI entity)
	{
		string voice = entity.ConfigGetString("dmBotVoice");
		if (voice == "")
			return "default";
		return voice;
	}

	//! Ключ реплики по lineId (cat = lineId/100, index = lineId%100). "" = нет реплики.
	private static string GetKeyword(int lineId)
	{
		int cat = lineId / 100;
		int index = lineId % 100;
		if (cat < 0 || cat >= s_Keys.Count())
			return "";
		TStringArray keys = s_Keys[cat];
		if (index < 1 || index > keys.Count())
			return "";
		return keys[index - 1];
	}

	//! lineId = category*100 + index (index 1..count). Пустая строка = нет реплики.
	static string GetSoundSetName(string voice, int lineId)
	{
		string keyword = GetKeyword(lineId);
		if (keyword == "")
			return "";
		return "dmBotVoice_" + voice + "_" + keyword + "_SoundSet";
	}

	//! Длительность рта (сек) — из конфига CfgSoundSets <set> duration. Fallback 1.0.
	static float GetTalkDuration(string voice, int lineId)
	{
		string name = GetSoundSetName(voice, lineId);
		if (name == "")
			return 1.0;

		string path = "CfgSoundSets " + name + " duration";
		if (GetGame().ConfigIsExisting(path))
			return GetGame().ConfigGetFloat(path);

		return 1.0;
	}
}
