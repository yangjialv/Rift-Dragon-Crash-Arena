# RDCA Combat Test Report

## GAS / 3C Source Showcase Checkpoint (2026-10-16)

- Editor Development and Win64 Shipping build checks succeeded; both targets are up to date.
- Fresh full RDCA automation run: **42 succeeded, 0 warnings, 0 failed**.
- Report: `rdca/Saved/Automation/GAS3CShowcase/index.json`; log: `rdca/Saved/Logs/GAS3CShowcaseTests.log` (local generated artifacts, not committed).
- The checkpoint includes GAS, separated 3C components, native Gameplay Cues, the saved visual Boss tree/blackboard/controller, and existing Boss Blueprint integration. No new package or additional visual acceptance is claimed.
- UE 5.3 Git working-set scanning failed on quoted Chinese filenames; setting `git config --local core.quotepath false` resolved the build-tool path parsing issue. Markdown backup copies remain local and are ignored.

## Standard Visual Boss AI (2026-10-16, current)

- [x] Editor / Win64 / Development build and Win64 / Shipping compile/link; no new Cook/Stage/package.
- [x] Real saved `BT_RiftDragonCombat`, separate `BB_RiftDragonCombat`, `BP_RiftDragonAIController`; 15 graph nodes, 10 blackboard keys including engine SelfActor.
- [x] Native UE blackboard decorators observe changes; Controller publishes facts on events, not Tick. Target service is a 0.2-second fallback.
- [x] Instanced GAS tasks subscribe before activation, match execution IDs, return on completion/cancellation and unsubscribe on abort/finish.
- [x] Target propagation reaches attack backend; formal AI no longer performs per-attack GetPlayerPawn(0) lookup.
- [x] `RDCA.AI.Standard.SavedVisualTreeAndController`: asset reload, graph presence, persisted BP configuration, no instance-synced keys, real saved-tree round execution.
- [x] `RDCA.AI.Standard.BlackboardTargetActuallyAimsBarrage`: non-player-zero target, real projectile aimed at that target, immediate blackboard clearing on destruction, native abort and callback cleanup.
- [x] `RDCA.AI.Standard.SharedAssetIndependentNodeInstances`: two Bosses share one saved tree without sharing target/BB/task lifecycle; stopping one leaves the other active.
- [x] `RDCA.AI.Standard.SchemaRejectsSyncedTargetAndWrongEnum`: malformed schemas fail closed, never silently fall back to autonomous legacy attacks.
- [x] Previous 38 tests pass; invalid-asset test now expects suspension (`RDCA.AI.Tree.InvalidAssetSuspendsCombat`).
- [x] Read-only asset commandlet verification: current Boss defaults already use native RDCABossAIController, AutoAI=PlacedInWorldOrSpawned and AutoPlayer=Disabled; no Boss Blueprint or map writes this turn.
- [x] Real Arena_Level headless startup: saved tree starts, intro Hold releases, two attacks end directly in stun, missed exposure expires into the next round, process exits normally.
- [ ] User validates visual tree debugging, complete two-phase combat, animations/Cues, mouse input and Restart in the real level.
- [ ] Fresh Cook and independent packaged-game verification.

Report: `rdca/Saved/Automation/GASStandardAI/index.json`; **42 succeeded, 0 warnings, 0 failed**.
Logs: `GASStandardAITests.log`, `VerifyStandardAIAssets.log`, `StandardAIArenaSmoke.log` under `rdca/Saved/Logs`.
The smoke check uses `-NullRHI -nosound -seconds=25`; BGM retry warnings are expected without an audio device, not an audio acceptance test.

During implementation, native blackboard conditions exposed a fixture initialization issue: isolated test worlds had not cached the Blackboard on the Brain component. Startup now explicitly connects both directions after UseBlackboard. An initial schema-test failure was duplicate expected-error registration, not a gameplay regression. The latest full suite passes.
Original tuning and user assets are preserved. `/Game/AI` is configured for Cook, but no Cook has been run for this revision.
See [current design and usage](boss_ai_standard_design.md). The PDF is intentionally the requested pre-implementation architecture snapshot.

## Boss AIController and Behavior Tree (2026-10-16, previous runtime-tree iteration)

