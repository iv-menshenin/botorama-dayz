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
			"dmBotorama_Loadout",
			"dmBotorama_Map",
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
		defines[] = { "DM_BOT_PROFILE", "DM_BOT_DEBUG_BODY", "DM_BOT_DEBUG_SURVIVAL", "DM_BOT_DEBUG_COVER", "DM_BOT_DEBUG_CAMP", "DM_BOT_DEBUG_ARSENAL", "DM_BOT_DEBUG_FSM_SHOOTING", "DM_BOT_DEBUG_FSM_RUSHING", "DM_BOT_DEBUG_FSM_HUNTING", "DM_BOT_DEBUG_FSM_FLANKING", "DM_BOT_DEBUG_FSM_KAMPING", "DM_BOT_DEBUG_FSM_FIGHTING", "DM_BOT_DEBUG_PERSIST", "DM_BOT_DEBUG_MOTION", "DM_BOT_DEBUG_ATTENTION", "DM_PERCEPTION_DEBUG" };
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
		dmBotVoice = "default";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Denis : SurvivorM_Denis
	{
		scope = 2;
		dmBotVoice = "default";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Boris : SurvivorM_Boris
	{
		scope = 2;
		dmBotVoice = "default";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Cyril : SurvivorM_Cyril
	{
		scope = 2;
		dmBotVoice = "default";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Elias : SurvivorM_Elias
	{
		scope = 2;
		dmBotVoice = "default";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Francis : SurvivorM_Francis
	{
		scope = 2;
		dmBotVoice = "severe";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Guo : SurvivorM_Guo
	{
		scope = 2;
		dmBotVoice = "severe";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Hassan : SurvivorM_Hassan
	{
		scope = 2;
		dmBotVoice = "severe";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Indar : SurvivorM_Indar
	{
		scope = 2;
		dmBotVoice = "severe";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Jose : SurvivorM_Jose
	{
		scope = 2;
		dmBotVoice = "severe";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Kaito : SurvivorM_Kaito
	{
		scope = 2;
		dmBotVoice = "hasty";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Lewis : SurvivorM_Lewis
	{
		scope = 2;
		dmBotVoice = "hasty";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Manua : SurvivorM_Manua
	{
		scope = 2;
		dmBotVoice = "hasty";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Niki : SurvivorM_Niki
	{
		scope = 2;
		dmBotVoice = "hasty";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Oliver : SurvivorM_Oliver
	{
		scope = 2;
		dmBotVoice = "hasty";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Peter : SurvivorM_Peter
	{
		scope = 2;
		dmBotVoice = "calm";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Quinn : SurvivorM_Quinn
	{
		scope = 2;
		dmBotVoice = "calm";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Rolf : SurvivorM_Rolf
	{
		scope = 2;
		dmBotVoice = "calm";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Seth : SurvivorM_Seth
	{
		scope = 2;
		dmBotVoice = "calm";
		class enfAnimSys : enfAnimSys
		{
			graphName = "dm_core\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Taiki : SurvivorM_Taiki
	{
		scope = 2;
		dmBotVoice = "calm";
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

	// default
	class dmBotVoice_default_dandy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\dandy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_what_a_fruit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\what_a_fruit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_sour_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\sour_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_like_a_bum_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\like_a_bum",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_life_worn_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\life_worn",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_familiar_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\familiar_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_hey_there_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\hey_there",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_my_respects_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\my_respects",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_glad_to_see_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\glad_to_see",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_what_people_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\greeting\what_people",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_why_not_dead_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\wake\why_not_dead",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_it_hurts_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\wake\it_hurts",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_screw_it_all_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\wake\screw_it_all",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_what_happened_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\wake\what_happened",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_screw_everyone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\wake\screw_everyone",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_best_driver_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\passenger\best_driver",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_not_firewood_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\passenger\not_firewood",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_should_take_bus_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\passenger\should_take_bus",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_that_hurt_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\passenger\that_hurt",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_still_alive_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\passenger\still_alive",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_so_sleepy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\idle\so_sleepy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_no_money_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\idle\no_money",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_good_days_work_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\idle\good_days_work",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_shoot_someone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\idle\shoot_someone",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_greedy_bastard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\patrol\greedy_bastard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_you_are_the_man_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\patrol\you_are_the_man",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_west_is_fine_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\patrol\west_is_fine",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_shoot_a_boar_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\patrol\shoot_a_boar",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_pissed_himself_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\patrol\pissed_himself",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_put_gun_down_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\aimed_at\put_gun_down",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_where_aiming_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\aimed_at\where_aiming",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_you_joking_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\aimed_at\you_joking",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_careful_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\aimed_at\careful",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_never_point_gun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\aimed_at\never_point_gun",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_shooting_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\heard_shot\shooting",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_someone_fun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\heard_shot\someone_fun",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_they_going_hard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\heard_shot\they_going_hard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_action_without_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\heard_shot\action_without_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_no_rest_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\heard_shot\no_rest",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_what_a_hit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\got_shot\what_a_hit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_on_me_on_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\got_shot\on_me_on_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_im_shot_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\got_shot\im_shot",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_bandaging_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\got_shot\bandaging",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_bleeding_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\got_shot\bleeding",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_here_we_go_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\combat\here_we_go",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_fun_begins_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\combat\fun_begins",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_contact_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\combat\contact",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_who_else_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\combat\who_else",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_future_corpses_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\combat\future_corpses",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_lets_go_ready_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\escort\lets_go_ready",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_anywhere_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\escort\anywhere",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_default_no_time_to_smoke_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\default\escort\no_time_to_smoke",
				1
			}
		};
		volume=1;
	};

	// severe
	class dmBotVoice_severe_dandy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\dandy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_what_a_fruit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\what_a_fruit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_sour_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\sour_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_like_a_bum_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\like_a_bum",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_life_worn_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\life_worn",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_familiar_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\familiar_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_hey_there_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\hey_there",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_my_respects_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\my_respects",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_glad_to_see_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\glad_to_see",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_what_people_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\greeting\what_people",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_why_not_dead_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\wake\why_not_dead",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_it_hurts_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\wake\it_hurts",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_screw_it_all_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\wake\screw_it_all",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_what_happened_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\wake\what_happened",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_screw_everyone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\wake\screw_everyone",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_best_driver_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\passenger\best_driver",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_not_firewood_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\passenger\not_firewood",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_should_take_bus_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\passenger\should_take_bus",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_that_hurt_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\passenger\that_hurt",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_still_alive_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\passenger\still_alive",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_so_sleepy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\idle\so_sleepy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_no_money_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\idle\no_money",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_good_days_work_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\idle\good_days_work",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_shoot_someone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\idle\shoot_someone",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_greedy_bastard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\patrol\greedy_bastard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_you_are_the_man_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\patrol\you_are_the_man",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_west_is_fine_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\patrol\west_is_fine",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_shoot_a_boar_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\patrol\shoot_a_boar",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_pissed_himself_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\patrol\pissed_himself",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_put_gun_down_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\aimed_at\put_gun_down",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_where_aiming_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\aimed_at\where_aiming",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_you_joking_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\aimed_at\you_joking",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_careful_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\aimed_at\careful",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_never_point_gun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\aimed_at\never_point_gun",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_shooting_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\heard_shot\shooting",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_someone_fun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\heard_shot\someone_fun",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_they_going_hard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\heard_shot\they_going_hard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_action_without_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\heard_shot\action_without_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_no_rest_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\heard_shot\no_rest",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_what_a_hit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\got_shot\what_a_hit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_on_me_on_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\got_shot\on_me_on_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_im_shot_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\got_shot\im_shot",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_bandaging_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\got_shot\bandaging",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_bleeding_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\got_shot\bleeding",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_here_we_go_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\combat\here_we_go",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_fun_begins_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\combat\fun_begins",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_contact_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\combat\contact",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_who_else_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\combat\who_else",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_future_corpses_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\combat\future_corpses",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_lets_go_ready_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\escort\lets_go_ready",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_anywhere_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\escort\anywhere",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_severe_no_time_to_smoke_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\severe\escort\no_time_to_smoke",
				1
			}
		};
		volume=1;
	};

	// hasty
	class dmBotVoice_hasty_dandy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\dandy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_what_a_fruit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\what_a_fruit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_sour_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\sour_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_like_a_bum_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\like_a_bum",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_life_worn_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\life_worn",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_familiar_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\familiar_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_hey_there_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\hey_there",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_my_respects_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\my_respects",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_glad_to_see_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\glad_to_see",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_what_people_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\greeting\what_people",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_why_not_dead_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\wake\why_not_dead",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_it_hurts_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\wake\it_hurts",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_screw_it_all_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\wake\screw_it_all",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_what_happened_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\wake\what_happened",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_screw_everyone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\wake\screw_everyone",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_best_driver_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\passenger\best_driver",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_not_firewood_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\passenger\not_firewood",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_should_take_bus_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\passenger\should_take_bus",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_that_hurt_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\passenger\that_hurt",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_still_alive_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\passenger\still_alive",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_so_sleepy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\idle\so_sleepy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_no_money_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\idle\no_money",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_good_days_work_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\idle\good_days_work",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_shoot_someone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\idle\shoot_someone",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_greedy_bastard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\patrol\greedy_bastard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_you_are_the_man_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\patrol\you_are_the_man",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_west_is_fine_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\patrol\west_is_fine",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_shoot_a_boar_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\patrol\shoot_a_boar",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_pissed_himself_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\patrol\pissed_himself",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_put_gun_down_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\aimed_at\put_gun_down",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_where_aiming_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\aimed_at\where_aiming",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_you_joking_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\aimed_at\you_joking",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_careful_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\aimed_at\careful",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_never_point_gun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\aimed_at\never_point_gun",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_shooting_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\heard_shot\shooting",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_someone_fun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\heard_shot\someone_fun",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_they_going_hard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\heard_shot\they_going_hard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_action_without_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\heard_shot\action_without_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_no_rest_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\heard_shot\no_rest",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_what_a_hit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\got_shot\what_a_hit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_on_me_on_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\got_shot\on_me_on_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_im_shot_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\got_shot\im_shot",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_bandaging_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\got_shot\bandaging",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_bleeding_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\got_shot\bleeding",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_here_we_go_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\combat\here_we_go",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_fun_begins_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\combat\fun_begins",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_contact_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\combat\contact",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_who_else_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\combat\who_else",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_future_corpses_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\combat\future_corpses",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_lets_go_ready_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\escort\lets_go_ready",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_anywhere_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\escort\anywhere",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_hasty_no_time_to_smoke_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\hasty\escort\no_time_to_smoke",
				1
			}
		};
		volume=1;
	};

	// calm
	class dmBotVoice_calm_dandy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\dandy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_what_a_fruit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\what_a_fruit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_sour_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\sour_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_like_a_bum_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\like_a_bum",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_life_worn_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\life_worn",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_familiar_face_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\familiar_face",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_hey_there_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\hey_there",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_my_respects_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\my_respects",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_glad_to_see_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\glad_to_see",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_what_people_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\greeting\what_people",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_why_not_dead_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\wake\why_not_dead",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_it_hurts_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\wake\it_hurts",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_screw_it_all_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\wake\screw_it_all",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_what_happened_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\wake\what_happened",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_screw_everyone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\wake\screw_everyone",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_best_driver_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\passenger\best_driver",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_not_firewood_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\passenger\not_firewood",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_should_take_bus_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\passenger\should_take_bus",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_that_hurt_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\passenger\that_hurt",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_still_alive_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\passenger\still_alive",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_so_sleepy_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\idle\so_sleepy",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_no_money_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\idle\no_money",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_good_days_work_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\idle\good_days_work",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_shoot_someone_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\idle\shoot_someone",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_greedy_bastard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\patrol\greedy_bastard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_you_are_the_man_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\patrol\you_are_the_man",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_west_is_fine_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\patrol\west_is_fine",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_shoot_a_boar_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\patrol\shoot_a_boar",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_pissed_himself_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\patrol\pissed_himself",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_put_gun_down_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\aimed_at\put_gun_down",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_where_aiming_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\aimed_at\where_aiming",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_you_joking_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\aimed_at\you_joking",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_careful_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\aimed_at\careful",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_never_point_gun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\aimed_at\never_point_gun",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_shooting_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\heard_shot\shooting",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_someone_fun_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\heard_shot\someone_fun",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_they_going_hard_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\heard_shot\they_going_hard",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_action_without_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\heard_shot\action_without_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_no_rest_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\heard_shot\no_rest",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_what_a_hit_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\got_shot\what_a_hit",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_on_me_on_me_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\got_shot\on_me_on_me",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_im_shot_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\got_shot\im_shot",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_bandaging_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\got_shot\bandaging",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_bleeding_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\got_shot\bleeding",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_here_we_go_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\combat\here_we_go",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_fun_begins_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\combat\fun_begins",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_contact_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\combat\contact",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_who_else_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\combat\who_else",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_future_corpses_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\combat\future_corpses",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_lets_go_ready_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\escort\lets_go_ready",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_anywhere_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\escort\anywhere",
				1
			}
		};
		volume=1;
	};
	class dmBotVoice_calm_no_time_to_smoke_SoundShader : baseCharacter_SoundShader
	{
		samples[]=
		{
			{
				"dm_core\voices\calm\escort\no_time_to_smoke",
				1
			}
		};
		volume=1;
	};

};

class CfgSoundSets
{
	class baseCharacter_SoundSet;

	// default
	class dmBotVoice_default_dandy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_dandy_SoundShader"
		};
		duration=1.85;
	};
	class dmBotVoice_default_what_a_fruit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_what_a_fruit_SoundShader"
		};
		duration=1.31;
	};
	class dmBotVoice_default_sour_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_sour_face_SoundShader"
		};
		duration=2.19;
	};
	class dmBotVoice_default_like_a_bum_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_like_a_bum_SoundShader"
		};
		duration=3.00;
	};
	class dmBotVoice_default_life_worn_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_life_worn_SoundShader"
		};
		duration=3.19;
	};
	class dmBotVoice_default_familiar_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_familiar_face_SoundShader"
		};
		duration=3.11;
	};
	class dmBotVoice_default_hey_there_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_hey_there_SoundShader"
		};
		duration=1.88;
	};
	class dmBotVoice_default_my_respects_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_my_respects_SoundShader"
		};
		duration=1.78;
	};
	class dmBotVoice_default_glad_to_see_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_glad_to_see_SoundShader"
		};
		duration=2.46;
	};
	class dmBotVoice_default_what_people_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_what_people_SoundShader"
		};
		duration=2.14;
	};
	class dmBotVoice_default_why_not_dead_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_why_not_dead_SoundShader"
		};
		duration=3.19;
	};
	class dmBotVoice_default_it_hurts_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_it_hurts_SoundShader"
		};
		duration=2.51;
	};
	class dmBotVoice_default_screw_it_all_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_screw_it_all_SoundShader"
		};
		duration=1.93;
	};
	class dmBotVoice_default_what_happened_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_what_happened_SoundShader"
		};
		duration=2.69;
	};
	class dmBotVoice_default_screw_everyone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_screw_everyone_SoundShader"
		};
		duration=2.22;
	};
	class dmBotVoice_default_best_driver_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_best_driver_SoundShader"
		};
		duration=2.93;
	};
	class dmBotVoice_default_not_firewood_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_not_firewood_SoundShader"
		};
		duration=2.06;
	};
	class dmBotVoice_default_should_take_bus_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_should_take_bus_SoundShader"
		};
		duration=1.96;
	};
	class dmBotVoice_default_that_hurt_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_that_hurt_SoundShader"
		};
		duration=2.69;
	};
	class dmBotVoice_default_still_alive_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_still_alive_SoundShader"
		};
		duration=3.11;
	};
	class dmBotVoice_default_so_sleepy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_so_sleepy_SoundShader"
		};
		duration=4.18;
	};
	class dmBotVoice_default_no_money_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_no_money_SoundShader"
		};
		duration=4.49;
	};
	class dmBotVoice_default_good_days_work_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_good_days_work_SoundShader"
		};
		duration=4.34;
	};
	class dmBotVoice_default_shoot_someone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_shoot_someone_SoundShader"
		};
		duration=2.77;
	};
	class dmBotVoice_default_greedy_bastard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_greedy_bastard_SoundShader"
		};
		duration=7.97;
	};
	class dmBotVoice_default_you_are_the_man_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_you_are_the_man_SoundShader"
		};
		duration=10.14;
	};
	class dmBotVoice_default_west_is_fine_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_west_is_fine_SoundShader"
		};
		duration=8.12;
	};
	class dmBotVoice_default_shoot_a_boar_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_shoot_a_boar_SoundShader"
		};
		duration=10.81;
	};
	class dmBotVoice_default_pissed_himself_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_pissed_himself_SoundShader"
		};
		duration=9.27;
	};
	class dmBotVoice_default_put_gun_down_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_put_gun_down_SoundShader"
		};
		duration=2.35;
	};
	class dmBotVoice_default_where_aiming_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_where_aiming_SoundShader"
		};
		duration=1.23;
	};
	class dmBotVoice_default_you_joking_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_you_joking_SoundShader"
		};
		duration=1.85;
	};
	class dmBotVoice_default_careful_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_careful_SoundShader"
		};
		duration=1.10;
	};
	class dmBotVoice_default_never_point_gun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_never_point_gun_SoundShader"
		};
		duration=3.13;
	};
	class dmBotVoice_default_shooting_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_shooting_SoundShader"
		};
		duration=1.31;
	};
	class dmBotVoice_default_someone_fun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_someone_fun_SoundShader"
		};
		duration=1.85;
	};
	class dmBotVoice_default_they_going_hard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_they_going_hard_SoundShader"
		};
		duration=2.61;
	};
	class dmBotVoice_default_action_without_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_action_without_me_SoundShader"
		};
		duration=3.97;
	};
	class dmBotVoice_default_no_rest_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_no_rest_SoundShader"
		};
		duration=2.04;
	};
	class dmBotVoice_default_what_a_hit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_what_a_hit_SoundShader"
		};
		duration=1.72;
	};
	class dmBotVoice_default_on_me_on_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_on_me_on_me_SoundShader"
		};
		duration=1.59;
	};
	class dmBotVoice_default_im_shot_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_im_shot_SoundShader"
		};
		duration=1.07;
	};
	class dmBotVoice_default_bandaging_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_bandaging_SoundShader"
		};
		duration=0.84;
	};
	class dmBotVoice_default_bleeding_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_bleeding_SoundShader"
		};
		duration=0.97;
	};
	class dmBotVoice_default_here_we_go_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_here_we_go_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_default_fun_begins_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_fun_begins_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_default_contact_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_contact_SoundShader"
		};
		duration=0.73;
	};
	class dmBotVoice_default_who_else_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_who_else_SoundShader"
		};
		duration=1.62;
	};
	class dmBotVoice_default_future_corpses_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_future_corpses_SoundShader"
		};
		duration=1.54;
	};
	class dmBotVoice_default_lets_go_ready_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_lets_go_ready_SoundShader"
		};
		duration=1.49;
	};
	class dmBotVoice_default_anywhere_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_anywhere_SoundShader"
		};
		duration=1.72;
	};
	class dmBotVoice_default_no_time_to_smoke_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_default_no_time_to_smoke_SoundShader"
		};
		duration=2.14;
	};

	// severe
	class dmBotVoice_severe_dandy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_dandy_SoundShader"
		};
		duration=1.46;
	};
	class dmBotVoice_severe_what_a_fruit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_what_a_fruit_SoundShader"
		};
		duration=1.25;
	};
	class dmBotVoice_severe_sour_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_sour_face_SoundShader"
		};
		duration=1.23;
	};
	class dmBotVoice_severe_like_a_bum_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_like_a_bum_SoundShader"
		};
		duration=1.44;
	};
	class dmBotVoice_severe_life_worn_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_life_worn_SoundShader"
		};
		duration=1.91;
	};
	class dmBotVoice_severe_familiar_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_familiar_face_SoundShader"
		};
		duration=2.64;
	};
	class dmBotVoice_severe_hey_there_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_hey_there_SoundShader"
		};
		duration=1.62;
	};
	class dmBotVoice_severe_my_respects_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_my_respects_SoundShader"
		};
		duration=1.31;
	};
	class dmBotVoice_severe_glad_to_see_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_glad_to_see_SoundShader"
		};
		duration=1.96;
	};
	class dmBotVoice_severe_what_people_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_what_people_SoundShader"
		};
		duration=2.04;
	};
	class dmBotVoice_severe_why_not_dead_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_why_not_dead_SoundShader"
		};
		duration=2.19;
	};
	class dmBotVoice_severe_it_hurts_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_it_hurts_SoundShader"
		};
		duration=1.62;
	};
	class dmBotVoice_severe_screw_it_all_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_screw_it_all_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_severe_what_happened_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_what_happened_SoundShader"
		};
		duration=1.88;
	};
	class dmBotVoice_severe_screw_everyone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_screw_everyone_SoundShader"
		};
		duration=2.66;
	};
	class dmBotVoice_severe_best_driver_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_best_driver_SoundShader"
		};
		duration=2.51;
	};
	class dmBotVoice_severe_not_firewood_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_not_firewood_SoundShader"
		};
		duration=1.70;
	};
	class dmBotVoice_severe_should_take_bus_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_should_take_bus_SoundShader"
		};
		duration=1.93;
	};
	class dmBotVoice_severe_that_hurt_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_that_hurt_SoundShader"
		};
		duration=2.30;
	};
	class dmBotVoice_severe_still_alive_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_still_alive_SoundShader"
		};
		duration=2.87;
	};
	class dmBotVoice_severe_so_sleepy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_so_sleepy_SoundShader"
		};
		duration=4.13;
	};
	class dmBotVoice_severe_no_money_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_no_money_SoundShader"
		};
		duration=3.71;
	};
	class dmBotVoice_severe_good_days_work_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_good_days_work_SoundShader"
		};
		duration=3.84;
	};
	class dmBotVoice_severe_shoot_someone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_shoot_someone_SoundShader"
		};
		duration=2.61;
	};
	class dmBotVoice_severe_greedy_bastard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_greedy_bastard_SoundShader"
		};
		duration=9.56;
	};
	class dmBotVoice_severe_you_are_the_man_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_you_are_the_man_SoundShader"
		};
		duration=11.49;
	};
	class dmBotVoice_severe_west_is_fine_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_west_is_fine_SoundShader"
		};
		duration=8.41;
	};
	class dmBotVoice_severe_shoot_a_boar_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_shoot_a_boar_SoundShader"
		};
		duration=9.33;
	};
	class dmBotVoice_severe_pissed_himself_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_pissed_himself_SoundShader"
		};
		duration=9.56;
	};
	class dmBotVoice_severe_put_gun_down_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_put_gun_down_SoundShader"
		};
		duration=2.32;
	};
	class dmBotVoice_severe_where_aiming_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_where_aiming_SoundShader"
		};
		duration=1.52;
	};
	class dmBotVoice_severe_you_joking_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_you_joking_SoundShader"
		};
		duration=1.62;
	};
	class dmBotVoice_severe_careful_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_careful_SoundShader"
		};
		duration=1.59;
	};
	class dmBotVoice_severe_never_point_gun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_never_point_gun_SoundShader"
		};
		duration=3.71;
	};
	class dmBotVoice_severe_shooting_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_shooting_SoundShader"
		};
		duration=2.06;
	};
	class dmBotVoice_severe_someone_fun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_someone_fun_SoundShader"
		};
		duration=2.43;
	};
	class dmBotVoice_severe_they_going_hard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_they_going_hard_SoundShader"
		};
		duration=2.09;
	};
	class dmBotVoice_severe_action_without_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_action_without_me_SoundShader"
		};
		duration=3.34;
	};
	class dmBotVoice_severe_no_rest_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_no_rest_SoundShader"
		};
		duration=2.35;
	};
	class dmBotVoice_severe_what_a_hit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_what_a_hit_SoundShader"
		};
		duration=1.88;
	};
	class dmBotVoice_severe_on_me_on_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_on_me_on_me_SoundShader"
		};
		duration=1.62;
	};
	class dmBotVoice_severe_im_shot_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_im_shot_SoundShader"
		};
		duration=1.38;
	};
	class dmBotVoice_severe_bandaging_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_bandaging_SoundShader"
		};
		duration=1.07;
	};
	class dmBotVoice_severe_bleeding_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_bleeding_SoundShader"
		};
		duration=1.25;
	};
	class dmBotVoice_severe_here_we_go_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_here_we_go_SoundShader"
		};
		duration=0.91;
	};
	class dmBotVoice_severe_fun_begins_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_fun_begins_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_severe_contact_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_contact_SoundShader"
		};
		duration=0.76;
	};
	class dmBotVoice_severe_who_else_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_who_else_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_severe_future_corpses_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_future_corpses_SoundShader"
		};
		duration=2.14;
	};
	class dmBotVoice_severe_lets_go_ready_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_lets_go_ready_SoundShader"
		};
		duration=1.25;
	};
	class dmBotVoice_severe_anywhere_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_anywhere_SoundShader"
		};
		duration=2.01;
	};
	class dmBotVoice_severe_no_time_to_smoke_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_severe_no_time_to_smoke_SoundShader"
		};
		duration=4.57;
	};

	// hasty
	class dmBotVoice_hasty_dandy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_dandy_SoundShader"
		};
		duration=1.49;
	};
	class dmBotVoice_hasty_what_a_fruit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_what_a_fruit_SoundShader"
		};
		duration=0.73;
	};
	class dmBotVoice_hasty_sour_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_sour_face_SoundShader"
		};
		duration=0.94;
	};
	class dmBotVoice_hasty_like_a_bum_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_like_a_bum_SoundShader"
		};
		duration=1.72;
	};
	class dmBotVoice_hasty_life_worn_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_life_worn_SoundShader"
		};
		duration=1.80;
	};
	class dmBotVoice_hasty_familiar_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_familiar_face_SoundShader"
		};
		duration=2.32;
	};
	class dmBotVoice_hasty_hey_there_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_hey_there_SoundShader"
		};
		duration=1.04;
	};
	class dmBotVoice_hasty_my_respects_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_my_respects_SoundShader"
		};
		duration=1.59;
	};
	class dmBotVoice_hasty_glad_to_see_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_glad_to_see_SoundShader"
		};
		duration=1.31;
	};
	class dmBotVoice_hasty_what_people_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_what_people_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_hasty_why_not_dead_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_why_not_dead_SoundShader"
		};
		duration=2.22;
	};
	class dmBotVoice_hasty_it_hurts_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_it_hurts_SoundShader"
		};
		duration=1.44;
	};
	class dmBotVoice_hasty_screw_it_all_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_screw_it_all_SoundShader"
		};
		duration=1.25;
	};
	class dmBotVoice_hasty_what_happened_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_what_happened_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_hasty_screw_everyone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_screw_everyone_SoundShader"
		};
		duration=2.40;
	};
	class dmBotVoice_hasty_best_driver_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_best_driver_SoundShader"
		};
		duration=2.14;
	};
	class dmBotVoice_hasty_not_firewood_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_not_firewood_SoundShader"
		};
		duration=1.78;
	};
	class dmBotVoice_hasty_should_take_bus_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_should_take_bus_SoundShader"
		};
		duration=2.22;
	};
	class dmBotVoice_hasty_that_hurt_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_that_hurt_SoundShader"
		};
		duration=2.59;
	};
	class dmBotVoice_hasty_still_alive_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_still_alive_SoundShader"
		};
		duration=3.42;
	};
	class dmBotVoice_hasty_so_sleepy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_so_sleepy_SoundShader"
		};
		duration=3.68;
	};
	class dmBotVoice_hasty_no_money_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_no_money_SoundShader"
		};
		duration=4.26;
	};
	class dmBotVoice_hasty_good_days_work_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_good_days_work_SoundShader"
		};
		duration=4.70;
	};
	class dmBotVoice_hasty_shoot_someone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_shoot_someone_SoundShader"
		};
		duration=2.66;
	};
	class dmBotVoice_hasty_greedy_bastard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_greedy_bastard_SoundShader"
		};
		duration=7.08;
	};
	class dmBotVoice_hasty_you_are_the_man_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_you_are_the_man_SoundShader"
		};
		duration=10.45;
	};
	class dmBotVoice_hasty_west_is_fine_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_west_is_fine_SoundShader"
		};
		duration=7.99;
	};
	class dmBotVoice_hasty_shoot_a_boar_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_shoot_a_boar_SoundShader"
		};
		duration=10.42;
	};
	class dmBotVoice_hasty_pissed_himself_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_pissed_himself_SoundShader"
		};
		duration=13.17;
	};
	class dmBotVoice_hasty_put_gun_down_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_put_gun_down_SoundShader"
		};
		duration=2.98;
	};
	class dmBotVoice_hasty_where_aiming_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_where_aiming_SoundShader"
		};
		duration=1.52;
	};
	class dmBotVoice_hasty_you_joking_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_you_joking_SoundShader"
		};
		duration=3.37;
	};
	class dmBotVoice_hasty_careful_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_careful_SoundShader"
		};
		duration=3.00;
	};
	class dmBotVoice_hasty_never_point_gun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_never_point_gun_SoundShader"
		};
		duration=4.28;
	};
	class dmBotVoice_hasty_shooting_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_shooting_SoundShader"
		};
		duration=1.10;
	};
	class dmBotVoice_hasty_someone_fun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_someone_fun_SoundShader"
		};
		duration=1.59;
	};
	class dmBotVoice_hasty_they_going_hard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_they_going_hard_SoundShader"
		};
		duration=2.40;
	};
	class dmBotVoice_hasty_action_without_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_action_without_me_SoundShader"
		};
		duration=2.74;
	};
	class dmBotVoice_hasty_no_rest_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_no_rest_SoundShader"
		};
		duration=2.04;
	};
	class dmBotVoice_hasty_what_a_hit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_what_a_hit_SoundShader"
		};
		duration=1.18;
	};
	class dmBotVoice_hasty_on_me_on_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_on_me_on_me_SoundShader"
		};
		duration=1.62;
	};
	class dmBotVoice_hasty_im_shot_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_im_shot_SoundShader"
		};
		duration=2.30;
	};
	class dmBotVoice_hasty_bandaging_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_bandaging_SoundShader"
		};
		duration=1.49;
	};
	class dmBotVoice_hasty_bleeding_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_bleeding_SoundShader"
		};
		duration=1.67;
	};
	class dmBotVoice_hasty_here_we_go_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_here_we_go_SoundShader"
		};
		duration=1.49;
	};
	class dmBotVoice_hasty_fun_begins_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_fun_begins_SoundShader"
		};
		duration=1.54;
	};
	class dmBotVoice_hasty_contact_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_contact_SoundShader"
		};
		duration=1.75;
	};
	class dmBotVoice_hasty_who_else_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_who_else_SoundShader"
		};
		duration=2.53;
	};
	class dmBotVoice_hasty_future_corpses_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_future_corpses_SoundShader"
		};
		duration=2.72;
	};
	class dmBotVoice_hasty_lets_go_ready_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_lets_go_ready_SoundShader"
		};
		duration=1.67;
	};
	class dmBotVoice_hasty_anywhere_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_anywhere_SoundShader"
		};
		duration=1.33;
	};
	class dmBotVoice_hasty_no_time_to_smoke_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_hasty_no_time_to_smoke_SoundShader"
		};
		duration=1.31;
	};

	// calm
	class dmBotVoice_calm_dandy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_dandy_SoundShader"
		};
		duration=1.44;
	};
	class dmBotVoice_calm_what_a_fruit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_what_a_fruit_SoundShader"
		};
		duration=3.34;
	};
	class dmBotVoice_calm_sour_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_sour_face_SoundShader"
		};
		duration=1.91;
	};
	class dmBotVoice_calm_like_a_bum_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_like_a_bum_SoundShader"
		};
		duration=3.94;
	};
	class dmBotVoice_calm_life_worn_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_life_worn_SoundShader"
		};
		duration=2.25;
	};
	class dmBotVoice_calm_familiar_face_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_familiar_face_SoundShader"
		};
		duration=3.06;
	};
	class dmBotVoice_calm_hey_there_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_hey_there_SoundShader"
		};
		duration=2.04;
	};
	class dmBotVoice_calm_my_respects_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_my_respects_SoundShader"
		};
		duration=1.96;
	};
	class dmBotVoice_calm_glad_to_see_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_glad_to_see_SoundShader"
		};
		duration=2.87;
	};
	class dmBotVoice_calm_what_people_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_what_people_SoundShader"
		};
		duration=2.46;
	};
	class dmBotVoice_calm_why_not_dead_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_why_not_dead_SoundShader"
		};
		duration=2.74;
	};
	class dmBotVoice_calm_it_hurts_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_it_hurts_SoundShader"
		};
		duration=2.22;
	};
	class dmBotVoice_calm_screw_it_all_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_screw_it_all_SoundShader"
		};
		duration=2.27;
	};
	class dmBotVoice_calm_what_happened_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_what_happened_SoundShader"
		};
		duration=2.74;
	};
	class dmBotVoice_calm_screw_everyone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_screw_everyone_SoundShader"
		};
		duration=2.90;
	};
	class dmBotVoice_calm_best_driver_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_best_driver_SoundShader"
		};
		duration=2.85;
	};
	class dmBotVoice_calm_not_firewood_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_not_firewood_SoundShader"
		};
		duration=3.19;
	};
	class dmBotVoice_calm_should_take_bus_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_should_take_bus_SoundShader"
		};
		duration=4.62;
	};
	class dmBotVoice_calm_that_hurt_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_that_hurt_SoundShader"
		};
		duration=2.56;
	};
	class dmBotVoice_calm_still_alive_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_still_alive_SoundShader"
		};
		duration=3.92;
	};
	class dmBotVoice_calm_so_sleepy_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_so_sleepy_SoundShader"
		};
		duration=3.16;
	};
	class dmBotVoice_calm_no_money_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_no_money_SoundShader"
		};
		duration=4.96;
	};
	class dmBotVoice_calm_good_days_work_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_good_days_work_SoundShader"
		};
		duration=5.12;
	};
	class dmBotVoice_calm_shoot_someone_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_shoot_someone_SoundShader"
		};
		duration=3.94;
	};
	class dmBotVoice_calm_greedy_bastard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_greedy_bastard_SoundShader"
		};
		duration=9.51;
	};
	class dmBotVoice_calm_you_are_the_man_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_you_are_the_man_SoundShader"
		};
		duration=12.30;
	};
	class dmBotVoice_calm_west_is_fine_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_west_is_fine_SoundShader"
		};
		duration=9.90;
	};
	class dmBotVoice_calm_shoot_a_boar_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_shoot_a_boar_SoundShader"
		};
		duration=11.08;
	};
	class dmBotVoice_calm_pissed_himself_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_pissed_himself_SoundShader"
		};
		duration=9.35;
	};
	class dmBotVoice_calm_put_gun_down_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_put_gun_down_SoundShader"
		};
		duration=2.25;
	};
	class dmBotVoice_calm_where_aiming_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_where_aiming_SoundShader"
		};
		duration=1.54;
	};
	class dmBotVoice_calm_you_joking_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_you_joking_SoundShader"
		};
		duration=1.91;
	};
	class dmBotVoice_calm_careful_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_careful_SoundShader"
		};
		duration=1.54;
	};
	class dmBotVoice_calm_never_point_gun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_never_point_gun_SoundShader"
		};
		duration=5.33;
	};
	class dmBotVoice_calm_shooting_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_shooting_SoundShader"
		};
		duration=2.27;
	};
	class dmBotVoice_calm_someone_fun_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_someone_fun_SoundShader"
		};
		duration=2.40;
	};
	class dmBotVoice_calm_they_going_hard_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_they_going_hard_SoundShader"
		};
		duration=2.77;
	};
	class dmBotVoice_calm_action_without_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_action_without_me_SoundShader"
		};
		duration=3.47;
	};
	class dmBotVoice_calm_no_rest_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_no_rest_SoundShader"
		};
		duration=3.50;
	};
	class dmBotVoice_calm_what_a_hit_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_what_a_hit_SoundShader"
		};
		duration=2.14;
	};
	class dmBotVoice_calm_on_me_on_me_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_on_me_on_me_SoundShader"
		};
		duration=1.41;
	};
	class dmBotVoice_calm_im_shot_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_im_shot_SoundShader"
		};
		duration=1.36;
	};
	class dmBotVoice_calm_bandaging_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_bandaging_SoundShader"
		};
		duration=1.20;
	};
	class dmBotVoice_calm_bleeding_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_bleeding_SoundShader"
		};
		duration=1.18;
	};
	class dmBotVoice_calm_here_we_go_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_here_we_go_SoundShader"
		};
		duration=1.49;
	};
	class dmBotVoice_calm_fun_begins_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_fun_begins_SoundShader"
		};
		duration=1.85;
	};
	class dmBotVoice_calm_contact_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_contact_SoundShader"
		};
		duration=0.84;
	};
	class dmBotVoice_calm_who_else_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_who_else_SoundShader"
		};
		duration=1.78;
	};
	class dmBotVoice_calm_future_corpses_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_future_corpses_SoundShader"
		};
		duration=3.00;
	};
	class dmBotVoice_calm_lets_go_ready_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_lets_go_ready_SoundShader"
		};
		duration=2.38;
	};
	class dmBotVoice_calm_anywhere_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_anywhere_SoundShader"
		};
		duration=2.40;
	};
	class dmBotVoice_calm_no_time_to_smoke_SoundSet : baseCharacter_SoundSet
	{
		soundShaders[]=
		{
			"dmBotVoice_calm_no_time_to_smoke_SoundShader"
		};
		duration=3.13;
	};

};
