# Mod-GD AutoPilot

A Windows Geode mod project targeting Geometry Dash **2.2081**.

## Build

GitHub Actions builds the Windows `.geode` package when files are pushed to `main`, or when **Build Mod-GD** is started manually.

1. Open the repository's **Actions** tab.
2. Open the **Build Mod-GD** workflow.
3. Select **Run workflow** when a manual build is needed.
4. Download the `mod-gd-windows` artifact from a successful run.

Install the resulting `.geode` package using the Geode in-game menu's **Install from file** option, then restart Geometry Dash.

## In-game controls

1. Start a level and press **Esc** to open the pause menu.
2. Press the **MGD** button to open the Mod-GD panel.
3. Turn on **Preview Noclip** to explore a level without collision-based deaths.
4. Turn on **Auto-Play Assist** to keep the run alive during experiments.

## Current implementation status

This is the first build of the project:
- Pause-screen launcher and in-game control panel.
- Preview Noclip toggle.
- Experimental Auto-Play Assist toggle (currently no-death only).
- On-screen status label.

A fully autonomous Geometry Dash player is a much larger project than a noclip switch. It must understand each game mode (Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing), portals, gravity and speed changes, pads/orbs, dual mode, moving objects, and 2.2 triggers. The knowledge notes in `docs/GEOMETRY_DASH_KNOWLEDGE.md` describe the groundwork. The build does not claim to solve every level yet.

## Target compatibility

- Windows x64
- Geometry Dash 2.2081
- Geode 5.11.2

Official development references:
- [Geode documentation](https://docs.geode-sdk.org/)
- [Geode build action](https://github.com/geode-sdk/build-geode-mod)
- [Geometry Dash wiki](https://geometrydash.wiki.gg/wiki/Geometry_Dash)
- [Portals](https://geometrydash.wiki.gg/wiki/Portals)
- [Transporters (pads and orbs)](https://geometrydash.wiki.gg/wiki/Transporters)
- [Triggers](https://geometrydash.wiki.gg/wiki/Triggers)
