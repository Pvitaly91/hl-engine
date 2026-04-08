# PROMPT-ID: HL-CL-20260408-100-first-writable-boundary-audit

## Continuation baseline

- Latest completed prompt: `HL-CL-20260408-099-canonical-manifest-continuation-baseline`
- Working branch: `codex/HL-CL-20260401-081-target-runtime-completion-state`
- Final continuation commit: `ac4f7c1fd84a364de2633f7cb2056c278bab73b2`

## Latest closed observational endpoint

- `changelevel_player_transfer_request_clear_outcome`

## Why the old helper/traceability line should stop here

`099` already closed the helper traceability flow through the canonical harvest index and the copied canonical manifest JSON artifacts with finalized `continuationBaseline`. The current checkout also already closes the engine-side observational chain through `changelevel_player_transfer_request_clear_outcome` in `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp`, and `src/app/host_application.cpp`. Extending the old helper/no-op line any further would add more bookkeeping without grounding the next runtime write, so the handoff should move back to one narrow C++ boundary instead of inventing another micro-seam family.

## First safe engine-side writable boundary candidate

The first safe writable boundary downstream of the closed observational chain is the existing target-runtime checkpoint application boundary that should become the first owner of the already prepared player `origin` and `yaw` writes.

### Exact grounded file paths and functions

- `include/game_api/hl_server_module.h`
  - `ChangeLevelPlayerTransferWriteSetSummary`
  - `ChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationStateSummary`
  - `ChangeLevelTransitionSummary::changelevel_player_transfer_write_set`
  - `ChangeLevelTransitionSummary::changelevel_player_transfer_target_runtime_checkpoint_application_state`
- `src/game_api/hl_server_module.cpp`
  - `BuildChangeLevelPlayerTransferApplyPlan`
  - `BuildChangeLevelPlayerTransferWriteSet`
  - `RefreshChangeLevelPlayerTransferWriteSet`
  - `BuildChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationState`
  - `RefreshChangeLevelPlayerTransferTargetRuntimeCheckpointApplicationState`
- `src/game_api/server_bootstrap.h`
  - `EdictStore::SetOrigin`
  - `EdictStore::SetAngles`
- `src/game_api/server_bootstrap.cpp`
  - `EdictStore::SetOrigin`
  - `EdictStore::SetAngles`

### Exact state, request, latch, and field objects involved

- `changelevel_player_transfer_apply_plan.target_player_origin`
- `changelevel_player_transfer_apply_plan.target_player_yaw`
- `changelevel_player_transfer_write_set.write_origin`
- `changelevel_player_transfer_write_set.write_yaw`
- `changelevel_player_transfer_write_set.write_count`
- `changelevel_player_transfer_write_set.runtime_write_suppressed`
- `changelevel_player_transfer_target_runtime_checkpoint_application_state.target_runtime_checkpoint_application_started`
- `changelevel_player_transfer_target_runtime_checkpoint_application_state.target_runtime_checkpoint_applied`
- `changelevel_player_transfer_target_runtime_checkpoint_application_state.target_runtime_checkpoint_application_state`
- `changelevel_player_transfer_target_runtime_checkpoint_application_state.target_runtime_checkpoint_application_state_reason`

### Why this is the first safe candidate

`BuildChangeLevelPlayerTransferApplyPlan` and `BuildChangeLevelPlayerTransferWriteSet` already narrow the future write set to two player writes only: `origin` and `yaw`, with `write_inventory = false`, `write_velocity = false`, and `write_count = 2`. `ValidateChangelevelPlayerTransferWriteSet` in `src/app/host_application.cpp` already asserts the exact prepared values and keeps `runtimeWriteSuppressed=yes`. The adjacent deferred apply chain already narrows execution timing to `post-target-bootstrap-pre-player-resume`, and the checkpoint chain already narrows the target runtime checkpoint to `serveractivate-complete`. The checkout already contains real low-level write primitives in `EdictStore::SetOrigin` and `EdictStore::SetAngles`. That makes the checkpoint application boundary the first place where an existing, validated, and already-suppressed write set can be turned into a guarded real write without widening lifecycle ownership or revisiting the earlier request-clear path.

## Nearby alternatives considered and rejected

1. `src/game_api/hl_server_module.cpp::ConsumePendingChangeLevelRequest` and the request-clear family were rejected because they own source-side pending-request and handoff bookkeeping. Touching them would risk altering the already-proven latch-only and stop-path semantics instead of applying the target-runtime player write.
2. The downstream `player_placement`, `player_attachment`, and `player_control_handoff` families were rejected because they sit after `target_runtime_checkpoint_applied`. Starting there would skip the already-grounded write-set boundary and widen the scope beyond the first write.
3. A direct raw call to `EdictStore::SetOrigin` and `EdictStore::SetAngles` outside the checkpoint application owner was rejected because it would bypass the existing deferred apply and checkpoint guard chain and would create an untracked runtime write.

## Guardrails for the future code step

- Keep `changelevel_player_transfer_request_clear_outcome` and the full request-clear family behavior unchanged.
- Keep helper behavior from `093` through `099` unchanged, including `RunLabelPrefix`, `PromptId`, `FinalizeHarvestIndexOnly`, harvest index emission, copied manifest continuation baseline propagation, manifest filenames, and `codex_run_identity`.
- Do not introduce map load, BSP switch, `ServerDeactivate`, `ParmsChangeLevel`, lifecycle widening, entity transfer, inventory writes, velocity writes, client/runtime/render/network widening, or helper workflow refactors.
- Restrict the future write to the already prepared target player `origin` and `yaw` only, and only behind the existing deferred apply plus checkpoint application guard chain.
- Preserve the proven A/B contract, especially the stop-path behavior with `completed=723`, `stopped_early=yes`, and `any_seh=no`.

## Recommended next C++ follow-up step

Narrowly wire the existing `changelevel_player_transfer_write_set` into the existing target-runtime checkpoint application owner so that, once the deferred apply checkpoint is satisfied, only the already prepared player `origin` and `yaw` writes are applied through `EdictStore::SetOrigin` and `EdictStore::SetAngles`, while inventory, velocity, request-clear behavior, map load, placement, attachment, and control handoff remain unchanged.

## Recommended next prompt slug

- `HL-CL-20260408-101-target-runtime-checkpoint-origin-yaw-apply`

## Risks, unknowns, and blockers before the future write step

- The future step still needs an exact grounded decision for which target-runtime player edict is the owner of the first apply at `serveractivate-complete`.
- Host-side validation currently expects the checkpoint application state to remain `not-applied`; the follow-up step must update those expectations coherently and only where the new guarded write is meant to surface.
- The checkpoint application boundary currently inherits a broad upstream gate chain. The future step must keep that owner narrow and avoid accidentally waking downstream placement or attachment work in the same patch.

## Final recommendation

`proceed with the recommended next C++ step`
