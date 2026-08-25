class CfgPatches
{
	class dmBotorama
	{
		units[]= {};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]= {
			"DZ_Characters"
		};
	};
};

class CfgMods 
{
	class dmBotorama
	{
		name = "botorama";
		author = "devalio";
		version = 1.0;
		type = "mod";
		defines[] = { "DM_BOT_DEBUG", "DM_BOT_TRACE" };	
		class defs 
		{
			class worldScriptModule {
				value = "";
				files[] = {
					"botorama/cons/4_World",
					"botorama/core/4_World"
				};
			};
			class missionScriptModule
			{
				value="";
				files[]={
					"botorama/cons/5_Mission",
					"botorama/core/5_Mission"
				};
			};
		}; 
	};
};

class CfgVehicles
{
	class SurvivorM_Mirek;
	class SurvivorM_Denis;
	class SurvivorF_Eva;

	class dmAI_SurvivorM_Mirek : SurvivorM_Mirek
	{
		scope = 2;
	};
	class dmAI_SurvivorM_Denis : SurvivorM_Denis
	{
		scope = 2;
	};
	class dmAI_SurvivorF_Eva : SurvivorF_Eva
	{
		scope = 2;
	};
};
