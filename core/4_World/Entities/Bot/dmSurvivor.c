//! Model-specific classes. The config (CfgVehicles) inherits the vanilla
//! SurvivorM_*/SurvivorF_* classes for the visual model.
class dmAI_SurvivorM_Mirek: dmAISurvivorBase{};
class dmAI_SurvivorM_Denis: dmAISurvivorBase{};
class dmAI_SurvivorM_Boris: dmAISurvivorBase{};
class dmAI_SurvivorM_Cyril: dmAISurvivorBase{};
class dmAI_SurvivorM_Elias: dmAISurvivorBase{};
class dmAI_SurvivorM_Francis: dmAISurvivorBase{};
class dmAI_SurvivorM_Guo: dmAISurvivorBase{};
class dmAI_SurvivorM_Hassan: dmAISurvivorBase{};
class dmAI_SurvivorM_Indar: dmAISurvivorBase{};
class dmAI_SurvivorM_Jose: dmAISurvivorBase{};
class dmAI_SurvivorM_Kaito: dmAISurvivorBase{};
class dmAI_SurvivorM_Lewis: dmAISurvivorBase{};
class dmAI_SurvivorM_Manua: dmAISurvivorBase{};
class dmAI_SurvivorM_Niki: dmAISurvivorBase{};
class dmAI_SurvivorM_Oliver: dmAISurvivorBase{};
class dmAI_SurvivorM_Peter: dmAISurvivorBase{};
class dmAI_SurvivorM_Quinn: dmAISurvivorBase{};
class dmAI_SurvivorM_Rolf: dmAISurvivorBase{};
class dmAI_SurvivorM_Seth: dmAISurvivorBase{};
class dmAI_SurvivorM_Taiki: dmAISurvivorBase{};
class dmAI_SurvivorF_Linda: dmAISurvivorBase{};
class dmAI_SurvivorF_Maria: dmAISurvivorBase{};
class dmAI_SurvivorF_Frida: dmAISurvivorBase{};
class dmAI_SurvivorF_Gabi: dmAISurvivorBase{};
class dmAI_SurvivorF_Helga: dmAISurvivorBase{};
class dmAI_SurvivorF_Irena: dmAISurvivorBase{};
class dmAI_SurvivorF_Judy: dmAISurvivorBase{};
class dmAI_SurvivorF_Keiko: dmAISurvivorBase{};
class dmAI_SurvivorF_Eva: dmAISurvivorBase{};
class dmAI_SurvivorF_Naomi: dmAISurvivorBase{};
class dmAI_SurvivorF_Baty: dmAISurvivorBase{};

class dmSurvivor
{
    static ref TStringArray classNames = {
        "dmAI_SurvivorM_Mirek",
        "dmAI_SurvivorM_Denis",
        "dmAI_SurvivorM_Boris",
        "dmAI_SurvivorM_Cyril",
        "dmAI_SurvivorM_Elias",
        "dmAI_SurvivorM_Francis",
        "dmAI_SurvivorM_Guo",
        "dmAI_SurvivorM_Hassan",
        "dmAI_SurvivorM_Indar",
        "dmAI_SurvivorM_Jose",
        "dmAI_SurvivorM_Kaito",
        "dmAI_SurvivorM_Lewis",
        "dmAI_SurvivorM_Manua",
        "dmAI_SurvivorM_Niki",
        "dmAI_SurvivorM_Oliver",
        "dmAI_SurvivorM_Peter",
        "dmAI_SurvivorM_Quinn",
        "dmAI_SurvivorM_Rolf",
        "dmAI_SurvivorM_Seth",
        "dmAI_SurvivorM_Taiki",
        "dmAI_SurvivorF_Linda",
        "dmAI_SurvivorF_Maria",
        "dmAI_SurvivorF_Frida",
        "dmAI_SurvivorF_Gabi",
        "dmAI_SurvivorF_Helga",
        "dmAI_SurvivorF_Irena",
        "dmAI_SurvivorF_Judy",
        "dmAI_SurvivorF_Keiko",
        "dmAI_SurvivorF_Eva",
        "dmAI_SurvivorF_Naomi",
        "dmAI_SurvivorF_Baty"
    };
    static ref TStringArray classNamesMale = {
        "dmAI_SurvivorM_Mirek",
        "dmAI_SurvivorM_Denis",
        "dmAI_SurvivorM_Boris",
        "dmAI_SurvivorM_Cyril",
        "dmAI_SurvivorM_Elias",
        "dmAI_SurvivorM_Francis",
        "dmAI_SurvivorM_Guo",
        "dmAI_SurvivorM_Hassan",
        "dmAI_SurvivorM_Indar",
        "dmAI_SurvivorM_Jose",
        "dmAI_SurvivorM_Kaito",
        "dmAI_SurvivorM_Lewis",
        "dmAI_SurvivorM_Manua",
        "dmAI_SurvivorM_Niki",
        "dmAI_SurvivorM_Oliver",
        "dmAI_SurvivorM_Peter",
        "dmAI_SurvivorM_Quinn",
        "dmAI_SurvivorM_Rolf",
        "dmAI_SurvivorM_Seth",
        "dmAI_SurvivorM_Taiki"
    };
    static ref TStringArray classNamesFemale = {
        "dmAI_SurvivorF_Linda",
        "dmAI_SurvivorF_Maria",
        "dmAI_SurvivorF_Frida",
        "dmAI_SurvivorF_Gabi",
        "dmAI_SurvivorF_Helga",
        "dmAI_SurvivorF_Irena",
        "dmAI_SurvivorF_Judy",
        "dmAI_SurvivorF_Keiko",
        "dmAI_SurvivorF_Eva",
        "dmAI_SurvivorF_Naomi",
        "dmAI_SurvivorF_Baty"
    };

