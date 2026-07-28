# Prompt 242 completion handoff

## Repository identity

- Workspace: `D:\DEV\CPP\HL-Engine`
- Authorized repository: `https://github.com/Pvitaly91/hl-engine.git`
- Baseline commit:
  `6bb36d6647c9634a8f3e479e840aee8f93cb1883`
- Task branch: `codex/goldsrc-world-baseline-slice`
- Commit subject:
  `feat(net): add stock GoldSrc world baseline bootstrap`
- SDK HEAD:
  `b1b5cf5892918535619b2937bb927e46cb097ba1`
- Stock client: Half-Life 1.1.2.2, Steam build 15961492

The baseline commit remains an ancestor of the task branch. The SDK was
read-only throughout the task. The commit containing this handoff is the
single Prompt 242 commit; its SHA is recorded by the post-commit/push
verification because a commit cannot contain its own object ID.

## Result

Prompt 242 is complete. The unchanged stock client advanced from:

`server_baseline_or_snapshot_required`

through an authoritative world/entity baseline bootstrap to:

`first_snapshot_required`.

The final stock run ended successfully after the client acknowledged the
baseline bundle and delivered `sendents`. It retained
`put_in_server=0`, `spawned=0`, and `active=0`.

## Frozen compatibility contract

The requirement was classified as a combined pre-snapshot bundle:

1. `svc_spawnbaseline`;
2. the ordered world, player-slot, and modeled-entity baseline records;
3. the `0xffff` entity terminator;
4. the six-bit instanced-baseline count and any instance records;
5. final byte padding;
6. immediately adjacent `svc_signonnum 1`;
7. reliable delivery through the existing netchan;
8. client acknowledgement followed by the typed `sendents` request.

The world baseline is entity zero. Reserved player slots use
`entity_state_player_t`; ordinary modeled entities use `entity_state_t`;
custom beam entities select `custom_entity_state_t`. The runtime delta
registry supplies the exact tables and field codecs. Static entities, static
sounds, light styles, snapshots, packet entities, and clientdata are not
required in this slice. Movevars, CD track, and view entity remain in the
already verified earlier signon bootstrap.

Public protocol behavior was used only to determine observable semantics and
wire ordering. The implementation is original and does not contain
proprietary source or installed data.

## Authoritative baseline construction

Production baselines are built from the loaded BSP, engine-owned `EdictStore`,
runtime precache/model indices, and the loaded Game DLL callbacks. The
selection is deterministic:

- world entity zero;
- every reserved player slot, even while inactive;
- each remaining non-free edict with a non-zero model index;
- ascending edict index with no duplicates.

`pfnCreateBaseline` is called once for every selected entity.
`pfnCreateInstancedBaselines` is called once per immutable map bundle.
Retries and per-client retransmission reuse the cached result and do not
repeat callbacks.

The typed model enforces protocol entity limits, unique indices, required
world semantics, valid model indices, explicit table selection, bounded
instance counts, bounded payload size, and stable build/encode errors. It
serializes fields through the existing protocol-48 bit writer and delta
registry rather than copying SDK structure memory.

The production `c0a0` result in the final stock run was:

- 15 entity baselines;
- 1 world baseline;
- 1 inactive reserved player baseline;
- 13 other modeled entity baselines;
- 0 instanced baselines;
- 15 `pfnCreateBaseline` calls;
- 1 `pfnCreateInstancedBaselines` call;
- 525 encoded bytes;
- one unfragmented reliable baseline carrier.

## Runtime and signon integration

The opt-in `--goldsrc-world-baselines` path extends the existing session,
netchan, reliable sender, fragment sender, delta registry, and signon state.
It does not add a second registry, transport, fragment protocol, or signon
machine.

The bounded states are:

`awaiting_server_baseline_or_snapshot` →
`awaiting_baseline_bootstrap` →
`baseline_bootstrap_queued` →
`baseline_bootstrap_sent_awaiting_ack` →
`baseline_bootstrap_acknowledged` →
`awaiting_first_snapshot`.

Repeated moves do not rebuild or requeue the bundle. Only the current
reliable generation can advance the phase. Disconnect and slot reset clear
per-client delivery state while the immutable map bundle remains reusable.

The real client also sends a bounded normal reliable fragment after the
resource transfer. The netchan now has an opt-in incoming normal-fragment
path for the world-baseline interoperability mode. It accepts only the
existing bounded normal stream, rejects file streams, acknowledges the
reliable toggle, and does not execute reassembled content generically.

