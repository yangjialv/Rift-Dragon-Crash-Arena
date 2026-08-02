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
