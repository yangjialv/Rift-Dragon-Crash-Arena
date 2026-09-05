# RDCA Combat Test Report

Run `tools/parse_combat_log.py` against the latest Unreal log to generate this report.

## Manual Test: Combat Camera

- [ ] The camera remains outside the player and faces the center Boss while circling the arena.
- [ ] Both player and Boss remain inside the frame at the maximum playable arena radius.
- [ ] The Boss stays below the top safe margin instead of touching or crossing the screen edge.
- [ ] Orbit yaw crosses every arena direction without a visible snap.
- [ ] Moving toward the arena edge increases camera arm length without a sudden jump.
- [ ] Moving toward the Boss smoothly returns to the closer framing.
- [ ] The player remains visible around the full circular arena boundary.
- [ ] Ground WASD follows screen up/down/left/right as the camera rotates.
- [ ] Attached surface movement retains its existing local surface controls.
- [ ] High-arc crashes and elevated Anchor attachment remain inside the frame.
- [ ] Shockwave, AimedVolley, FanBarrage, and SweepLaser warnings remain readable at maximum zoom.

## Manual Test: Shared Anchor Overload

- [ ] Player attachment increases `OverloadAlpha` continuously.
- [ ] Warning starts after `SafeAttachmentDuration`, regardless of the safe/warning duration ratio.
- [ ] Detaching preserves the accumulated overload.
- [ ] One fan projectile adds approximately `0.20` and is consumed.
- [ ] The sweep laser adds approximately `0.35` per second while overlapping.
- [ ] Shockwave does not affect anchors.
- [ ] Player time and Boss attacks add to the same overload value.
- [ ] At `1.0`, an attached player takes damage and is force-detached.
- [ ] A managed anchor fractures and respawns at a different free point.
- [ ] Repeating the cycle does not reduce or duplicate the active anchor count.
- [ ] Releasing a valid crash jump from an attached Anchor shatters that Anchor without damaging the player.
- [ ] Starting a valid dash from an attached Anchor shatters that Anchor without damaging the player.
- [ ] Cancelling aim or attempting an invalid launch does not shatter the Anchor.
- [ ] A departure-shattered managed Anchor respawns at a different free point exactly once.

## Regression Test: Attack Detection

- [ ] Attachable Box is normalized to WorldDynamic at runtime and produces overlap events.
- [ ] The visible sweep laser, its gameplay volume, and the Anchor actually overlap in height.
- [ ] Fan projectiles only overload an Anchor when their collision spheres physically intersect it.
- [ ] A grounded player takes shockwave damage when the expanding ring reaches them.
- [ ] A player above `GroundDamageMaximumHeight` avoids shockwave damage.
- [ ] Shockwave logs report radius, player distance, and height on a successful hit.

## Manual Test: Boss Phase 1 Decision State Machine

- [ ] HUD distinguishes Preparing, Attacking, Recovery, and Weak Point Exposed.
- [ ] The same attack is never selected twice in succession.
- [ ] A grounded player increases the frequency of Shockwave and AimedVolley.
- [ ] An attached player increases the frequency of SweepLaser.
- [ ] AimedVolley and Laser keep the target selected at the start of their warning.
- [ ] Moving during the warning does not retarget the attack.
- [ ] The weak point is exposed after two completed attacks, not after every attack.
- [ ] Existing Shockwave, projectile, Laser visual, damage, and cleanup still work.
- [ ] Holding crash during cooldown starts aiming on the first available frame.
- [ ] Releasing crash before cooldown completes cancels the buffered request.
- [ ] A dash pressed shortly before availability executes once and is not repeated.
- [ ] AimedVolley fires three faster projectiles toward the locked center/left/right targets.
- [ ] Logs contain phase, player spatial state, weights, previous attack, selected attack, and seed.

## Manual Test: Boss Phase 2 Combos

- [ ] Reducing Boss HP to `1` enters Phase 2 only after the current weak-point window ends.
- [ ] A grounded player selects Ground Pressure: Shockwave, input gap, then AimedVolley.
- [ ] An attached player selects Anchor Pressure: FanBarrage, input gap, then SweepLaser.
- [ ] After the first Phase 2 combo, both combos alternate even if the player stays grounded.
- [ ] FanBarrage fires 13 projectiles across a dense 90-degree arc.
- [ ] Both attacks in a combo use the target locked when the combo was selected.
- [ ] The `0.65` second inter-attack gap leaves one clear movement-input opportunity.
- [ ] Completing the second attack always exposes the weak point.
- [ ] Phase 2 weak-point exposure lasts approximately `2.25` seconds.
- [ ] HUD displays `FAN BARRAGE` during warning and active states.
- [ ] Victory during or after a combo removes all remaining projectile and laser actors.

## Manual Test: Boss Facing and Attack Pressure

