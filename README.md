# Fallout 2 CE Extended

Heavily upgraded fork of native-source Fallout 2 CE engine which is fully compatible with **RPU** and **Et Tu** (F2in1) for ultimate modern Fallout 1 & 2 gaming experience. Created specifically for Apple Silicon (ARM) Macs (but can be re-build for any other platform).

[INSTALL_MAC_ARM.md](INSTALL_MAC_ARM.md) — How to install on Apple Silicon Macs

## Download

[**Download the latest build DMG (macOS ARM64)**](release/Fallout-II-Community-Edition.dmg)

The file lives in the repo's `release/` folder and is refreshed on every build
release — the link always points to the newest engine build. Prefer the app bundle
instead? [release/Fallout-II-Community-Edition.app.zip](release/Fallout-II-Community-Edition.app.zip)

## Current status

| Mod | Status | Notes |
| --- | --- | --- |
| [Fallout 2 Restoration Project (RPU)](https://github.com/BGforgeNet/Fallout2_Restoration_Project) | Supported | Works, runs and fully playable. Currently going through testing to catch minor bugs. |
| [Fallout Et Tu](https://github.com/rotators/Fo1in2) | Supported | Works, runs and fully playable. Currently going through testing to catch minor bugs. |
| [sfall](https://github.com/sfall-team/sfall) | Supported | sfall 4.5.1 scripting surface required for RPU and Et Tu reimplemented natively inside CE (121+ opcodes/metarules, 43 hook types) |

Remaining problems which are currently in progress can be found in [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

## Screenshot

RPU and Et Tu running on Apple Silicon Mac

![RPU and Et Tu](screenshot_rpu_ettu.jpg)

## Docs

- [INSTALL.md](INSTALL.md) — build and install for all platforms
- [SFALL_COMPATIBILITY.md](SFALL_COMPATIBILITY.md) — sfall / RPU / Et Tu compatibility reference: what's implemented vs. upstream CE, remaining-work checklists
- [CHANGELOG.md](CHANGELOG.md) — release history

## License

The source code in this repository is available under the [Sustainable Use License](LICENSE.md).
