class CfgPatches
{
	class dmBotorama_Core
	{
		units[]= {};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]= {
			"dmBotorama_Cons",
			"dmBotorama_Roads",
			"dmBotorama_Reg",
			"DZ_Characters",
			"DZ_Anims_Anm_Player",
			"DZ_Anims_Cfg",
			"DZ_Sounds_Effects",
		};
	};
};

class CfgMods 
{
	class dmBotorama_Core
	{
		name = "botorama";
		author = "devalio";
		type = "mod";
		defines[] = { "DM_BOT_PROFILE", "DM_BOT_DEBUG_FSM", "DM_WEAPON_DEBUG_FSM", "DM_BOT_DEBUG_SPAWN", "DM_BOT_DEBUG_BODY", "DM_BOT_DEBUG_CAR", "DM_BOT_DEBUG_DRIVE_TELEMETRY" };
		class defs 
		{
			class gameScriptModule {
				value = "";
				files[] = {
					"dm_core/3_Game",
				};
			};
			class worldScriptModule {
				value = "";
				files[] = {
					"dm_core/4_World",
				};
			};
			class missionScriptModule {
				value = "";
				files[] = {
					"dm_core/5_Mission",
				};
			};
		}; 
	};
};

class CfgVehicles
{
	class SurvivorM_Mirek;
	class SurvivorM_Denis;
	class SurvivorM_Boris;
	class SurvivorM_Cyril;
	class SurvivorM_Elias;
	class SurvivorM_Francis;
	class SurvivorM_Guo;
	class SurvivorM_Hassan;
	class SurvivorM_Indar;
	class SurvivorM_Jose;
	class SurvivorM_Kaito;
	class SurvivorM_Lewis;
	class SurvivorM_Manua;
	class SurvivorM_Niki;
	class SurvivorM_Oliver;
	class SurvivorM_Peter;
	class SurvivorM_Quinn;
	class SurvivorM_Rolf;
	class SurvivorM_Seth;
	class SurvivorM_Taiki;
	class SurvivorF_Linda;
	class SurvivorF_Maria;
	class SurvivorF_Frida;
	class SurvivorF_Gabi;
	class SurvivorF_Helga;
	class SurvivorF_Irena;
	class SurvivorF_Judy;
	class SurvivorF_Keiko;
	class SurvivorF_Eva;
	class SurvivorF_Naomi;
	class SurvivorF_Baty;

	class enfAnimSys;

	class dmAI_SurvivorM_Mirek : SurvivorM_Mirek
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Denis : SurvivorM_Denis
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Boris : SurvivorM_Boris
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Cyril : SurvivorM_Cyril
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Elias : SurvivorM_Elias
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Francis : SurvivorM_Francis
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Guo : SurvivorM_Guo
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Hassan : SurvivorM_Hassan
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Indar : SurvivorM_Indar
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Jose : SurvivorM_Jose
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Kaito : SurvivorM_Kaito
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Lewis : SurvivorM_Lewis
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Manua : SurvivorM_Manua
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Niki : SurvivorM_Niki
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Oliver : SurvivorM_Oliver
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Peter : SurvivorM_Peter
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Quinn : SurvivorM_Quinn
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Rolf : SurvivorM_Rolf
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Seth : SurvivorM_Seth
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Taiki : SurvivorM_Taiki
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Linda : SurvivorF_Linda
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Maria : SurvivorF_Maria
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Frida : SurvivorF_Frida
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Gabi : SurvivorF_Gabi
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Helga : SurvivorF_Helga
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Irena : SurvivorF_Irena
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Judy : SurvivorF_Judy
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Keiko : SurvivorF_Keiko
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Eva : SurvivorF_Eva
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Naomi : SurvivorF_Naomi
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Baty : SurvivorF_Baty
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
};

class CfgSoundShaders
{
	class baseCharacter_SoundShader;

