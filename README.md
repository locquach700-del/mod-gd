# Mod-GD AutoPilot

A Windows Geode mod project targeting Geometry Dash **2.2081** and Geode **5.11.2**.

## Build

GitHub Actions builds the Windows `.geode` package when files are pushed to `main`, or when **Build Mod-GD** is started manually.

1. Open the repository's **Actions** tab.
2. Open the **Build Mod-GD** workflow.
3. Download the `mod-gd-windows` artifact from the latest successful run.
4. Extract the ZIP and install the `.geode` file using Geode's **Install from file** option.
5. Restart Geometry Dash.

## In-game controls

1. Start a level and press **Esc**.
2. Open **MGD** from the larger launcher on the left side of the pause screen.
3. **Preview No-Death** suppresses the death transition for a visual preview. It does not disable collision or calculate a route.
4. **Learn Route** restarts a level in normal mode and records the native press/release inputs. Finish the level normally to save the route for that level.
5. On a later attempt, **Play Saved Route** restarts from the beginning and replays the saved inputs.

## What the route player does

- Records Geometry Dash input events with simulation-step positions.
- Saves a route only after a successful normal-mode clear. Practice runs and preview runs are not stored as clean routes.
- Replays press/release events through Geometry Dash's own input handler. This lets the game apply the correct behavior for Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing, orbs, pads, portals and dual-player input.
- Saves the last Practice Mode death percentage as a marker for the current level.

## Important limitations

- This is a **learned-route recorder/player**, not an AI pathfinder. A route must first be completed manually in normal mode.
- A route playback repeats the recorded input sequence. It does not invent a different path after a failure, understand unseen levels, or guarantee a clear if level physics, version, or relevant gameplay conditions differ.
- Practice markers record where a death occurred; they do not automatically find or apply a better route.
- Preview No-Death only skips the death transition. It does not turn off hitboxes or prove that the input route is valid.
- The mod has been compiled by GitHub Actions, but in-game behavior still needs to be tested on the target Geometry Dash installation.

## Target compatibility

- Windows x64
- Geometry Dash 2.2081
- Geode 5.11.2

## References

- [Geode documentation](https://docs.geode-sdk.org/)
- [Geode build action](https://github.com/geode-sdk/build-geode-mod)
- [Geometry Dash wiki](https://geometrydash.wiki.gg/wiki/Geometry_Dash)
- [Portals](https://geometrydash.wiki.gg/wiki/Portals)
- [Transporters (pads and orbs)](https://geometrydash.wiki.gg/wiki/Transporters)
- [Triggers](https://geometrydash.wiki.gg/wiki/Triggers)
