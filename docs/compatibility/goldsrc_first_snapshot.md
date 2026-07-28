# First stock-compatible GoldSrc world snapshot

Status: implemented and verified contract for Prompt 243.

## Evidence and scope

- Baseline branch: `codex/goldsrc-world-baseline-slice`
- Baseline commit: `67208b2f4a688d885d74752cd08e9e7969214aac`
- Task branch: `codex/goldsrc-first-snapshot-slice`
- Half-Life SDK commit: `b1b5cf5892918535619b2937bb927e46cb097ba1`
- Stock client: Half-Life 1.1.2.2, Steam build 15961492
- Previous observed boundary: `first_snapshot_required`
- ReHLDS reference: commit
  `0124d56c3d888d922eb045775f71c6682ad1226f`, especially
  `rehlds/engine/sv_main.cpp`, `rehlds/engine/sv_user.cpp`, and
  `rehlds/engine/net.h`
- Xash3D FWGS cross-check: commit
  `009855c193c951da7068af0cb2cc14817375efbc`, especially
  `engine/server/sv_frame.c`

The pre-change stock observation reproduced the Prompt 242 boundary. After
acknowledging the baseline bundle the stock client sent `sendents` with a
valid trailing `clc_move` and then waited for the first server snapshot.
Stock composition may place the already observed exact `VModEnable 1`
command before `sendents` in the same bounded reliable payload. That one
typed composition is accepted; standalone, changed, and arbitrary commands
remain rejected. No player lifecycle callback was required to reach that
boundary.

## Frozen wire contract

The first snapshot is one normal unreliable netchan application payload in
this exact order:

1. `svc_time` (opcode 7), followed by the authoritative server-frame time as
   one little-endian IEEE-754 float.
2. `svc_clientdata` (opcode 15), bit encoded as a full record relative to the
   zero `clientdata_t` baseline. The leading previous-frame bit is zero.
   The record uses the negotiated `clientdata_t` delta table. The weapon list
   terminates with a zero presence bit.
3. `svc_packetentities` (opcode 40), followed by the full entity count as a
   little-endian unsigned short and baseline-relative full entity records.
   The bitstream terminates with the 16-bit zero entity sentinel.

Every bit-coded command is byte-aligned before the next service opcode. No
signon marker, view change, event, ping, or user message is part of this
slice.

The frame identifier is the full server-to-client netchan outgoing sequence
used to carry the snapshot. The stock client refers to it with
`clc_delta` (opcode 4) followed by the low eight bits of that sequence. The
server resolves that byte only against its bounded per-client history; it
does not treat the transport acknowledgement as a snapshot acknowledgement.

## Semantic sources

- Server time source: the authoritative host `server_state.time`, checked
  for finiteness and monotonicity.
- Clientdata source: an explicit typed pre-spawn zero/default record. The
  client slot is connected but has not entered the Game DLL, so no
  `UpdateClientData` callback is invoked and no player values are invented.
- Weapon data: not required for this pre-spawn first frame. No
  `GetWeaponData` callback is invoked; the list terminator is encoded.
- Entity source: the immutable runtime-map world baseline bundle already
  captured through the Prompt 242 engine/Game DLL baseline path.
- Entity selection policy:
  `runtime_map_modeled_nonplayer_baselines_no_pvs`.
- Visibility policy: deterministic conservative inclusion of modeled
  runtime-map non-player baseline entities. Entity 0 (world) is not a packet
  entity. Reserved player slots are excluded because no player has entered
  the Game DLL. Instanced baselines are definitions, not live entities.
- Ordering: strictly increasing entity number with no duplicates.
- Delta-table selection: `custom_entity_state_t` for beam/custom entities,
  `entity_state_player_t` for valid live player entities, and
  `entity_state_t` otherwise.
- First full entity encoding: relative to the exact entity baseline, never a
  previous client frame. No removal operations are emitted.

The fixed per-client history depth is 64 frames. It stores immutable semantic
frames only after successful encoding and atomically with successful
transmission. Deterministic oldest-first eviction applies. Unknown, future,
evicted, malformed, and duplicate references have separate outcomes.
Duplicate acknowledgement is idempotent. Disconnect/reset clears the history.

## Scheduling and loss

The first snapshot is prepared only after the baseline is acknowledged and
the exact `sendents` command is delivered. At most one snapshot send is
scheduled by that transition. It uses the ordinary unreliable channel and is
not inserted into the reliable signon queue. Loss therefore cannot mutate or
corrupt reliable signon state. Continuous cadence, resend policy, and delta
snapshots are outside this slice.

## Validation plan

- `goldsrc_snapshot_unit_tests` covers the semantic model, message order,
  round trip, delta-table selection, entity bounds/order, output capacity,
  frame history, acknowledgement outcomes, and reset.
- `scripts/run_goldsrc_first_snapshot_proof.ps1` Proof A drives the real
  `hlhost.exe` from a separate localhost UDP process through the previous
  boundary, decodes the snapshot, sends the exact frame reference, and
  verifies the acknowledged pre-spawn state.
- The same script's negative mode provides Proof B for invalid references,
  malformed semantic fixtures, overflow without truncation, unreliable loss
  isolation, history reset, and a clean fresh session.
- The prior proof suite and feature-off host run remain required regressions.

## Verified result

The final external Proof A decoded a 13-entity first snapshot, validated the
three-message order and baseline-relative entity records, sent the low-eight
frame reference, and observed `first_snapshot_acknowledged`. Proof B withheld
the first frame reference while sending a valid keepalive, verified that the
reliable signon state remained intact, rejected future and evicted references,
then accepted one valid acknowledgement and one idempotent duplicate. Unit
coverage supplies the otherwise unreachable one-frame `unknown` case plus
malformed clientdata, duplicate/order, capacity, eviction, and reset cases.

The final unmodified Half-Life 1.1.2.2 Steam build 15961492 run used `c0a0`
and localhost only. The runtime sent frame 35 at server time 0.05 as a
25-byte unreliable payload containing 13 non-player packet entities. The
client returned `clc_delta` reference 35. No malformed-message diagnostic
appeared, and the server recorded:

- `first_snapshot_acked=1`;
- `previous_boundary_resolved=true`;
- `advanced_past_previous_boundary=true`;
- `next_boundary=continuous_snapshot_cadence_required`;
- `put_in_server=0`, `spawned=0`, and `active=0`.

The stock run also exposed that a hard-coded baseline delta time base is not
valid after the client's startup delay. Runtime baseline encode and semantic
decode now receive the same authoritative `server_state.time`; deterministic
fixtures retain their default time base. A dedicated TIMEWINDOW regression
test covers this behavior.

## Deliberate limitations

This slice does not implement continuous snapshots, previous-frame entity
deltas, PVS/PAS, live-player `AddToFullPack`, removals, events, prediction,
movement, spawn, or gameplay.

Acceptance of the first world snapshot does not mean that continuous delta
snapshots, Game DLL ClientPutInServer, spawn, movement, prediction, player
replication, or gameplay are complete.
