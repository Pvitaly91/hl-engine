# Prompt 244 completion handoff

## Identity and scope

- Repository: `https://github.com/Pvitaly91/hl-engine.git`
- Baseline: `76ac0be8fe9a77dcf323ee3f76036cfb0019cd77`
- Task branch: `codex/goldsrc-continuous-snapshot-slice`
- SDK HEAD: `b1b5cf5892918535619b2937bb927e46cb097ba1`
- Stock client: Half-Life 1.1.2.2, Steam build `15961492`
- Previous boundary: `continuous_snapshot_cadence_required`

The SDK and installed client/game files remained read-only.  All
interoperability traffic used `127.0.0.1`.

## Implemented result

The existing snapshot/session architecture now continues after the accepted
first frame.  Its scheduler uses monotonic host network-server time, defaults
to 20 Hz (50 ms), accepts only 10--30 Hz, emits at most one due snapshot per
host network frame, and does not accumulate catch-up bursts.  Snapshot sending
has priority at the bounded host-frame boundary so high-rate client keepalive
traffic cannot exhaust the send budget and starve cadence.

The 30-bit carrier sequence is the full frame ID.  Frame ordering and distance
use explicit modular arithmetic.  A dedicated resolver searches only the
64-entry sent-frame history, accepts a unique low-eight match, treats a
duplicate idempotently, and rejects stale, future, unknown, ambiguous, and
evicted references without moving the selected base.  Both low-eight and full
identifier wrap have deterministic tests.

Every frame rebuilds the current pre-spawn clientdata and the conservative
`runtime_map_modeled_nonplayer_baselines_no_pvs` entity set.  A valid
acknowledged frame produces:

1. `svc_time`;
2. `svc_clientdata` relative to that acknowledged frame;
3. `svc_deltapacketentities` relative to the same frame.

The entity comparison is a bounded sorted merge supporting Add, Update,
Remove, and omitted Unchanged entities.  The decoder reconstructs the complete
current semantic set and validates its entity count, ordering, table kind, and
terminator.  No ACK, an evicted base, or an invalid semantic base selects the
existing full snapshot encoder instead.

If one or several unreliable frames are dropped, later frames continue to use
the newest valid acknowledged base.  Reliable signon state is independent.
Disconnect/reset clears scheduler, history, and frame-reference state; a
reused slot starts fresh.  Feature-off mode never enters the scheduler.

## Verification

The final canonical Release build passed.  CTest passed 10/10:

- connectionless;
- netchan;
- fragmentation;
- signon;
- resource manifest;
- delta descriptions;
- client move;
- world baseline;
- first snapshot;
- continuous snapshot.

The dedicated continuous test target covers cadence boundaries, bounded late
frames, rate limits, reset, valid/duplicate/stale/unknown/future/evicted and
ambiguous references, low-eight wrap, 30-bit wrap, deterministic base
selection, Add/Update/Remove/Unchanged comparison, delta encode/decode,
truncation/overflow, clientdata reconstruction, one and multiple lost frames,
history eviction, full fallback, and streaming-state transitions.

Final external Proof A used the actual built `hlhost.exe` in a separate
process and a separate loopback UDP socket.  It received 151 continuous delta
snapshots plus the first full snapshot, checked monotonic time and advancing
frame IDs, reconstructed the static 13-entity set, acknowledged multiple
frames, and ended cleanly.

Final external Proof B received 286 continuous frames after the first frame.
It deliberately dropped one and then several frames, verified deltas remained
based on the last acknowledged frame, rejected unknown and stale references,
held an ACK base until its 64-frame history entry was evicted, observed a full
fallback, crossed low-eight wrap, acknowledged the recovery frame, and reused
the existing same-process reset/fresh-session path.  Reliable state remained
intact.

The complete legacy localhost matrix passed after implementation:

| Gate | Result |
|---|---|
| handshake and disconnected slot reuse | PASS |
| netchan normal and retransmission | PASS |
| serverinfo A/B | PASS |
| resource manifest A/B | PASS |
| fragmentation A/B | PASS |
| signon continuation A/B | PASS |
| delta-description A/B | PASS |
| post-resource command | PASS |
| world baseline A/B | PASS |
| first snapshot A/B | PASS |
| feature-off acceptance/drift | PASS |
| normal host behavior changed | 0 |

The final feature-off wrapper reported `wrapper_full_run_passed=1` and
`normal_host_behavior_changed=0`.

## Stock-client result

The final stock run used the exact installed client and `c0a0`, with the host
bound only to `127.0.0.1`.  The client remained alive for the full observation
and the host exited normally after more than ten seconds.  The host sent:

- one accepted full snapshot;
- 105 later delta snapshots;
- monotonically advancing server time;
- 13 valid modeled non-player entities per semantic frame.

The client acknowledged 39 distinct server frame IDs, proving acceptance of
multiple delta snapshots rather than repetition of the first frame.  No
malformed-server-message marker was observed.  Final lifecycle state remained
`put_in_server=0`, `spawned=0`, and `active=0`.  The installed client and
runtime delta-definition hashes were unchanged.

The previous boundary is resolved.  The next observed boundary is
`player_lifecycle_or_signon_progression_required`; continuous acceptance alone
does not determine which branch should be implemented next.

## Commands

Canonical build and test:

```powershell
cmake -S . -B out/build/vs2022-win32-reference-sdk -G "Visual Studio 17 2022" -A Win32
cmake --build out/build/vs2022-win32-reference-sdk --config Release
ctest --test-dir out/build/vs2022-win32-reference-sdk -C Release --output-on-failure
```

Continuous Proof A/B:

```powershell
scripts/run_goldsrc_continuous_snapshot_proof.ps1 -ExecutablePath out/build/vs2022-win32-reference-sdk/Release/hlhost.exe -GameDir <installed-valve> -SnapshotRateHz 20
scripts/run_goldsrc_continuous_snapshot_proof.ps1 -ExecutablePath out/build/vs2022-win32-reference-sdk/Release/hlhost.exe -GameDir <installed-valve> -SnapshotCount 270 -SnapshotRateHz 20 -NegativeProof
```

## Repository hygiene and limitation

The intended change contains only repository source, headers, tests, proof
scripts, and documentation.  The pre-existing untracked `out/` remains
unstaged.  No SDK, installed client/game data, capture, generated binary, or
external temporary report belongs in the commit.

Stable continuous full and delta snapshots do not mean that Game DLL
ClientPutInServer, player spawn, PM_Move, prediction, player replication,
weapons, damage, or gameplay are complete.