    static string GetMale()
    {
        return classNamesMale.GetRandomElement();
    }

    static string GetFemale()
    {
        return classNamesFemale.GetRandomElement();
    }

    static string GetRandom()
    {
        return classNames.GetRandomElement();
    }

    static typename GetByName(string name)
    {
        switch (name) {
        case "dmAI_SurvivorM_Mirek":
            return dmAI_SurvivorM_Mirek;
        case "dmAI_SurvivorM_Denis":
            return dmAI_SurvivorM_Denis;
        case "dmAI_SurvivorM_Boris":
            return dmAI_SurvivorM_Boris;
        case "dmAI_SurvivorM_Cyril":
            return dmAI_SurvivorM_Cyril;
        case "dmAI_SurvivorM_Elias":
            return dmAI_SurvivorM_Elias;
        case "dmAI_SurvivorM_Francis":
            return dmAI_SurvivorM_Francis;
        case "dmAI_SurvivorM_Guo":
            return dmAI_SurvivorM_Guo;
        case "dmAI_SurvivorM_Hassan":
            return dmAI_SurvivorM_Hassan;
        case "dmAI_SurvivorM_Indar":
            return dmAI_SurvivorM_Indar;
        case "dmAI_SurvivorM_Jose":
            return dmAI_SurvivorM_Jose;
        case "dmAI_SurvivorM_Kaito":
            return dmAI_SurvivorM_Kaito;
        case "dmAI_SurvivorM_Lewis":
            return dmAI_SurvivorM_Lewis;
        case "dmAI_SurvivorM_Manua":
            return dmAI_SurvivorM_Manua;
        case "dmAI_SurvivorM_Niki":
            return dmAI_SurvivorM_Niki;
        case "dmAI_SurvivorM_Oliver":
            return dmAI_SurvivorM_Oliver;
        case "dmAI_SurvivorM_Peter":
            return dmAI_SurvivorM_Peter;
        case "dmAI_SurvivorM_Quinn":
            return dmAI_SurvivorM_Quinn;
        case "dmAI_SurvivorM_Rolf":
            return dmAI_SurvivorM_Rolf;
        case "dmAI_SurvivorM_Seth":
            return dmAI_SurvivorM_Seth;
        case "dmAI_SurvivorM_Taiki":
            return dmAI_SurvivorM_Taiki;
        case "dmAI_SurvivorF_Linda":
            return dmAI_SurvivorF_Linda;
        case "dmAI_SurvivorF_Maria":
            return dmAI_SurvivorF_Maria;
        case "dmAI_SurvivorF_Frida":
            return dmAI_SurvivorF_Frida;
        case "dmAI_SurvivorF_Gabi":
            return dmAI_SurvivorF_Gabi;
        case "dmAI_SurvivorF_Helga":
            return dmAI_SurvivorF_Helga;
        case "dmAI_SurvivorF_Irena":
            return dmAI_SurvivorF_Irena;
        case "dmAI_SurvivorF_Judy":
            return dmAI_SurvivorF_Judy;
        case "dmAI_SurvivorF_Keiko":
            return dmAI_SurvivorF_Keiko;
        case "dmAI_SurvivorF_Eva":
            return dmAI_SurvivorF_Eva;
        case "dmAI_SurvivorF_Naomi":
            return dmAI_SurvivorF_Naomi;
        case "dmAI_SurvivorF_Baty":
            return dmAI_SurvivorF_Baty;
        }
        return dmAI_SurvivorM_Denis;
    }
}
