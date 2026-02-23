#include "InstantDefuse.h"

#include "CBaseGrenade.h"

InstantDefuse InstantDefuse;

IVEngineServer2* engine = nullptr;
CGlobalVars* gpGlobals = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

IUtilsApi* utils;
IPlayersApi* players_api;

// VAR`s
DefuseOptions DefuseOptions;
DefuseConfig DefuseConfig;

PLUGIN_EXPOSE(InstantDefuse,InstantDefuse);

static bool pluginLoaded = false;
bool debug_mode;
map<string, string> phrases;
string prefix;
bool HaveAliveTerrorists();
bool CheckTimeBeforeDetonate(bool hasKit);
bool MolotovCloseToSite();

void dbgmsg(const char* text) {
    if (!debug_mode) return;
    META_CONPRINTF("%s | %s\n",g_PLAPI->GetLogTag(),text);
    utils->PrintToChatAll("%s | %s",g_PLAPI->GetLogTag(),text);
}
void SendToAllPrefix(const char* text) {
    utils->PrintToChatAll("%s %s",prefix.c_str(),text);
}
void LoadConfig() {
    KeyValues* config = new KeyValues("Config");
    const char* path = "addons/configs/InstantDefuse/config.ini";
    if (!config->LoadFromFile(g_pFullFileSystem, path)) {
        utils->ErrorLog("%s Failed to load: %s",g_PLAPI->GetLogTag(), path);
        delete config;
        return;
    }
    DefuseConfig.HaveTerroristAliveEnabled = config->GetBool("TerroristAlive", true);
    DefuseConfig.HEGrenadeThrownEnabled = config->GetBool("ThrownHEGrenade", true);
    DefuseConfig.MolotovOnPlantEnabled = config->GetBool("MolotovOnPlant", true);
    debug_mode = config->GetBool("debug_mode", false);
    prefix = config->GetString("prefix","");

    delete config;
}

void LoadTranslations() {
    phrases.clear();
    KeyValues* g_kvPhrases = new KeyValues("Phrases");
    const char *pszPath = "addons/translations/InstantDefuse.phrases.txt";

    if (!g_kvPhrases->LoadFromFile(g_pFullFileSystem, pszPath))
    {
        utils->ErrorLog("%s Failed to load %s", g_PLAPI->GetLogTag(), pszPath);
        return;
    }

    const char* language = utils->GetLanguage();

    for (KeyValues *pKey = g_kvPhrases->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey()) {
        phrases[string(pKey->GetName())] = string(pKey->GetString(language));
    }
    delete g_kvPhrases;

}

const char* GetTranslation(const char* key) {
    return phrases[string(key)].c_str();
}

void InstantDefuseBomb(int iSlot, bool haveKits) {
    if (!DefuseOptions.PlayerDefusing) return;
    if (!CheckTimeBeforeDetonate(haveKits)) {
        SendToAllPrefix(GetTranslation("NotEnoughTime"));
        return;
    }
    if (HaveAliveTerrorists()) {
        SendToAllPrefix(GetTranslation("HaveAliveTerrorists"));
        return;
    }
    if (DefuseOptions.GrenadeThrown > 0) {
        SendToAllPrefix(GetTranslation("HaveHEGrenadeThrown"));
        return;
    }
    if (MolotovCloseToSite()) {
        SendToAllPrefix(GetTranslation("MolotovClose"));
        return;
    }
    utils->NextFrame([iSlot]() {
        if (!DefuseOptions.PlayerDefusing) return;
        CCSPlayerController* player = CCSPlayerController::FromSlot(iSlot);
        if (!player) return;
        CPlantedC4* pBomb = (CPlantedC4*)UTIL_FindEntityByClassname("planted_c4");
        dbgmsg("Got c4 entity, Insta Defusing.");
        if (!pBomb) return;
        auto pPawn = player->GetPlayerPawn();
        if (!pPawn) return;
        SendToAllPrefix(GetTranslation("InstantDefuse"));
        pBomb->m_flDefuseCountDown().SetTime(gpGlobals->curtime);
        pPawn->m_iProgressBarDuration() = 0;
    });
}

bool CheckTimeBeforeDetonate(bool hasKit) {
    float minTime = 10.0;
    if (hasKit) minTime = 5.0;
    CPlantedC4* pBomb = (CPlantedC4*)UTIL_FindEntityByClassname("planted_c4");
    dbgmsg("Got c4 entity, checking if enough time to defuse.");
    if (!pBomb) return false;
    auto BlowTime =  pBomb->m_flC4Blow().GetTime() - gpGlobals->curtime;
    if ((pBomb->m_flC4Blow().GetTime() - gpGlobals->curtime) <= minTime) {
        dbgmsg("Not enough time, initial explode");
        pBomb->m_flC4Blow().SetTime(0);
        return false;
    }
    dbgmsg("Enough time to defuse, continue.");
    return true;
}

