# Prompt 241 completion handoff

## Outcome

Prompt 241 is complete. The first stock GoldSrc application command after the
Prompt 240 resource boundary is independently identified as unreliable opcode
`2`, `clc_move`. The host now decodes its protected envelope, validates the
sequence checksum, bounds and delta-decodes its `usercmd_t` records through
the existing runtime registry, and accepts it only at the verified pre-spawn
signon boundary.

The unmodified Half-Life client advanced beyond
`unreliable_post_resource_client_command`. The exact next boundary is:

`server_baseline_or_snapshot_required`

No baseline, snapshot, movement, spawn, or gameplay data is fabricated.

## Repository identity

- workspace: `D:\DEV\CPP\HL-Engine`;
- origin: `https://github.com/Pvitaly91/hl-engine.git`;
- baseline branch: `codex/goldsrc-delta-description-slice`;
- baseline commit: `b5e784ee7bd0de8a02a57590ae6088d52ab4259b`;
- task branch: `codex/goldsrc-post-resource-command-slice`;
- Half-Life SDK commit:
  `b1b5cf5892918535619b2937bb927e46cb097ba1`;
- stock client: Half-Life `1.1.2.2`, Steam build `15961492`.

The baseline commit is an ancestor of the task branch. The SDK remained
read-only and unchanged. The installed client and game data were read-only
inputs.

## Initial stock observation

Before command parsing changed, the same stock client completed challenge,
connect, netchan, serverinfo, all seven delta descriptions, the batched
resource request, and the fragmented resource manifest. The first later
application payload was:

- unreliable;
- opcode `2`;
- 14 application bytes in the first observation;
- not batched with another application command;
- repeated in fresh outer sequences while the client waited.

A one-shot observation decoder established zero-percent packet loss, two
backup commands, one new command, and three delta-compressed user commands.
The diagnostic did not retain a raw capture and was removed before the final
implementation.

## Frozen command contract

The selected identity is `clc_move`. Its bounded contract is:

1. opcode;
2. protected-body byte length;
3. sequence-derived checksum byte;
4. GoldSrc-munged protected body;
5. separately dispatched trailing application messages, if any.

After the existing GoldSrc transform is reversed, the body contains
packet-loss/voice flags, backup and new command counts, and
`backup + new` delta-compressed user commands. Counts, declared length,
packet loss, schema size, mask width, field values, bit reads, per-command
padding, and trailing bits are bounded.

The checksum is the GoldSrc sequence CRC low byte over the bounded move body.
The implementation independently reproduces the reference behavior and
rejects mismatches before command state mutation.

Each user command is decoded through the already delivered runtime
`usercmd_t` table. Unchanged fields inherit from the preceding typed command;
the first inherits from zero. Each delta starts at a byte boundary and has
zero alignment padding before the next record. The decoded representation
owns its platform-independent values and has no receive-buffer pointers.

The command does not itself carry a snapshot frame reference. Public ReHLDS
and Xash3D behavior corroborates the envelope, transform, checksum, usercmd
delta use, and pre-spawn policy. The stock observation supplies the exact
placement, counts, ordering, and client behavior.

## Dispatch, ordering, and state

The existing unreliable application seam now parses the full accepted
payload. It accepts verified NOP padding and one move container, and rejects
unknown opcodes, multiple move containers, malformed boundaries, truncation,
invalid checksums or counts, invalid schemas, nonzero padding, and unsupported
trailing data.

Transport sequence and acknowledgement state is accepted before application
decode. Therefore malformed application data does not undo valid transport
ACK processing. Pending server reliable data is retained. Duplicate and
out-of-order outer packets are suppressed by the existing netchan and never
reach application delivery.

The typed signon transition is:

`resource_manifest_acknowledged`
→ `awaiting_post_resource_command`
→ `awaiting_server_baseline_or_snapshot`.

The first valid move advances this state once. Later fresh valid moves are
decoded pre-spawn keepalives and do not advance signon again. Disconnect and
slot reuse clear the command-specific state.

No decoded angle, button, impulse, movement, or timing value is executed as
gameplay. The ordinary netchan response is the only immediate response. The
host never sets `put_in_server`, `spawned`, or `active`.

## Implementation

The new network codec and types live in:

- `include/network/goldsrc_client_move.h`;
- `src/network/goldsrc_client_move.cpp`.

