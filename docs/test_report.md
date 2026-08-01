# RDCA Combat Test Report

Run `tools/parse_combat_log.py` against the latest Unreal log to generate this report.

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