void OnBombPlanted(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    if (!DefuseConfig.MolotovOnPlantEnabled) return;
    int iSlot = pEvent->GetInt("userid");
    CCSPlayerController* player = CCSPlayerController::FromSlot(iSlot);
    if (!player) return;
    auto pPawn = player->GetPlayerPawn();
    DefuseOptions.bombLocation = pPawn->GetAbsOrigin();
    if (!debug_mode) return;
    char dbg[256];
    g_SMAPI->Format(dbg,sizeof(dbg),"Bomb planted on: %.1f, %.1f, %.1f",DefuseOptions.bombLocation.x,DefuseOptions.bombLocation.y,DefuseOptions.bombLocation.z);
    dbgmsg(dbg);
}
void OnBombDefused(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    dbgmsg("Bomb defused, setting bomblocation on 0,0,0");
    DefuseOptions.bombLocation = Vector(0,0,0);
}
void OnBombExploded(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    dbgmsg("Bomb defused, setting bomblocation on 0,0,0");
    DefuseOptions.bombLocation = Vector(0,0,0);
}

void OnBombBeginDefuse(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    DefuseOptions.PlayerDefusing = true;
    InstantDefuseBomb(pEvent->GetInt("userid"),pEvent->GetBool("haskit"));
}
void OnBombAbortDefuse(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    DefuseOptions.PlayerDefusing = false;
}

void OnGrenadeThrown(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    if (!DefuseConfig.HEGrenadeThrownEnabled) return;
    int iSlot = pEvent->GetInt("userid");
    string grenadeName = pEvent->GetString("weapon");
    CCSPlayerController* player = CCSPlayerController::FromSlot(iSlot);
    if (!player) return;
    auto pPawn = player->GetPlayerPawn();
    if (debug_mode) {
        char dbg[256];
        g_SMAPI->Format(dbg,sizeof(dbg),"Grenade name: %s | Expecting for grenade.",grenadeName.c_str());
        dbgmsg(dbg);
    }
    if (grenadeName == "hegrenade" || grenadeName == "incgrenade" || grenadeName == "molotov") {
        char dbg[256];
        g_SMAPI->Format(dbg,sizeof(dbg),"Grenade name = %s, added to GrenadeThrown.",grenadeName.c_str());
        dbgmsg(dbg);
        DefuseOptions.GrenadeThrown++;
    }
    else return;
}

void OnHEGrenadeDetonate(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    if (!DefuseConfig.HEGrenadeThrownEnabled) return;
    CEntityIndex index = (CEntityIndex)pEvent->GetInt("entityid");
    int iSlot = pEvent->GetInt("userid");
    CEntityInstance* grenade = g_pEntitySystem->GetEntityInstance(index);
    if (!grenade) return;
    CCSPlayerController* player = CCSPlayerController::FromSlot(iSlot);
    if (!player) return;
    auto pPawn = player->GetPlayerPawn();
    if (debug_mode) {
        char dbg[256];
        g_SMAPI->Format(dbg,sizeof(dbg),"Grenade classname: %s | Expecting for hegrenade_projectile.",grenade->GetClassname());
        dbgmsg(dbg);
    }
    if (strcmp(grenade->GetClassname(),"hegrenade_projectile") != 0) return;
    dbgmsg("Exploded HE found. Erasing 1 from GrenadeThrown ");
    if (DefuseOptions.GrenadeThrown == 0) {
        return;
    } DefuseOptions.GrenadeThrown--;
}

void OnInfernoStartburn(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    if (!DefuseConfig.MolotovOnPlantEnabled) return;
    CEntityIndex infIndex = pEvent->GetInt("entityid");
    Vector molLocation = Vector(pEvent->GetFloat("x"),pEvent->GetFloat("y"),pEvent->GetFloat("z"));
    if (DefuseOptions.GrenadeThrown == 0) {
        return;
    } DefuseOptions.GrenadeThrown--;
    dbgmsg("Exploded Molotov found. Erasing 1 from GrenadeThrown ");
    DefuseOptions.molotovLocation.push_back(molLocation);
    dbgmsg("Molotov Startburn, pushed Vector of this molotov");
    if (debug_mode) {
        char dbg[256];
        g_SMAPI->Format(dbg, sizeof(dbg),"Molotov landed on: %.1f, %.1f, %.1f",molLocation.x,molLocation.y,molLocation.z);
        dbgmsg(dbg);
    }
}

