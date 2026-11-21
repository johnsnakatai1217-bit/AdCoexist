#include <RED4ext/RED4ext.hpp>

#include "Coexist.hpp"

// ADCOEXIST_VERSION_* come from project(VERSION) in CMakeLists.txt
#define ADCOEXIST_NAME "AdCoexist"

// Entry point called by RED4ext on load and unload
RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle, RED4ext::v1::EMainReason aReason,
                                        const RED4ext::v1::Sdk* aSdk)
{
    switch (aReason)
    {
    case RED4ext::v1::EMainReason::Load:
    {
        // Written to red4ext\logs\adcoexist-<date>.log
        // aSdk->runtime is the game's product version (e.g. 2.5.0 for 2.50)
        const auto* game = aSdk->runtime;
        aSdk->logger->InfoF(aHandle, "%s %d.%d.%d loaded (game %u.%u.%u)", ADCOEXIST_NAME, ADCOEXIST_VERSION_MAJOR,
                            ADCOEXIST_VERSION_MINOR, ADCOEXIST_VERSION_PATCH, static_cast<unsigned>(game->major),
                            static_cast<unsigned>(game->minor), static_cast<unsigned>(game->patch));
        Coexist::Attach(aHandle, aSdk);
        break;
    }
    case RED4ext::v1::EMainReason::Unload:
    {
        Coexist::Detach();
        break;
    }
    }

    return true;
}

// Plugin info. The plugin isn't loaded unless runtime matches the running game version
RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
    aInfo->name = L"AdCoexist";
    aInfo->author = L"johnsnakatai1217-bit";
    aInfo->version = RED4EXT_V1_SEMVER(ADCOEXIST_VERSION_MAJOR, ADCOEXIST_VERSION_MINOR, ADCOEXIST_VERSION_PATCH);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
