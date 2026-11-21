# AdCoexist

Install as many billboard ad mods as you like, and every one of them shows up in the game, even if they replace the same ad.

AdCoexist turns each ad replacer into an additional ad and puts the vanilla ad back in its original place.

This mod is inspired by [Advert Controller](https://www.nexusmods.com/cyberpunk2077/mods/18118) and the
[custom advertisements guide](https://wiki.redmodding.org/cyberpunk-2077-modding/modding-guides/textures-and-luts/custom-advertisements-1)
on the Cyberpunk 2077 Modding Wiki. The guide shows how to add an ad without replacing a vanilla one.
AdCoexist does the same for replacers, so they can be used together and managed in Advert Controller.

- Shows every installed ad replacer in the game, not only the one that wins the load order
- Puts the vanilla ad back in its original place
- Adds each replacer as a separate ad in the same districts as the original
- Works with [Advert Controller](https://www.nexusmods.com/cyberpunk2077/mods/18118), which lists the converted ads with previews
- Doesn't modify game files or mod files

## Getting Started

### Requirements

- Cyberpunk 2077 2.5
- [RED4ext](https://docs.red4ext.com/getting-started/installing-red4ext) 1.31.1+
- [TweakXL](https://github.com/psiberx/cp2077-tweak-xl) 1.12.2+

### Recommended

- [Advert Controller](https://www.nexusmods.com/cyberpunk2077/mods/18118), to see the converted ads with previews and turn them on or off

### Installation

1. Install requirements
2. Extract the release archive `AdCoexist-x.x.x.zip` into the Cyberpunk 2077 directory, or install it with Vortex
3. Install your ad replacers as usual

### How it works

On every game start, AdCoexist checks your mods for files that replace vanilla ads.
For each replacer, it creates a new ad that uses the mod's images, the same way an ad extension made with the
[custom advertisements guide](https://wiki.redmodding.org/cyberpunk-2077-modding/modding-guides/textures-and-luts/custom-advertisements-1)
would, and restores the vanilla images for the original ad.

The generated files are stored in `r6\cache\AdCoexist` and are reused until your mods change.
Ad extensions are not affected.

## Notes

- Converted ads appear in the same districts as the ad they replace. Use Advert Controller if you want to change that.
- Some billboards always show the same ad. These keep showing the vanilla ad, and the converted ads appear on the other billboards.
- Some ads are only used in specific places or quests and are not part of any district. A replacer for one of these is shown on random billboards in all districts, and the original place shows the vanilla ad.
- Advert Controller hides the static version of vanilla ads that have an animated version. This doesn't apply to converted ads, so both versions of a replacer are shown. You can turn one of them off in Advert Controller.
- Replacers that use ArchiveXL resource links, patches or extra archives are not converted.
- AdCoexist has no settings. To stop converting replacers, uninstall it.
- A game update that adds or changes ads may require an update of AdCoexist. If the bundled ad catalog doesn't match the game, a warning is written to the log.
- REDmod ad replacers haven't been tested.

### Uninstalling

Remove `red4ext\plugins\AdCoexist`. You can also delete `r6\cache\AdCoexist`, but don't delete the whole `r6\cache` directory, since it contains game files.

### Troubleshooting

The log is written to `red4ext\logs\adcoexist-<date>.log`. Please include it with bug reports, together with the list of your ad mods.

## Building from source

Requires Visual Studio Build Tools 2026 with the C++ workload, CMake 3.23+ and Git.

```powershell
git submodule update --init --recursive
powershell -ExecutionPolicy Bypass -File tools/package.ps1
```

The script builds the plugin, runs the tests and creates `dist/AdCoexist-x.x.x.zip`.

## License

AdCoexist is licensed under the [MIT License](LICENSE).
It includes code from [RED4ext.SDK](https://github.com/wopss/RED4ext.SDK) (MIT), see [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).

## Credits

- [Advert Controller](https://www.nexusmods.com/cyberpunk2077/mods/18118) and the
  [custom advertisements guide](https://wiki.redmodding.org/cyberpunk-2077-modding/modding-guides/textures-and-luts/custom-advertisements-1)
  by mana vortex and nremind, which this mod is built on. The ads AdCoexist creates follow the format described in the guide.
- [RED4ext](https://github.com/wopss/RED4ext) by WopsS
- [TweakXL](https://github.com/psiberx/cp2077-tweak-xl) and [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl) by psiberx
- [WolvenKit](https://github.com/WolvenKit/WolvenKit), whose file and TweakDB name lists are used to build the ad catalog