void OnInfernoExpired(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    DefuseOptions.molotovLocation.erase(DefuseOptions.molotovLocation.begin());
}

void OnRoundStart(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    DefuseOptions.GrenadeThrown = 0;
    DefuseOptions.bombLocation = Vector(0,0,0);
    DefuseOptions.molotovLocation.clear();
    DefuseOptions.PlayerDefusing = false;
}

bool HaveAliveTerrorists() {
    if (!DefuseConfig.HaveTerroristAliveEnabled) return false;
    for (int i = 0; i < 64; i++) {
        CCSPlayerController* player = CCSPlayerController::FromSlot(i);
        if (!player) continue;
        auto pPawn = player->GetPlayerPawn();
        if (!pPawn) continue;
        if (pPawn->IsAlive() && pPawn->GetTeam() == 2) {
            dbgmsg("Found alive terrorist, check failed, return.");
            return true;
        }
    }
    dbgmsg("Cant find alive terrosts, check passed, continue");
    return false;
}

bool MolotovCloseToSite() {
    if (!DefuseConfig.MolotovOnPlantEnabled) return false;
    for (auto& mLocation : DefuseOptions.molotovLocation) {
        auto res = mLocation.DistTo(DefuseOptions.bombLocation);
        if (debug_mode) {
            char dbg[256];
            g_SMAPI->Format(dbg,sizeof(dbg),"dist: %0.1f",res);
            dbgmsg(dbg);
        }
        if (res < 250) {
            dbgmsg("Dist < 250 - check not passed, return");
            return true;
        }
    }
    dbgmsg("Dist > 250, check passed, continue");
    return false;
}

CON_COMMAND_F(mm_instantdefuse_reload,"Reload Instant Defuse config and translations",FCVAR_SERVER_CAN_EXECUTE) {
    LoadConfig();
    LoadTranslations();
    META_CONPRINTF("%s | Reloaded successfully.\n",g_PLAPI->GetLogTag());
}

CGameEntitySystem* GameEntitySystem()
{
    return utils ? utils->GetCGameEntitySystem() : nullptr;
}

void StartupServer()
{
    g_pGameEntitySystem = GameEntitySystem();
    g_pEntitySystem = utils->GetCEntitySystem();
    gpGlobals = utils->GetCGlobalVars();
    META_CONPRINTF("%s Plugin started successfully. Marking system ready.\n", g_PLAPI->GetLogTag());
}

bool InstantDefuse::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();

    GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);

    ConVar_Register(FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);
    g_SMAPI->AddListener(this, this);

    return true;
}

void InstantDefuse::AllPluginsLoaded() {
    int ret;
    utils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing UTILS plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }
    players_api = (IPlayersApi*)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("%s | Missing UTILS plugin.",g_PLAPI->GetLogTag());
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }


    utils->StartupServer(g_PLID, StartupServer);

    utils->HookEvent(g_PLID,"bomb_planted",OnBombPlanted);
    utils->HookEvent(g_PLID,"bomb_defused",OnBombDefused);
    utils->HookEvent(g_PLID,"bomb_exploded",OnBombExploded);
    utils->HookEvent(g_PLID,"bomb_begindefuse",OnBombBeginDefuse);
    utils->HookEvent(g_PLID,"bomb_abortdefuse",OnBombAbortDefuse);
    utils->HookEvent(g_PLID,"grenade_thrown",OnGrenadeThrown);
    utils->HookEvent(g_PLID,"hegrenade_detonate",OnHEGrenadeDetonate);
    utils->HookEvent(g_PLID,"inferno_startburn",OnInfernoStartburn);
    utils->HookEvent(g_PLID,"inferno_expire",OnInfernoExpired);
    utils->HookEvent(g_PLID,"inferno_extinguish",OnInfernoExpired);
    utils->HookEvent(g_PLID,"round_start",OnRoundStart);


    LoadConfig();
    LoadTranslations();
    pluginLoaded = true;

}


bool InstantDefuse::Unload(char* error, size_t maxlen)
{
    utils->ClearAllHooks(g_PLID);
    ConVar_Unregister();
    return true;
}


const char* InstantDefuse::GetAuthor() { return "niffox"; }
const char* InstantDefuse::GetDate() { return __DATE__; }
const char* InstantDefuse::GetDescription() { return "Simple plugin for fast defuse"; }
const char* InstantDefuse::GetLicense() { return "Free"; }
const char* InstantDefuse::GetLogTag() { return "InstantDefuse"; }
const char* InstantDefuse::GetName() { return "InstantDefuse"; }
const char* InstantDefuse::GetURL() { return ""; }
const char* InstantDefuse::GetVersion() { return "1.0.2"; }
