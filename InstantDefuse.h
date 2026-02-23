#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iplayerinfo.h>
#include "utlvector.h"
#include "ehandle.h"
#include <sh_vector.h>
#include <entity2/entitysystem.h>
#include "CCSPlayerController.h"
#include "CPlantedC4.h"
#include "CGameRules.h"
#include "iserver.h"
#include "include/menus.h"
#include <ctime>
#include <deque>
#include <functional>
#include <cmath>

using namespace std;

struct DefuseOptions {
    // Molotov On Plant VAR`s
    Vector bombLocation; // Bomb location based on Player location who planted
    vector<Vector> molotovLocation;

    // HE Grenade Thrown VAR`s
    int GrenadeThrown; // Is T player throw grenade

    bool PlayerDefusing;

};
struct DefuseConfig {
    bool MolotovOnPlantEnabled; // Config
    bool HaveTerroristAliveEnabled; // Config
    bool HEGrenadeThrownEnabled; // Config
};
class InstantDefuse final : public ISmmPlugin, public IMetamodListener {
public:
    bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
    bool Unload(char* error, size_t maxlen);
    void AllPluginsLoaded();
    void OnGameServerSteamAPIActivated();
private:
    const char* GetAuthor();
    const char* GetName();
    const char* GetDescription();
    const char* GetURL();
    const char* GetLicense();
    const char* GetVersion();
    const char* GetDate();
    const char* GetLogTag();
};

#endif //_INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_