- [ ] Boss smoothly faces the player during Idle and attack preparation.
- [ ] Boss stops free tracking during Recovery, WeakPointExposed, and Dead.
- [ ] Laser warning follows the player before the final `0.25` second lock window.
- [ ] After lock, reversing direction can evade the fixed 80-degree sweep.
- [ ] Active laser never retargets the player's live position.
- [ ] AimedVolley fires five center-out precision shots.
- [ ] FanBarrage fires 21 projectiles in two interleaved angular waves.
- [ ] AimedVolley and FanBarrage descend from `ProjectileOrigin` to the locked player height, then continue horizontally instead of passing down through the arena floor.
- [ ] Shockwave expands to a maximum radius of 3600 units, with its visual edge and damage check remaining synchronized.
- [ ] Every accepted player hit broadcasts `OnDamaged` and immediately starts the shared flash, camera-shake, and hit-sound feedback path.
- [ ] A player material with a `HitFlash` scalar returns from 1 to 0 over approximately 0.12 seconds after damage.
- [ ] Missing optional hit sound or camera-shake assets do not produce errors or block damage.
- [ ] The slime keeps its last facing while idle and turns toward movement, dash, or crash travel instead of continuously facing the Boss.
- [ ] Idle, moving, charging, dashing, airborne, and attached states produce distinct smooth squash-and-stretch silhouettes without changing collision size.
- [ ] Attached presentation flattens against the current surface normal and follows face changes without rotating the camera root.
- [ ] Shockwave reaches maximum radius in approximately `2.4` seconds.
- [ ] Visible shockwave outer edge and damage timing coincide on a grounded player.
- [ ] Player HP changes in the same frame as the shockwave hit log.

## Manual Test: Boss Art-Safe Origins

- [ ] AimedVolley and FanBarrage spawn from `ProjectileOrigin`.
- [ ] SweepLaser spawns from `LaserOrigin`, with visual and DamageVolume still aligned.
- [ ] Shockwave visual and gameplay radius share `ShockwaveOrigin`.
- [ ] Boss weak-point collision and visible mesh share `WeakPointOrigin`.
- [ ] Moving an origin component changes only that system and requires no C++ edit.
- [ ] Removing any optional origin falls back to the previous placeholder behavior.

## Manual Test: Combat Camera Pillar Occlusion

- [ ] A matching `PhaseMap_Pillar*` tag in either Actor Tags or the pillar mesh's Component Tags is recognised. At the outer arena edge, the 120-degree rear sector centred on the Player's outward radial direction hides those pillars.
- [ ] Moving toward the arena centre smoothly expands the rear hidden sector toward 240 degrees.
- [ ] Pillars outside the current radial hidden sector remain visible.
- [ ] Phase-hidden Cyber/Code pillar variants remain hidden; occlusion restoration never reveals the inactive phase's pillar.
- [ ] Turning off `Hide Camera Occluder Pillars` restores every pillar hidden by this feature.

## Manual Test: Boss Animation Scheduling

- [ ] FlyIdle loops through `ABP_Dragon` while no Montage is active.
- [ ] Intro Montage holds the encounter and no attack is selected before it finishes.
- [ ] Intro completion returns to FlyIdle and releases the encounter exactly once.
- [ ] AimedVolley and FanBarrage play the Attack Montage.
- [ ] Shockwave plays the Roar Montage.
- [ ] SweepLaser enters Spell Start during warning, Loop during the active sweep, and End during recovery.
- [ ] An effective weak-point crash plays Hit and exposure plays Stun.
- [ ] Death interrupts active Montages, plays Death, and never returns to FlyIdle.
- [ ] Missing optional Montages do not block the combat state machine.
- [ ] IntroTakeoff fires once at the marked frame and triggers only the intro platform fracture.

## Manual Test: Arena Phase Transition

- [ ] `ArenaPhaseController` finds one complete Cyber/Source pair for `PhaseMap_RingFloor` and each pillar tag.
- [ ] Phase 1 starts with only Cyber Rift actors visible; the Cyber ring has collision and the Source ring has none.
- [ ] When the Boss first enters Phase 2, combat pauses for `ExpansionDuration` and the expansion begins at `ArenaPhaseOrigin`.
- [ ] Each `PhaseMap_PillarXX` pair switches only when the expansion radius reaches its placement distance.
- [ ] At `RingRevealRadius`, the Source ring collision is enabled before Cyber ring collision is disabled; the player never falls.
- [ ] At the end, all Source Code Void actors are visible, all Cyber Rift actors are hidden, and combat resumes once.
- [ ] With `bUseMaterialSphereMask` disabled, the object-level transition is correct and needs no material setup.
- [ ] With a configured `MPC_ArenaPhase` and matching environment materials, the visible boundary is a sphere centred on `ArenaPhaseOrigin`.

## Manual Test: Arena Bounds and Floor Proxy

- [ ] `Arena Combat Bounds` and the separate `Arena Floor Collision` Actor are placed at the ring centre and at the visible floor height.
- [ ] Walking, dashing, and a maximum-distance crash stop at the invisible outer wall.
- [ ] Boss projectiles and the sweep laser are not stopped by the outer wall.
- [ ] The invisible Floor Disc supports the arena without enabling collision on either high-triangle visual ring.
- [ ] The visible Inner Boundary disc accurately marks the circular low air wall at the Boss centre.
- [ ] Ground movement is blocked by the inner air wall, while a high crash can pass over it to reach the Boss.
- [ ] Enabling `Use Arena Random Spawns` on Anchor Spawn Manager creates every Anchor between the configured inner and outer clearances.
- [ ] Randomly generated Anchors preserve emergence, overload destruction, and delayed replacement behavior.
- [ ] `Minimum Anchor Spacing` prevents two simultaneously active Anchors from occupying the same tactical position.
