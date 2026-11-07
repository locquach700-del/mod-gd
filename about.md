# Mod-GD AutoPilot

Mod-GD is an experimental Geode mod for Geometry Dash 2.2081 on Windows.

## First build

- A launcher button appears on the pause screen (open it with Esc).
- The Mod-GD panel contains a Preview Noclip switch.
- The panel also contains an Auto-Play Assist switch.
- A small status label appears while either mode is enabled.

## Important limitation

The first build implements preview/no-death hooks only. The Auto-Play Assist switch currently prevents collision-based deaths; it does not yet plan or send jump, hold, release, ship, wave, orb, or spider inputs. It is not a fully autonomous level solver. A real solver needs mode-aware physics, object/trigger parsing, and playback tests across different level patterns.

Use these options for local testing and level exploration. Do not treat runs completed with no-death enabled as legitimate leaderboard records.
