# hl-engine

## Changelevel Policy Regressions

Run the accepted changelevel regression pair with one command:

```powershell
cmake --build --preset vs2022-debug --target changelevel_policy_regressions
```

The target runs, in order:

1. `changelevel-latch-only-continuation`
2. `changelevel-request-consumed`

The runner fails fast on the first non-zero exit and writes grouped logs under:

`..\logs\changelevel_policy_regressions\`

If the local Half-Life `valve` directory is not at `D:\Steam\steamapps\common\Half-Life\valve`, set `HLENGINE_VALVE_DIR` before invoking the target or run the script directly:

```powershell
$env:HLENGINE_VALVE_DIR = "D:\Steam\steamapps\common\Half-Life\valve"
powershell -ExecutionPolicy Bypass -File .\scripts\run_changelevel_policy_regressions.ps1
```

## Self-Hosted CI Path

The optional Windows self-hosted workflow lives at `.github/workflows/changelevel-policy-self-hosted.yml`
and uses `scripts/run_changelevel_policy_self_hosted.ps1` as its CI wrapper entrypoint.

The workflow checks out this repository into a `host/` subdirectory and expects the self-hosted runner workspace to
already contain the surrounding HLengine root (`CMakePresets.json`, `third_party/`, `out/`) one level above it, matching
the local layout that already runs the accepted regressions.

If the self-hosted runner uses a different outer layout, set `HLENGINE_WORKSPACE_ROOT` to the HLengine workspace root
explicitly before invoking the wrapper or workflow.

That wrapper keeps the existing local `changelevel_policy_regressions` target as the execution source of truth:

```powershell
cmake --preset vs2022-win32
cmake --build --preset vs2022-debug --target changelevel_policy_regressions
```

On self-hosted runners, the CI wrapper requires `HLENGINE_VALVE_DIR` to be set explicitly and checks:

1. `dlls/hl.dll`
2. `cl_dlls/client.dll` or fallback `dlls/client.dll`
3. `maps/c0a0.bsp`
4. workspace root presence
5. `CMakePresets.json`
6. `third_party/`

The wrapper reports `failure_kind=assets` when `HLENGINE_VALVE_DIR` or the required Half-Life files are unavailable,
and `failure_kind=workspace-layout` when the outer HLengine workspace is missing or incomplete. In either case the
workflow records the environment as not runnable and skips the regression run job instead of silently treating it as a
normal pass.
