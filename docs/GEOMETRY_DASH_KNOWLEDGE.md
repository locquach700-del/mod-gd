# Geometry Dash mechanics notes for Mod-GD

This file is a concise engineering reference for the first AutoPilot prototype. It is not a copy of the entire Geometry Dash wiki.

## Core model

Geometry Dash is a forward-moving platform game. The player's form, gravity, speed, size, camera state, and active triggers affect movement. Hazards, portals, pads, orbs, moving objects, and triggers can change what the player must do next. A solver that only looks for spikes or uses one fixed tap interval will fail on many level types.

## Player forms

- **Cube:** runs along a surface and jumps when the player presses. The exact landing position and jump timing matter.
- **Ship:** thrust is controlled by holding; releasing allows the ship to descend. A solver must reason about vertical position and velocity continuously.
- **Ball:** changes gravity when activated, normally switching between floor and ceiling movement.
- **UFO:** uses separate taps for discrete hops; it is not equivalent to a cube jump.
- **Wave:** moves diagonally while held or released, producing a sharp up/down path. Narrow corridors require precise timing.
- **Robot:** jump height depends on hold duration, so short and long presses differ.
- **Spider:** teleports to the opposite surface and may invert gravity based on the surface.
- **Swing:** introduced in Update 2.2; its direction and momentum respond to held/released input.
- **Jetpack:** a related flight form used in some 2.2 content and variants.

## Portals and player-state changes

A solver must detect at least:
- Form portals: Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing, and Jetpack where present.
- Gravity portals: normal, inverted, and gravity-reset behavior.
- Speed portals: changes to horizontal movement speed.
- Size portals: normal and mini scale.
- Dual portals: two player objects that must be considered together.
- Mirror/teleport and camera-related transitions where present.

## Pads and orbs

Pads activate on contact; orbs generally require an input while the player is within their activation region. Their effects vary by type and can redirect the player into a hazard if activated at the wrong time. A path planner must distinguish passive contact activation from optional input activation.

## Hazards and collision

The level can contain spikes, blocks, saws, slopes, hazards with custom hitboxes, and moving or animated objects. Collision prediction needs actual hitboxes and current player state, not just object-center distance. No-death mode is a bypass for exploration, not evidence that an input sequence is valid.

## Triggers

Version 2.2 adds many trigger families. Depending on the level, triggers may alter:
- object movement and follow behavior,
- opacity, tint, pulse, and visibility,
- rotation, scale, and player/gameplay direction,
- camera position, zoom, rotation, and effects,
- time warp and game speed,
- shader or visual effects,
- spawn/toggle behavior and other gameplay state.

A solver must parse trigger timing and channel/group relationships, not only static object positions.

## Autoplay architecture roadmap

1. **Read the active level:** collect object positions, types, groups, and relevant trigger data.
2. **Track player state:** current form, dual state, gravity, speed, size, camera, and x/y velocity.
3. **Predict:** use short time steps to test candidate press/release actions against the game's collision and movement rules.
4. **Learn from attempts:** store successful input traces and the state around failures; replay and refine only when the level and relevant settings match.
5. **Validate:** test in practice/editor copies and local offline sessions; do not treat noclip clears as legitimate records.

## Current code boundary

The initial Mod-GD build currently has:
- a pause-menu launcher and panel,
- a preview noclip switch,
- an experimental no-death assist switch,
- an on-screen indicator.

It does **not** yet implement the path planner, input generator, automatic retry loop, or universal level completion. Those need separate code and testing.
