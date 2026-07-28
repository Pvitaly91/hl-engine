# Prompt 243 completion

## Identity and scope

- Workspace: `D:\DEV\CPP\HL-Engine`
- Baseline branch: `codex/goldsrc-world-baseline-slice`
- Baseline commit: `67208b2f4a688d885d74752cd08e9e7969214aac`
- Task branch: `codex/goldsrc-first-snapshot-slice`
- Commit subject: `feat(net): add first stock GoldSrc world snapshot`
- Half-Life SDK commit:
  `b1b5cf5892918535619b2937bb927e46cb097ba1`
- Stock client: Half-Life 1.1.2.2, Steam build `15961492`
- Original boundary: `first_snapshot_required`
- Final boundary: `continuous_snapshot_cadence_required`

All interoperability traffic remained on `127.0.0.1`. The installed client,
game data, and SDK were read-only and were not staged. The user-owned
untracked `out/` tree was preserved.

## Implemented result

Prompt 243 adds a platform-independent semantic first-frame model, snapshot
builder/encoder/decoder, bounded per-client frame history, client
`clc_delta` reference decoding, an ordinary unreliable netchan carrier, and
the narrow signon/runtime integration needed for one first snapshot.

The exact application order is:

1. `svc_time`;
2. `svc_clientdata`;
3. `svc_packetentities`.

The authoritative server time comes from `server_state.time`. Pre-spawn
clientdata is an explicit typed zero/default record, and weapon data is not
required. Packet entities include the 13 modeled runtime-map non-player
baselines in strictly increasing entity order. Entity zero, the reserved
player slot, and instanced baseline definitions are excluded. Records are
encoded relative to their established baseline with the negotiated normal,
player, or custom delta table.

The snapshot frame identifier is its full server-to-client outgoing netchan
sequence. The client acknowledges it through `clc_delta` plus the low eight
bits. A 64-entry per-client history performs full-ID resolution and
deterministic oldest-first eviction. Unknown, future, evicted, stale, valid,
and duplicate outcomes remain distinct; duplicates are idempotent. Reset
clears the prepared frame and history.

The first snapshot is scheduled only by accepted `sendents`, sent once as
unreliable traffic, and stored atomically after a successful send. It never
enters the reliable signon queue and does not create or activate a player.
The strict dispatcher also handles the exact stock-observed
`VModEnable 1` + `sendents` composition while continuing to reject changed
or generic string commands.

During stock verification, the client startup delay exposed a Prompt 242
baseline TIMEWINDOW issue: the prior codec used a constant time base of 1.0.
Runtime baseline encode and semantic decode now use the same authoritative
current server time. Existing deterministic fixture callers retain the
default, and a focused TIMEWINDOW test covers delayed runtime state.

## Verification

The final Release/Win32 build completed successfully. CTest passed 9/9:

1. connectionless;
2. netchan;
3. fragmentation;
4. signon;
5. resource manifest;
6. delta descriptions;
7. client move;
8. world baseline;
9. first snapshot.

Final localhost regression results:

| Gate | Result |
|---|---|
| handshake and disconnected-slot reuse | PASS |
| netchan Proof A/B | PASS |
| serverinfo Proof A/B | PASS |
| resource-manifest Proof A/B | PASS |
| fragmented-manifest Proof A/B | PASS |
| signon-continuation Proof A/B | PASS |
| delta-description Proof A/B | PASS |
| post-resource command Proof A/B | PASS |
| world-baseline Proof A/B | PASS |
| first-snapshot Proof A/B | PASS |
| feature-off acceptance/drift | PASS |
| normal host behavior changed | 0 |

Proof A launched the actual `hlhost.exe` in a separate process, drove the
external UDP protocol through Prompt 242, decoded the exact first snapshot,
validated server time, clientdata, weapon terminator, all entity numbers,
baseline-relative records, table selection, ordering and terminator, then
sent the real client frame reference. The host recorded one acknowledgement
and remained inactive.

Proof B used the same-process negative/reset path. It withheld the first
frame reference while sending a checksum-valid keepalive, confirmed that
reliable signon state remained intact, rejected future and evicted frame
references, accepted one valid reference, and treated its duplicate
idempotently. Unit coverage verified unknown/evicted history states,
malformed clientdata, duplicate entity numbers, invalid ordering, capacity
failure without truncation, unreliable carrier isolation, and reset.

Feature-off verification recorded `wrapper_full_run_passed=1`,
`normal_host_behavior_changed=0`, and zero public/LAN sockets, real-client
invocation, connect, post-connect, or signon paths.

## Stock-client result

The final bounded run used the exact unmodified installed client and `c0a0`.
The baseline bundle was acknowledged and `sendents` was delivered. The
server then sent:

- frame id 35;
- authoritative time 0.05;
- a 25-byte unreliable application payload;
- zero-baseline clientdata;
- no weapon entries;
- 13 non-player packet entities.

The client emitted no malformed-server-message diagnostic and returned
`clc_delta` frame reference 35. The server resolved it against history,
recorded `first_snapshot_acked=1`, and advanced to
`first_snapshot_acknowledged`. Final lifecycle state was
`put_in_server=0`, `spawned=0`, and `active=0`.

The previous `first_snapshot_required` boundary is resolved. The next
observed boundary is `continuous_snapshot_cadence_required`; implementing
continuous full/delta frames is intentionally outside this task.

## Repository hygiene

The intended change set contains only repository source, headers, tests,
proof scripts, and documentation. It contains no SDK modification,
installed client or game data, captures, temporary reports, generated
binaries, or build output. `git diff --check` passed, and the pre-existing
untracked `out/` directory remains unstaged.

Acceptance of the first world snapshot does not mean that continuous delta
snapshots, Game DLL ClientPutInServer, spawn, movement, prediction, player
replication, or gameplay are complete.
