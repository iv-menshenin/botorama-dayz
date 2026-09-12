class CfgPatches
{
	class dmBotorama
	{
		units[]= {};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]= {
			"DZ_Characters",
			"DZ_Anims_Anm_Player",
			"DZ_Anims_Cfg"
		};
	};
};

class CfgMods 
{
	class dmBotorama
	{
		name = "botorama";
		author = "devalio";
		version = 2.4;
		type = "mod";
		defines[] = { "DM_BOT_PROFILE", "DM_BOT_DEBUG_EVADE", "DM_BOT_DEBUG_MEDICAL", "DM_BOT_DEBUG_VOICE" };
		class defs 
		{
			class gameScriptModule {
				value = "";
				files[] = {
					"botorama/cons/3_Game",
					"botorama/core/3_Game",
					"botorama/test/3_Game"
				};
			};
			class worldScriptModule {
				value = "";
				files[] = {
					"botorama/cons/4_World",
					"botorama/reg/4_World",
					"botorama/core/4_World",
					"botorama/loadout/4_World",
					"botorama/test/4_World"
				};
			};
			class missionScriptModule
			{
				value="";
				files[]={
					"botorama/cons/5_Mission",
					"botorama/core/5_Mission",
					"botorama/test/5_Mission"
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
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Denis : SurvivorM_Denis
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Boris : SurvivorM_Boris
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Cyril : SurvivorM_Cyril
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Elias : SurvivorM_Elias
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Francis : SurvivorM_Francis
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Guo : SurvivorM_Guo
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Hassan : SurvivorM_Hassan
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Indar : SurvivorM_Indar
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Jose : SurvivorM_Jose
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Kaito : SurvivorM_Kaito
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Lewis : SurvivorM_Lewis
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Manua : SurvivorM_Manua
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Niki : SurvivorM_Niki
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Oliver : SurvivorM_Oliver
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Peter : SurvivorM_Peter
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Quinn : SurvivorM_Quinn
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Rolf : SurvivorM_Rolf
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Seth : SurvivorM_Seth
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorM_Taiki : SurvivorM_Taiki
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Linda : SurvivorF_Linda
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Maria : SurvivorF_Maria
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Frida : SurvivorF_Frida
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Gabi : SurvivorF_Gabi
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Helga : SurvivorF_Helga
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Irena : SurvivorF_Irena
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Judy : SurvivorF_Judy
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Keiko : SurvivorF_Keiko
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Eva : SurvivorF_Eva
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Naomi : SurvivorF_Naomi
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
	class dmAI_SurvivorF_Baty : SurvivorF_Baty
	{
		scope = 2;
		class enfAnimSys : enfAnimSys
		{
			graphName = "botorama\Animations\player_main.agr";
		};
	};
};
