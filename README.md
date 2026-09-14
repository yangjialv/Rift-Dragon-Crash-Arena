# Rift Dragon Crash Arena

Unreal Engine 5.3 C++/Blueprint action Boss-battle demo. The player controls a
liquid slime across a circular arena, uses timed charged jumps and ground dashes,
attaches to temporary Anchors, and crashes into an exposed dragon weak point.

## Controls

- `W/A/S/D`: move on the arena or across an attached Anchor surface.
- Hold and release `Left Mouse`: aim and perform a charged arc jump.
- `Right Mouse`: immediate ground dash.
- While attached, aim at the Boss and release `Left Mouse`: direct weak-point crash.
- `P`: pause or resume the battle.
- `Esc`: exit the game.

## Play in editor

Open `rdca/rdca.uproject` with Unreal Engine 5.3 and run `Arena_Level` in PIE.

## Windows demo packaging

The repository is configured to start and cook `/Game/Maps/Arena_Level` with the
RDCA GameMode, combat HUD, combat audio, and BGM. Build a Development package
first and complete the checklist in `docs/packaging_checklist.md` before creating
a Shipping build for distribution.