- [x] User confirmed the real Boss Blueprint works after reparenting to `RDCABossPawn`.
- [x] Editor / Win64 / Development build.
- [x] Win64 / Shipping compile and link; no Cook/Stage/package.
- [x] Native `ARDCABossAIController`, UE behavior tree, blackboard and reusable wait/select/execute/condition nodes.
- [x] `RDCA.AI.DecisionOwnershipAndExecutionIds`: one decision owner, legacy Tick cannot select under AI, selection/execution split, duplicate rejection, stale/foreign cancellation guards, explicit fallback versus suspension.
- [x] `RDCA.AI.Tree.Phase1HoldAndBlackboard`: real tree execution, opening Hold, synchronous blackboard changes, registered player target, exactly two skills and immediate stun.
- [x] `RDCA.AI.Tree.Phase2UniqueRoundDoubleWaveAndExpiry`: category-first weighting, three unique attacks, concurrent double-wave collision counted once, exposure expiry and new-round reset.
- [x] `RDCA.AI.Tree.InterruptionTargetLossAndUnpossess`: Hold, encounter stop, death, player unpossession, AI unpossession, cancellation and resumed Hold.
- [x] `RDCA.AI.Tree.DeferredStartupAndRepossess`: possession before component BeginPlay, deferred initialization, idempotent start, unchanged GAS grants, suspension and fresh tree after re-possession.
- [x] `RDCA.AI.Tree.InvalidAssetRetainsLegacyScheduler`: invalid override asset logs the expected error, never claims ownership, retains the only legacy scheduler.
- [x] Previous 32 GAS, 3C, Cue/Control and Pawn compatibility tests pass.
- [x] Original `Arena_Level` headless game-mode startup still loads the reparented Blueprint, completes intro, attacks and stun, and exits normally. Its saved Auto Possess AI is still Disabled: this checks compatibility, not enabled-AI level acceptance.
- [ ] User enables the new controller/auto-possession in the original level and verifies the complete two-phase fight, presentation and Restart.