	// greeting
	class dmBotVoice_dandy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\dandy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_what_a_fruit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\what_a_fruit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_sour_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\sour_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_like_a_bum_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\like_a_bum",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_life_worn_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\life_worn",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_familiar_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\familiar_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hey_there_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\hey_there",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_my_respects_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\my_respects",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_glad_to_see_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\glad_to_see",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_what_people_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\greeting\what_people",
				1
			}
		};
		volume=1;
	};

	// wake
	class dmBotVoice_why_not_dead_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\wake\why_not_dead",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_it_hurts_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\wake\it_hurts",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_screw_it_all_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\wake\screw_it_all",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_what_happened_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\wake\what_happened",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_screw_everyone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\wake\screw_everyone",
				1
			}
		};
		volume=1;
	};

	// passenger
	class dmBotVoice_best_driver_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\passenger\best_driver",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_not_firewood_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\passenger\not_firewood",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_should_take_bus_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\passenger\should_take_bus",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_that_hurt_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\passenger\that_hurt",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_still_alive_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\passenger\still_alive",
				1
			}
		};
		volume=1;
	};

	// idle
	class dmBotVoice_so_sleepy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\idle\so_sleepy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_no_money_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\idle\no_money",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_good_days_work_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\idle\good_days_work",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_shoot_someone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\idle\shoot_someone",
				1
			}
		};
		volume=1;
	};

	// patrol
	class dmBotVoice_greedy_bastard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\patrol\greedy_bastard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_you_are_the_man_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\patrol\you_are_the_man",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_west_is_fine_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\patrol\west_is_fine",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_shoot_a_boar_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\patrol\shoot_a_boar",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_pissed_himself_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\patrol\pissed_himself",
				1
			}
		};
		volume=1;
	};

	// aimed_at
	class dmBotVoice_put_gun_down_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\aimed_at\put_gun_down",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_where_aiming_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\aimed_at\where_aiming",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_you_joking_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\aimed_at\you_joking",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_careful_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\aimed_at\careful",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_never_point_gun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\aimed_at\never_point_gun",
				1
			}
		};
		volume=1;
	};

	// heard_shot
	class dmBotVoice_shooting_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\heard_shot\shooting",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_someone_fun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\heard_shot\someone_fun",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_they_going_hard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\heard_shot\they_going_hard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_action_without_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\heard_shot\action_without_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_no_rest_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\heard_shot\no_rest",
				1
			}
		};
		volume=1;
	};

	// got_shot
	class dmBotVoice_what_a_hit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\got_shot\what_a_hit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_on_me_on_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\got_shot\on_me_on_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_im_shot_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\got_shot\im_shot",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_bandaging_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\got_shot\bandaging",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_bleeding_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\got_shot\bleeding",
				1
			}
		};
		volume=1;
	};

	// combat
	class dmBotVoice_here_we_go_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\combat\here_we_go",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_fun_begins_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\combat\fun_begins",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_contact_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\combat\contact",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_who_else_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\combat\who_else",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_future_corpses_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\combat\future_corpses",
				1
			}
		};
		volume=1;
	};

	// escort
	class dmBotVoice_lets_go_ready_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\escort\lets_go_ready",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_anywhere_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\escort\anywhere",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_no_time_to_smoke_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\escort\no_time_to_smoke",
				1
			}
		};
		volume=1;
	};
};

class CfgSoundSets
{
	class baseCharacter_SoundSet;

