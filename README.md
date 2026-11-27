# AdCoexist

Install as many billboard ad mods as you like, and every one of them shows up in the game, even if they replace the same ad.

When several mods replace the same ad, the one that wins the load order replaces the vanilla ad as usual, and the others
show up as extra ads. Vanilla ads that you replace with mods are off by default. The optional Restore Vanilla file turns
them back on.

This mod is inspired by [Advert Controller](https://www.nexusmods.com/cyberpunk2077/mods/18118) and the
[custom advertisements guide](https://wiki.redmodding.org/cyberpunk-2077-modding/modding-guides/textures-and-luts/custom-advertisements-1)
on the Cyberpunk 2077 Modding Wiki. The guide shows how to add an ad without replacing a vanilla one.
AdCoexist does the same for replacers, so they can be used together and managed in Advert Controller.

- Shows every installed ad replacer in the game, not only the one that wins the load order
- Adds the other replacers of an ad as extra ads in the same districts as the original
- Can bring the vanilla ads back, with the optional file
- Works with [Advert Controller](https://www.nexusmods.com/cyberpunk2077/mods/18118), which lists the extra ads with previews
- Doesn't modify game files or mod files

## Getting Started

### Requirements

- Cyberpunk 2077 2.5
- [RED4ext](https://docs.red4ext.com/getting-started/installing-red4ext) 1.31.1+
- [TweakXL](https://github.com/psiberx/cp2077-tweak-xl) 1.12.2+

### Recommended

- [Advert Controller](https://www.nexusmods.com/cyberpunk2077/mods/18118), to see the extra ads with previews and turn them on or off

### Installation

1. Install requirements
2. Download AdCoexist from [Nexus Mods](https://www.nexusmods.com/cyberpunk2077/mods/39192) or [GitHub Releases](https://github.com/johnsnakatai1217-bit/AdCoexist/releases)
3. Extract the release archive `AdCoexist-x.x.x.zip` into the Cyberpunk 2077 directory, or install it with Vortex
4. Install your ad replacers as usual

### Bringing back the vanilla ads

Install the optional file `AdCoexist-RestoreVanilla-x.x.x.zip` the same way as AdCoexist. The vanilla ads then come back,
and every replacer shows up as an extra ad. To turn this off, disable or remove the optional file. The change applies on
the next game start.

The optional file only adds `red4ext\plugins\AdCoexist\restore-vanilla.txt`, so you can also create that file yourself.

### How it works

On every game start, AdCoexist checks your mods for files that replace vanilla ads.
When more than one mod replaces an ad, it creates an extra ad for each mod except the one that wins the load order,
using that mod's images, the same way an ad extension made with the
[custom advertisements guide](https://wiki.redmodding.org/cyberpunk-2077-modding/modding-guides/textures-and-luts/custom-advertisements-1)
would.

The generated files are stored in `r6\cache\AdCoexist` and are reused until your mods or the optional file change.
Ad extensions are not affected.
Mods that keep the vanilla pictures and only change how they are shown, such as quality, animation or layout fixes,
are not converted and work as they do without AdCoexist.

## Notes

- Extra ads appear in the same districts as the ad they replace. Use Advert Controller if you want to change that.
- Some billboards always show the same ad. These keep showing the replacer that wins the load order (or the vanilla ad, with the optional file), and extra ads appear on the other billboards. The Override Randomizer option of [World Advert Configurator](https://www.nexusmods.com/cyberpunk2077/mods/18214) makes these billboards random too, so extra ads can appear on them.
- Some ads are only used in specific places or quests and are not part of any district. An extra ad made from one of these is shown on random billboards in all districts.
- Advert Controller hides the static version of vanilla ads that have an animated version. This doesn't apply to extra ads, so both versions of them are shown. You can turn one of them off in Advert Controller.
- A replacer that replaces the vanilla ad is listed in Advert Controller under the vanilla ad's name.
- Replacers that use ArchiveXL resource links, patches or extra archives are not converted.
- The optional file is the only setting. To stop converting replacers, uninstall AdCoexist.
- A game update that adds or changes ads may require an update of AdCoexist. If the bundled ad catalog doesn't match the game, a warning is written to the log.
- REDmod ad replacers haven't been tested.

### Uninstalling

Remove `red4ext\plugins\AdCoexist`. You can also delete `r6\cache\AdCoexist`, but don't delete the whole `r6\cache` directory, since it contains game files.

### Troubleshooting

On every game start, AdCoexist writes a short report to `red4ext\logs\adcoexist-report.txt` (with MO2, it's in the `overwrite` folder). The report fits in one Nexus post and contains no folder paths, so you can paste it as is. Please include it with bug reports.

The full log is written to `red4ext\logs\adcoexist-<date>.log`.

## Building from source

Requires Visual Studio Build Tools 2026 with the C++ workload, CMake 3.23+ and Git.

```powershell
git submodule update --init --recursive
powershell -ExecutionPolicy Bypass -File tools/package.ps1
```

The script builds the plugin, runs the tests and creates `dist/AdCoexist-x.x.x.zip` and
`dist/AdCoexist-RestoreVanilla-x.x.x.zip`.

## License

AdCoexist is licensed under the [MIT License](LICENSE).
It includes code from [RED4ext.SDK](https://github.com/wopss/RED4ext.SDK) (MIT), see [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).

## Credits

- [Advert Controller](https://www.nexusmods.com/cyberpunk2077/mods/18118) by nremind and the
  [custom advertisements guide](https://wiki.redmodding.org/cyberpunk-2077-modding/modding-guides/textures-and-luts/custom-advertisements-1)
  by mana vortex and nremind, which this mod is built on. The ads AdCoexist creates follow the format described in the guide.
- [RED4ext](https://github.com/wopss/RED4ext) by WopsS
- [TweakXL](https://github.com/psiberx/cp2077-tweak-xl) by psiberx, used to add the generated ad records
- [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl) by psiberx, whose source code was a reference for how the game loads archives
- [WolvenKit](https://github.com/WolvenKit/WolvenKit), whose file and TweakDB name lists are used to build the ad catalog
