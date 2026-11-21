#pragma once

#include <RED4ext/RED4ext.hpp>

// Converts ad replacer mods into additional ads.
// Right after InitializeArchives, it matches the mod archives against the ad catalog,
// generates an archive and a YAML file in r6\cache\AdCoexist, and loads them.
namespace Coexist
{
bool Attach(RED4ext::v1::PluginHandle aHandle, const RED4ext::v1::Sdk* aSdk);
void Detach();
} // namespace Coexist