	// greeting
	class dmBotVoice_dandy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_dandy_SoundShader"
		};
		duration=1.85;
	};
	class dmBotVoice_what_a_fruit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_what_a_fruit_SoundShader"
		};
		duration=1.31;
	};
	class dmBotVoice_sour_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_sour_face_SoundShader"
		};
		duration=2.19;
	};
	class dmBotVoice_like_a_bum_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_like_a_bum_SoundShader"
		};
		duration=3.00;
	};
	class dmBotVoice_life_worn_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_life_worn_SoundShader"
		};
		duration=3.19;
	};
	class dmBotVoice_familiar_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_familiar_face_SoundShader"
		};
		duration=3.11;
	};
	class dmBotVoice_hey_there_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hey_there_SoundShader"
		};
		duration=1.88;
	};
	class dmBotVoice_my_respects_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_my_respects_SoundShader"
		};
		duration=1.78;
	};
	class dmBotVoice_glad_to_see_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_glad_to_see_SoundShader"
		};
		duration=2.46;
	};
	class dmBotVoice_what_people_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_what_people_SoundShader"
		};
		duration=2.14;
	};

	// wake
	class dmBotVoice_why_not_dead_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_why_not_dead_SoundShader"
		};
		duration=3.19;
	};
	class dmBotVoice_it_hurts_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_it_hurts_SoundShader"
		};
		duration=2.51;
	};
	class dmBotVoice_screw_it_all_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_screw_it_all_SoundShader"
		};
		duration=1.93;
	};
	class dmBotVoice_what_happened_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_what_happened_SoundShader"
		};
		duration=2.69;
	};
	class dmBotVoice_screw_everyone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_screw_everyone_SoundShader"
		};
		duration=2.22;
	};

	// passenger
	class dmBotVoice_best_driver_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_best_driver_SoundShader"
		};
		duration=2.93;
	};
	class dmBotVoice_not_firewood_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_not_firewood_SoundShader"
		};
		duration=2.06;
	};
	class dmBotVoice_should_take_bus_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_should_take_bus_SoundShader"
		};
		duration=1.96;
	};
	class dmBotVoice_that_hurt_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_that_hurt_SoundShader"
		};
		duration=2.69;
	};
	class dmBotVoice_still_alive_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_still_alive_SoundShader"
		};
		duration=3.11;
	};

	// idle
	class dmBotVoice_so_sleepy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_so_sleepy_SoundShader"
		};
		duration=4.18;
	};
	class dmBotVoice_no_money_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_no_money_SoundShader"
		};
		duration=4.49;
	};
	class dmBotVoice_good_days_work_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_good_days_work_SoundShader"
		};
		duration=4.34;
	};
	class dmBotVoice_shoot_someone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_shoot_someone_SoundShader"
		};
		duration=2.77;
	};

	// patrol
	class dmBotVoice_greedy_bastard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_greedy_bastard_SoundShader"
		};
		duration=7.97;
	};
	class dmBotVoice_you_are_the_man_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_you_are_the_man_SoundShader"
		};
		duration=10.14;
	};
	class dmBotVoice_west_is_fine_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_west_is_fine_SoundShader"
		};
		duration=8.12;
	};
	class dmBotVoice_shoot_a_boar_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_shoot_a_boar_SoundShader"
		};
		duration=10.81;
	};
	class dmBotVoice_pissed_himself_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_pissed_himself_SoundShader"
		};
		duration=9.27;
	};

	// aimed_at
	class dmBotVoice_put_gun_down_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_put_gun_down_SoundShader"
		};
		duration=2.35;
	};
	class dmBotVoice_where_aiming_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_where_aiming_SoundShader"
		};
		duration=1.23;
	};
	class dmBotVoice_you_joking_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_you_joking_SoundShader"
		};
		duration=1.85;
	};
	class dmBotVoice_careful_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_careful_SoundShader"
		};
		duration=1.10;
	};
	class dmBotVoice_never_point_gun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_never_point_gun_SoundShader"
		};
		duration=3.13;
	};

	// heard_shot
	class dmBotVoice_shooting_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_shooting_SoundShader"
		};
		duration=1.31;
	};
	class dmBotVoice_someone_fun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_someone_fun_SoundShader"
		};
		duration=1.85;
	};
	class dmBotVoice_they_going_hard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_they_going_hard_SoundShader"
		};
		duration=2.61;
	};
	class dmBotVoice_action_without_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_action_without_me_SoundShader"
		};
		duration=3.97;
	};
	class dmBotVoice_no_rest_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_no_rest_SoundShader"
		};
		duration=2.04;
	};

	// got_shot
	class dmBotVoice_what_a_hit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_what_a_hit_SoundShader"
		};
		duration=1.72;
	};
	class dmBotVoice_on_me_on_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_on_me_on_me_SoundShader"
		};
		duration=1.59;
	};
	class dmBotVoice_im_shot_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_im_shot_SoundShader"
		};
		duration=1.07;
	};
	class dmBotVoice_bandaging_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_bandaging_SoundShader"
		};
		duration=0.84;
	};
	class dmBotVoice_bleeding_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_bleeding_SoundShader"
		};
		duration=0.97;
	};

	// combat
	class dmBotVoice_here_we_go_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_here_we_go_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_fun_begins_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_fun_begins_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_contact_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_contact_SoundShader"
		};
		duration=0.73;
	};
	class dmBotVoice_who_else_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_who_else_SoundShader"
		};
		duration=1.62;
	};
	class dmBotVoice_future_corpses_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_future_corpses_SoundShader"
		};
		duration=1.54;
	};

	// escort
	class dmBotVoice_lets_go_ready_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_lets_go_ready_SoundShader"
		};
		duration=1.49;
	};
	class dmBotVoice_anywhere_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_anywhere_SoundShader"
		};
		duration=1.72;
	};
	class dmBotVoice_no_time_to_smoke_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_no_time_to_smoke_SoundShader"
		};
		duration=2.14;
	};
};