After baseline acknowledgement, the stock client composes reliable
`stringcmd:sendents` with a trailing unreliable `clc_move` in one netchan
payload. The strict signon decoder still rejects arbitrary trailing data. It
exposes the exact primary-command boundary, and the dispatcher accepts this
one composition only after the existing move decoder validates the complete
checksum and `usercmd_t` suffix. The trailing move remains a pre-spawn
keepalive and is not executed.

## Automated verification

The final Release/Win32 build used the established reference-SDK workspace:

```powershell
cmake --build out/build/vs2022-win32-reference-sdk `
  --config Release --parallel

ctest --test-dir out/build/vs2022-win32-reference-sdk `
  -C Release --output-on-failure
```

CTest passed 8/8:

1. connectionless;
2. netchan;
3. fragmentation;
4. signon;
5. resource manifest;
6. delta descriptions;
7. client move;
8. world baseline.

The deterministic world proof commands used the real installed `valve/c0a0`
map source with the repository delta fixture and exact map-tail expectations:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_world_baseline_proof.ps1 `
  -ExecutablePath out/build/vs2022-win32-reference-sdk/Release/hlhost.exe `
  -GameDir <installed-valve-directory> `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 90 `
  -ExpectedZMaximum 6300 -ExpectedCdTrack 3 -SkipServerOutput
```

Proof B used the same command with `-NegativeProof`.

Proof A passed exact bundle receipt/order, world/player/entity decoding,
acknowledgement, the single signon advance, inactive lifecycle state, and
clean shutdown.

Proof B passed byte-identical retransmission, duplicate trigger suppression,
invalid entity/world/table rejection, wrong acknowledgement rejection,
correct single advancement, reset/fresh-session reuse, responsiveness,
inactive lifecycle state, and clean shutdown.

## Final regression matrix

| Gate | Result |
|---|---|
| Release/Win32 build | PASS |
| CTest | PASS, 8/8 |
| handshake | PASS |
| disconnected-slot reuse | PASS |
| netchan Proof A/B | PASS |
| serverinfo Proof A/B | PASS |
| resource-manifest Proof A/B | PASS |
| fragmented-manifest Proof A/B | PASS |
| signon-continuation Proof A/B | PASS |
| delta-description Proof A/B | PASS |
| post-resource command Proof A/B | PASS |
| world-baseline Proof A/B | PASS |
| stock-client interoperability | PASS |
| feature-off acceptance/drift | PASS |
| PowerShell parser validation | PASS |
| `git diff --check` | PASS |

All protocol proofs bound only to `127.0.0.1`.

The feature-off wrapper recorded `wrapper_full_run_passed=1`, no public or
LAN socket, no real-client invocation, no connection/signon invocation, and
`normal_host_behavior_changed=0`.

## Stock-client evidence

The final bounded run used the same unchanged Half-Life 1.1.2.2 Steam build
15961492 executable, installed `valve` data, installed runtime `delta.lst`,
and `c0a0`. It ran on a dynamically selected localhost server port. The
client executable and `delta.lst` hashes matched before and after the run.
No unrelated `hl.exe` overlapped the accepted run, and only the owned client
PID was stopped during cleanup.

Sanitized final result:

- all Prompt 241 phases completed;
- one checksum-valid pre-spawn `clc_move` reached the old boundary;
- the runtime built and sent the 15-entity baseline bundle;
- the stock client acknowledged the baseline carrier;
- the client sent `sendents` with a validated trailing move;
- `sendents_received=1` and `sendents_delivered=1`;
- `previous_boundary_resolved=true`;
- `next_boundary=first_snapshot_required`;
- `put_in_server=0`, `spawned=0`, `active=0`;
- clean shutdown and host exit code zero.

No raw packets, installed contents, or machine-local captures are retained in
the repository.

## Limits and next task

The implementation intentionally stops at `first_snapshot_required`.
Accepting the world/entity baseline bootstrap does not mean that the first
live snapshot, packet entities, clientdata, ClientPutInServer, spawn,
movement, or gameplay are complete.

Prompt 243 should determine the smallest truthful first-snapshot/clientdata
contract. It must not infer that the client is active merely because
`sendents` was accepted.

## Repository hygiene

The intended change set contains source, headers, tests, proof scripts, and
these two documentation files. It contains no SDK change, proprietary file,
installed game data, capture, generated binary, or build artifact.

The pre-existing untracked `out/` tree remains user-owned and is not staged.
Temporary proof and stock-run directories are removed only by exact validated
paths under the system temporary directory.
