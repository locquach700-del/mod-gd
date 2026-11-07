# Mod-GD AutoPilot

A Windows Geode mod for Geometry Dash **2.2081** / Geode **5.11.2**.

## Build and install

1. Open this repository's **Actions** tab.
2. Open the latest successful **Build Mod-GD** run.
3. Download the `mod-gd-windows` artifact ZIP.
4. Extract the ZIP and install the included `.geode` file with Geode's **Install from file** option.
5. Restart Geometry Dash.

## In-game menu

1. Open a level and press **Esc**.
2. Press the larger **MGD** button on the left side of the pause menu.
3. Press **Analyze Level** before using the AI pilot. The scanner counts level objects, hazards, portals, orbs, pads, triggers, slopes, and checks whether the level matches the current pilot's supported rules.
4. On a supported simple cube-only level, press **AI Auto-Play**. The pause menu is dismissed and the level restarts from the beginning.
5. **Learn Route** remains a manual recorder: it records your own press/release input while you complete a normal-mode run.
6. **Play Saved** replays a successfully saved route.
7. **Save Level** stores Mod-GD's analysis and route data locally. It does not edit or upload the original Geometry Dash level.
8. **Exit Level** uses the game's normal quit action.

## AI status and limitations

This release includes an **experimental cube-only pilot**, not a universal Geometry Dash solver. It scans the loaded level before starting, then uses a simple hazard-distance and jump-timing heuristic. It currently refuses platformer, dual, portal, orb, and pad levels because it does not yet model those mechanics. Some static cube levels may still defeat it; retry attempts vary the jump lead distance, but this is not a full physics-based pathfinder and does not guarantee completion.

The current pilot does not yet understand ship thrust, wave steering, UFO hops, ball gravity changes, robot hold height, spider teleport timing, swing momentum, orb timing, dual synchronization, moving hazards, or complex trigger effects. Noclip preview only suppresses death; it does not prove a route is valid.

The recorder/player is separate from the AI pilot. It records a human-played input route, saves it after a normal-mode clear, and can replay that trace on later attempts. Practice mode stores a last-death percentage marker, not an automatically repaired route.

## Compatibility

- Windows x64
- Geometry Dash 2.2081
- Geode 5.11.2