The existing signon state and UDP/netchan runtime own delivery and
diagnostics. There is no new UDP transport, netchan, fragment sender, delta
registry, client registry, generic string executor, or parallel signon state
machine.

`goldsrc_client_move_tests` contains the deterministic observed golden
fixture and covers the exact semantic decode, envelope and checksum failures,
count boundaries, schema failures, truncated bitstreams, field masks,
alignment/trailing data, dispatcher composition, signon transitions, reset,
and the no-gameplay policy.

## Automated verification

The final Release build passed:

```powershell
cmake --build out/build/vs2022-win32-reference-sdk `
  --config Release --parallel
```

CTest passed 7/7:

1. `goldsrc_connectionless_tests`;
2. `goldsrc_netchan_tests`;
3. `goldsrc_fragmentation_tests`;
4. `goldsrc_signon_tests`;
5. `goldsrc_resource_manifest_tests`;
6. `goldsrc_delta_description_tests`;
7. `goldsrc_client_move_tests`.

Post-resource Proof A:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_post_resource_command_proof.ps1 `
  -ExecutablePath out/build/vs2022-win32-reference-sdk/Release/hlhost.exe `
  -GameDir out/runtime/valve-fixture -SkipServerOutput
```

Proof A completed all prior stages, decoded the exact command identity and
semantic fields, delivered it once, preserved pre-spawn state, entered
`awaiting_server_baseline_or_snapshot`, and shut down cleanly.

Proof B used the same command with `-NegativeProof`. It passed duplicate and
out-of-order suppression, truncated command, invalid count, invalid checksum,
unsupported opcode, invalid phase, pending reliable-state preservation,
slot reset, fresh session reuse, one final valid delivery, responsiveness,
inactive gameplay state, and clean shutdown.

The inherited final regression matrix passed:

| Check | Result |
|---|---|
| handshake | PASS |
| disconnected-slot reuse | PASS |
| netchan Proof A/B | PASS |
| serverinfo Proof A/B | PASS |
| resource-manifest Proof A/B | PASS |
| fragmented-manifest Proof A/B | PASS |
| Prompt 238 continuation Proof A/B | PASS |
| delta-description Proof A/B with default behavior | PASS |
| feature-off acceptance and drift | PASS |
| PowerShell parser validation | PASS |
| `git diff --check` | PASS |

The feature-off wrapper recorded `wrapper_full_run_passed=1`, no public or LAN
socket, no real-client invocation, no connection or signon path invocation,
and `normal_host_behavior_changed=0`.

All automated protocol proofs bound only to `127.0.0.1` and retained no raw
packet dumps.

## Final stock-client verification

The final run used the installed runtime delta source:

- delta source: `runtime_game_dir`;
- delta tables/fields: 7/219;
- `usercmd_t`: present;
- bootstrap fragments acknowledged: 7/7;
- resource-manifest entries: 308;
- resource-manifest fragments acknowledged: 8/8;
- final resource acknowledgement: yes;
- `clc_move` checksum: valid;
- decoded backup/new/total commands: 2/1/3;
- command deliveries: 1;
- state advances: 1;
- final signon phase: `awaiting_server_baseline_or_snapshot`;
- session count: 1;
- `put_in_server=0`;
- `spawned=0`;
- `active=0`;
- clean host shutdown: yes.

The original unidentified boundary did not recur. The installed client
executable and delta definition were unchanged by before/after hashes. Only
the exact owned process IDs and isolated temporary working directory were
cleaned.

## Artifact and Git scope

The intended commit contains only source, tests, proof scripts, compatibility
documentation, and this handoff. The pre-existing untracked `out/` tree is
excluded.

Before the authorized commit:

- build artifacts staged: no;
- SDK files staged: no;
- proprietary or installed game files staged: no;
- captures and temporary logs staged: no;
- unrelated changes reverted: no.

The commit subject is
`feat(net): decode stock GoldSrc move command envelope`. The authorized push
is a normal non-force push of the task branch. No pull request is created.
The local and remote heads are verified after the push.

## Limitations

The host has no entity or instance baselines, snapshots, packet entities,
clientdata replication, movement simulation, prediction, Game DLL
`ClientPutInServer`, spawn, weapons, damage, or gameplay in this slice.

Decoding and accepting the first post-resource client command does not mean
that movement, entity baselines, snapshots, ClientPutInServer, spawn, or
gameplay are complete.
