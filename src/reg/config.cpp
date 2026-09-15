class CfgPatches
{
	class dmBotorama_Reg
	{
		units[]= {};
		weapons[]={};
		requiredVersion=0.1;
		requiredAddons[]= {
			"dmBotorama_Cons",
			"DZ_Vehicles_Wheeled",
			"DZ_Vehicles_Parts",
		};
	};
};

class CfgMods 
{
	class dmBotorama_Reg
	{
		name = "botorama";
		author = "devalio";
		type = "mod";
		class defs 
		{
			class gameScriptModule {
				value = "";
				files[] = {
					"dm_reg/3_Game",
				};
			};
			class worldScriptModule {
				value = "";
				files[] = {
					"dm_reg/4_World",
				};
			};
		}; 
	};
};