Report: `rdca/Saved/Automation/GASBossAI/index.json`; **38 succeeded, 0 warnings, 0 failed**.
Log: `rdca/Saved/Logs/GASBossAITests.log`.
An initial ownership test used abstract `UObject` as a dummy driver and caused an engine ensure; the fixture now uses concrete, unregistered scene components. The production ownership behavior was not weakened.
The default tree is a runtime native UE tree, not a new custom scheduler and not a Content Browser graph asset. An optional authored tree can reuse the native nodes and the documented blackboard schema.
Old attack settings, random stream, round rules, GAS timing, synchronous completion-to-stun and animation/Cue compatibility remain on their existing owners.
Actual asset/level changes are left to the documented setup; this turn did not rewrite the user's modified Boss Blueprint or map.
Smoke log: `rdca/Saved/Logs/GASBossAIArenaSmoke.log`; `-NullRHI -nosound -seconds=20`, no Fatal/missing-package/ability-activation errors. Expected BGM retry warnings remain because this check has no audio device.
Setup: [Boss AI migration plan, section 9](boss_ai_behavior_tree_plan.md#9-现在具体怎么开启新-ai).

## Boss Pawn Reparenting Foundation (2026-10-15)

- [x] Editor / Win64 / Development build.
- [x] Win64 / Shipping compile and link; no new cooked package.
- [x] `RDCA.AI.BossPawn.CompositionAndSharedASC`: no native replacement root, collision, movement or duplicate ASC; no automatic possession; authored transform and weak-point collision survive initialization; UE GAS lookup returns the existing shared ASC and three Boss abilities.
- [x] `RDCA.AI.BossPawn.LegacySchedulingCompatibility`: generic Actor and new Pawn produce identical selected attacks for the same random seed in both phases; one completion per skill, immediate final-attack stun and cleanup remain intact.
- [x] Previous 30 GAS, 3C, Control and Cue tests pass.
- [x] Original Boss Blueprint and map assets are unchanged.
- [ ] User reparents `BP_BossPlaceholder` to `RDCABossPawn` and verifies the original level.
- [ ] AIController, blackboard and behavior-tree tasks: next stage, not implemented by the Pawn bridge.

Report: `rdca/Saved/Automation/GASBossPawn/index.json`; **32 succeeded, 0 warnings, 0 failed**.
Log: `rdca/Saved/Logs/GASBossPawnTests.log`.
The first run passed logic checks but reported a constructor warning from `SetReplicates(false)`; initialization now assigns `bReplicates` directly, and the full suite passed again without test warnings.
The bridge permits authored Blueprint Event Tick through `ChildCanTick`, without adding a native tick, root or movement system.
These are native-world tests, not proof that the real skeletal Boss Blueprint has been reparented or visually accepted.
Design and manual steps: [Boss AI migration plan](boss_ai_behavior_tree_plan.md).

## Gameplay Cues and Control Ownership

- [x] Editor / Win64 / Development build.
- [x] Win64 / Shipping compile and link; no resource cook or new package.
- [x] All gameplay/audio call sites now enter the native Gameplay Cue gateway; raw `RDCAAudio` remains only in the audio/Cue backend.
- [x] Motion no longer stores held input, queued world targets or buffer time, performs mouse deprojection, or switches hardware cursor state.
- [x] `RDCA.Control.AimRayProjection`: no-viewport ray projection, original ground reference, decorative collision filtering, real combat hits and invalid rays.
- [x] `RDCA.Control.BufferedWorldTargetsAndRelease`: early release cancels queued charge, one buffered dash retains its world target, reset clears pending input even without an active ability.
- [x] `RDCA.Cues.NativeDispatchAndTags`: 31 audio identifiers map to native tags, spatial/2D events pass through the UE Cue Manager exactly once, no gameplay mutation.
- [x] `RDCA.Cues.PersistentIsolationAndCleanup`: repeated start/stop, no-audio cleanup, local cue tags, stateless shared notify, independent targets, Niagara lifecycle and EndPlay.
- [x] `RDCA.Cues.DamageFeedbackAndWaveVisual`: only accepted damage triggers feedback, immunity prevents duplicates, Cue changes real wave visibility without applying damage.
- [x] Previous 25 GAS/3C tests pass.
- [x] Original `Arena_Level` headless game-mode startup: existing GameMode/Boss blueprints load, initial attacks and stun execute, timed exit succeeds.
- [x] User confirmed the current Cue/Control version works normally before the Boss Pawn migration.

Report: `rdca/Saved/Automation/GASCuesControl/index.json`; **30 succeeded, 0 warnings, 0 failed**.
Log: `rdca/Saved/Logs/GASCuesControlTests.log`.
An initial buffered-dash test waited 0.10 seconds despite the original 0.12-second minimum flight; the fixture now waits 0.15 seconds, without changing gameplay flight timing.
Looped audio and flame use OnActive/Removed; WhileActive never starts a duplicate voice/effect.
Cue data is per invocation/target, not stored on the shared notify CDO. Native cue classes are registered directly, with no new binary assets or broad asset scans.
The Cue transport is local/single-player and carries local source data; it is not a multiplayer replication implementation.
Damage, collision, Chaos fracture physics, trajectory shape and continuous state-based geometry/material updates remain independent of cosmetic dispatch.
Original-level smoke log: `rdca/Saved/Logs/GASCuesControlArenaSmoke.log`; no Fatal, missing game package, Cue registration or ability-activation errors.
The smoke command uses `-NullRHI -nosound -seconds=20`; BGM retry warnings and absent optional engine profiling DLLs are expected, not audio/visual acceptance failures.

## 3C Component Separation

- [x] Editor / Win64 / Development build.
- [x] Win64 / Shipping compile and link; no new cooked/staged package.
- [x] `RDCA.ThreeC.CompositionAndPresentation`: native components, original tuning fields, forwarded state, visual-only transforms and motion-type display.
- [x] `RDCA.ThreeC.InputBindingsAndCancellation`: idempotent binding, unrelated binding preservation, held/buffered input cleanup, camera-relative movement, disabled/dead input gates.
- [x] `RDCA.ThreeC.CameraEndpointsAndIsolation`: original near/far tuning, smoothing, target-loss fallback, no gameplay mutation.
- [x] `RDCA.ThreeC.OcclusionAndCleanup`: Actor/Component tags, previously invisible meshes, restored camera-owned visibility and phase actor hiding.
- [x] `RDCA.ThreeC.AttachedBottomAndIntent`: real attachment, head-on tangent fallback, attached movement and attached charging support.
- [x] Previous 20 GAS tests pass.
- [x] Existing `Arena_Level` startup in editor executable game mode, headless, timed exit; original Blueprint GameMode/Boss load and attack/stun progression observed.
- [x] User confirmed the current 3C version works normally before the Boss Pawn migration.
- [ ] Fresh resource cook/package and independent/low-spec machine acceptance.

Report: `rdca/Saved/Automation/GAS3C/index.json`; **25 succeeded, 0 warnings, 0 failed**.
Log: `rdca/Saved/Logs/GAS3CTests.log`.
The attachment test initially exposed a real visual-basis degeneracy on head-on contact; a tangent fallback now fixes it without changing collision or movement.
The movement fixture now advances world frame time because existing attached movement intentionally consumes world DeltaSeconds.
Native components retain Pawn-owned Blueprint tuning; mouse projection and buffered-action storage stay in the motion compatibility layer.
Attachment coverage also verifies 180-degree authored forward correction and an imported model with local-Y bottom.
Startup log: `rdca/Saved/Logs/GAS3CArenaSmoke.log`; no Fatal/missing-resource/ability-activation errors.
This smoke run used `-nosound`, so BGM retry warnings are expected; engine/editor configuration warnings were also present.
It is not a cooked package, rendered visual check or audio acceptance.

## GAS Boss Attack Lifecycle

- [x] Editor / Win64 / Development build (new test sources included with `-NoUBTMakefiles`).
- [x] Win64 / Shipping compile and link; no new cooked/staged package.
- [x] `RDCA.GAS.Boss.LaserLifecycleAndExclusivity`: automatic unique grants, mutual exclusion, warning/pause, descent, immediate stun and cleanup.
- [x] `RDCA.GAS.Boss.HoldStopDeathAndReset`: cancellation during warning/active attack, no completion count, altitude and actor cleanup, resume.
- [x] `RDCA.GAS.Boss.DoubleWaveCountsOnce`: concurrent pulses, independent ending, one skill completion, immediate stun.
- [x] `RDCA.GAS.Boss.WaveCancellationAndEndPlay`: both collision rings disabled on Hold, stop and component EndPlay.
- [x] `RDCA.GAS.Boss.AllEightBarragePatterns`: all existing patterns emit against a possessed player, complete once, and stop cleanly.
- [x] `RDCA.GAS.Boss.Phase2SelectionAndThreeAttackRound`: category-first weighting, concrete-attack uniqueness, third completion enters stun directly.
- [x] Previous 14 health, dash and charged-action tests pass.
- [x] User confirmed the original level's current combat version works normally before the 3C separation.

Report: `rdca/Saved/Automation/GASBoss/index.json`; **20 succeeded, 0 warnings, 0 failed**.
Log: `rdca/Saved/Logs/GASBossTests.log`.
Initial barrage test failures were fixture errors: predictive/homing require a player, and an isolated test world's controller must be registered explicitly.
The fixture now creates and registers a possessed player; gameplay targeting behavior was not weakened to make tests pass.
These headless tests do not verify rendered Niagara, materials, skeletal animation or actual mouse input.

## GAS Charged Jump and Anchor Crash

- [x] Editor / Win64 / Development build.
- [x] Win64 / Shipping compile and link (no new cooked package).
- [x] `RDCA.GAS.Crash.ShortClickAndChargedArc`: same-frame press/release, preview and actual arc, charge cap.
- [x] `RDCA.GAS.Crash.CancelDeathAndRescue`: cancelling the release task, invalid trajectory, death and rescue replacement.
- [x] `RDCA.GAS.Crash.BufferedJumpAfterDash`: held jump intent begins after dash cooldown and releases once.
- [x] `RDCA.GAS.Crash.AttachmentBossDirectAndWeakPoint`: simultaneous attached/charging state, cancel preserves support, far direct crash, exposure and single damage, rebound.
- [x] `RDCA.GAS.Crash.ArenaLandingRefresh`: real ring floor collision both prevents pending cooldown and removes an already active cooldown.
- [x] `RDCA.GAS.Crash.ZeroArcAndZeroRecovery`: explicit motion type, saved jump cooldown and zero-duration completion.
- [x] All four dash and all four health tests still pass.
- [x] User confirmed the current version works normally before the 3C separation.

Report: `rdca/Saved/Automation/GASCrash/index.json`; **14 succeeded, 0 warnings, 0 failed**.
Log: `rdca/Saved/Logs/GASCrashTests.log`.
The landing fixture starts with the same five-unit floor clearance as the real player and verifies its physical floor first.
Native tests verify world-space intents and collision; actual mouse targeting, visuals and camera remain manual checks.

## GAS Ground Dash: 2026-10-14

- [x] Editor / Win64 / Development build.
- [x] Win64 / Shipping compile and link (no new cooked package).
- [x] `RDCA.GAS.Dash.LifecycleAndBuffer`: immediate start, GAS action state, cooldown and one buffered follow-up.
- [x] `RDCA.GAS.Dash.InvalidTargetDeathAndRecovery`: invalid intent, disabled input, death, reset and rescue replacement.
- [x] `RDCA.GAS.Dash.ConfiguredCooldownAndZeroRecovery`: saved cooldown value, zero recovery and zero cooldown.
- [x] `RDCA.GAS.Dash.CollisionAndAttachment`: real swept blocking collision and attachment completing the skill.
- [x] All four GAS health tests still pass.
- [x] In-editor mouse/Anchor/Boss/3C acceptance after this dash migration (user confirmed).

Report: `rdca/Saved/Automation/GASDash/index.json`; **8 succeeded, 0 warnings, 0 failed**.
Log: `rdca/Saved/Logs/GASDashTests.log`.
The native tests use world-space input intents; actual mouse projection, effects and camera feel require manual testing.

## GAS Foundation: 2026-10-14

Branch: `refactor/gas-3c`; pre-refactor checkpoint: `ed60eab`.

- [x] UE 5.3 Editor / Win64 / Development build.
- [x] UE 5.3 Win64 / Shipping compile and link (not a new cooked/staged package).
- [x] `RDCA.GAS.Health.DamageDeathReset`.
- [x] `RDCA.GAS.Health.ExistingComponentCompatibility`.
- [x] `RDCA.GAS.Health.ImmunityAndExpiration`.
- [x] `RDCA.GAS.Health.ReentrantDamage`.
- [x] Original Arena_Level manual combat/3C regression (user confirmed normal).
- [ ] Packaged-game validation after the full refactor.

Automation report: `rdca/Saved/Automation/GASFoundation/index.json`.
Final result: **4 succeeded, 0 warnings, 0 failed**.
The expiry test advances an isolated game world across engine frames; it does not rely on wall-clock sleeps.
These tests cover native rules and adapter compatibility, not the complete level or subjective control/camera feel.
Migration scope and manual checks: [GAS and 3C refactor plan](gas_3c_refactor_plan.md).

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
- [ ] Player and Boss both start with 5 HP.
- [ ] Phase 1 round one is Legacy Aimed Volley, then one Shockwave.
- [ ] Phase 1 round two starts with Predictive Volley or Gap Wall, then Sweep Laser.
- [ ] Missing a Phase 1 round-two weak-point window swaps its first barrage option when possible.
- [ ] Curved, Homing, Rotating Gap, Double Spiral, and Legacy Dense Fan never enter the formal Phase 1 schedule.
- [ ] AimedVolley and Laser keep the target selected at the start of their warning.
- [ ] Moving during the warning does not retarget the attack.
- [ ] The weak point is exposed after two completed attacks, not after every attack.
- [ ] The weak-point stun/exposure window lasts 6.0 seconds in Phase 1 and 4.5 seconds in Phase 2 with the default 2.0 multiplier.
- [ ] Existing Shockwave, projectile, Laser visual, damage, and cleanup still work.
- [ ] Holding crash during cooldown starts aiming on the first available frame.
- [ ] Releasing crash before cooldown completes cancels the buffered request.
- [ ] A dash pressed shortly before availability executes once and is not repeated.
- [ ] One effective crash consumes the current weak-point window, preventing multiple HP losses in one exposure.
- [ ] The final attack of every round enters Weak Point Exposed immediately; no general Recovery delay plays between the attack montage and stun montage.
- [ ] A left-click launch becomes Ready immediately after landing on Arena Floor Collision, while right-click ground dash still observes Ground Dash Cooldown.
- [ ] Anchors are blue-purple before the phase sphere reaches them and green afterwards; Phase 1 projectiles are blue and Phase 2 projectiles are red.
- [ ] Logs contain round, step, player spatial state, previous attack, selected attack, barrage pattern, and seed.

## Manual Test: Boss Phase 2 Combos

- [ ] Reducing Boss HP from 4 to 3 starts the Phase 2 arena transition immediately.
- [ ] The first Phase 2 round executes Shockwave, a configurable gap, then a second fully warned Shockwave.
- [ ] Each Phase 2 Shockwave resets its own one-hit damage guard and can damage the player independently.
- [ ] The second Phase 2 round executes Rotating Gap or Legacy Dense Fan, then Sweep Laser.
- [ ] The third Phase 2 round executes Curved or Limited Homing, then Double Spiral.
- [ ] Missing a weak-point window repeats the same HP round without advancing Boss health.
- [ ] Both attacks in a combo use the target locked when the combo was selected.
- [ ] The `0.8` second double-Shockwave gap plus the second warning leaves a valid jump-input opportunity.
- [ ] The `0.65` second barrage-combo gap leaves one clear movement-input opportunity.
- [ ] Completing the second attack always exposes the weak point.
- [ ] Phase 2 weak-point exposure lasts approximately `4.5` seconds with the default `2.0` stun multiplier.
- [ ] Boss requires three Phase 2 weak-point hits before reaching zero HP.
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
- [ ] Stun loops for the full weak-point exposure window; it never falls back to FlyIdle early.
- [ ] Death interrupts active Montages, plays Death, and never returns to FlyIdle.
- [ ] Missing optional Montages do not block the combat state machine.
- [ ] A looping Ground Idle plays for three seconds before Intro starts.
- [ ] IntroTakeoff fires once when Ground Idle transitions into Intro and triggers only the intro platform fracture.
- [ ] When Ground Idle is unconfigured, Intro may instead use `Intro Ground Hold Duration` as a fallback.
- [ ] The centre fracture is not pre-placed, inherits the Cyber centre material, and appears only at IntroTakeoff.
- [ ] During Intro the player cannot move, aim, jump, or dash; `IntroFinished` releases input exactly once.
- [ ] Intro completion spawns the configured nine-Anchor layout after the centre platform has fractured.

## Manual Test: Arena Phase Transition

- [ ] `ArenaPhaseController` finds one complete Cyber/Source pair for `PhaseMap_RingFloor` and each pillar tag.
- [ ] Phase 1 starts with only Cyber Rift actors visible; both visual ring meshes remain collision-free because Floor Disc owns gameplay collision.
- [ ] When the Boss first enters Phase 2, combat pauses for `ExpansionDuration` and the expansion begins at `ArenaPhaseOrigin`.
- [ ] Each `PhaseMap_PillarXX` pair switches only when the expansion radius reaches its placement distance.
- [ ] The visual ring swap never changes collision; the player remains supported by the dedicated Floor Disc.
- [ ] At the end, all Source Code Void actors are visible, all Cyber Rift actors are hidden, and combat resumes once.
- [ ] `PhaseMap_Sky` pairs `cyber_sky` and `code_sky`: only Cyber is visible at startup, and the pair switches once at `SkySwitchProgress` even when the material sphere mask is enabled.
- [ ] With `bUseMaterialSphereMask` disabled, the object-level transition is correct and needs no material setup.
- [ ] With a configured `MPC_ArenaPhase` and matching environment materials, the visible boundary is a sphere centred on `ArenaPhaseOrigin`.

## Manual Test: Arena Bounds and Floor Proxy

- [ ] `Arena Combat Bounds` and the separate `Arena Floor Collision` Actor are placed at the ring centre and at the visible floor height.
- [ ] Walking, dashing, and a maximum-distance crash stop at the invisible outer wall.
- [ ] Boss projectiles and the sweep laser are not stopped by the outer wall.
- [ ] The invisible Floor Disc supports the arena without enabling collision on either high-triangle visual ring.
- [ ] The visible Inner Boundary disc accurately marks the circular low air wall at the Boss centre.
- [ ] Ground movement is blocked by the inner air wall, while a high crash can pass over it to reach the Boss.
- [ ] While attached, aiming left-click at Boss body or weak point changes the cursor to a crosshair and previews a red straight line; releasing launches a fast direct Boss crash.
- [ ] While attached, aiming anywhere other than the Boss retains the normal drag parabola and cursor.
- [ ] Enabling `Use Arena Random Spawns` on Anchor Spawn Manager creates every Anchor between the configured inner and outer clearances.
- [ ] Randomly generated Anchors preserve emergence, overload destruction, and delayed replacement behavior.
- [ ] `Minimum Anchor Spacing` prevents two simultaneously active Anchors from occupying the same tactical position.
- [ ] Across several runs, Anchor positions occupy different parts of the ring without forming a regular array; each later Anchor preferentially fills the largest gap between active Anchors.
- [ ] When no active Anchor is within `Guaranteed Anchor Distance` of the player, the next replacement appears within that distance whenever a valid point exists.
