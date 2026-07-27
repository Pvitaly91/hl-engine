# PROMPT 237 completion handoff

Date: 2026-07-27

## Baseline, branch, and dependency pin

- Repository: `https://github.com/Pvitaly91/hl-engine.git`
- Baseline branch: `codex/goldsrc-resource-manifest-slice`
- Baseline commit: `6ed74d8ef586275a6d6d0c59c62ba4bf560b3bc8`
- Task branch: `codex/goldsrc-netchan-fragmentation-slice`
- Half-Life SDK commit:
  `b1b5cf5892918535619b2937bb927e46cb097ba1`
- Commit subject: `feat(net): add GoldSrc reliable fragmentation slice`
- The baseline commit remains an ancestor of the task branch.
- The SDK checkout was read-only for this task and its HEAD remained unchanged.

The pre-existing untracked `out/` build/validation tree was preserved and
excluded from the source change. No generated binary, proprietary game file,
SDK file, capture, local log, or machine-specific absolute path belongs to the
Prompt 237 commit.

## Completed vertical slice

The opt-in localhost protocol-48 signon now sends one large server-to-client
reliable application payload as a bounded normal fragment stream. The first
production consumer is the encoded resource-manifest response. Payloads up to
1200 bytes retain the Prompt 236 ordinary reliable path; payloads from 1201
through 65536 bytes use the transport-generic sender.

The fragment layer is platform-independent and contains:

- an exact two-stream metadata codec;
- a fixed-capacity deterministic sender and frozen plan;
- a fixed-capacity normal-stream reassembler;
- one typed transfer phase;
- stable typed plan/process results;
- exact duplicate suppression and conflicting-duplicate rejection;
- reference-compatible current-fragment retransmission;
- a 30-second no-progress timeout;
- reset/reuse semantics and bounded diagnostics.

The runtime integration remains small. The authoritative player slot owns the
netchan; the netchan owns at most one fragment sender. The host pump stages at
most one next fragment after a valid ACK, retains its existing receive/send
budgets, and does not advance gameplay frames to drive transport.

The server does not accept fragmented client application messages. The codec
can classify their metadata, but production netchan processing rejects them
before sequence, ACK, accepted-activity, signon, or gameplay mutation.

## Selected protocol behavior

The detailed field table, byte order, flag meanings, transfer identity rules,
payload/count limits, transformation range, planning, ACK correlation,
retransmission, duplicate behavior, timeout, and reset contract are recorded
in `docs/compatibility/goldsrc_netchan_fragmentation.md`.

The selected behavior was corroborated from:

- ReHLDS commit `0124d56c3d888d922eb045775f71c6682ad1226f`,
  specifically `rehlds/engine/net_chan.cpp` and `rehlds/engine/net.h`;
- Xash3D FWGS commit `009855c193c951da7068af0cb2cc14817375efbc`,
  specifically `engine/common/net_chan.c`.

Both public implementations place bit 30 in the outgoing sequence word, write
normal and file stream descriptors after the eight-byte base header, pack the
one-based fragment index/count into the high/low 16 bits of `fragid`, transform
the complete post-header region, and resend the current reliable fragment when
the ordinary ACK advances beyond its carrier while the reliable ACK state
still differs.

There is no distinct wire transfer ID or full-transfer-size field. This
implementation uses session ownership plus one in-flight transfer, and a
caller-owned non-wire generation for reassembly identity. Full size is the
bounded sum of the unique indexed fragments.

The selected payload capacity is 1024 bytes:

```text
min(reference S2C maximum 1024,
    routeable 1400 - base header 8 - normal metadata 10)
```

The complete transfer is limited to 65536 bytes and 64 fragments. Compression,
file fragments, coexistence with another regular reliable, and multiple
simultaneous streams are intentionally not implemented.

## Transfer and signon semantics

The sender phases are `none`, `planned`, `sending`, `sent_awaiting_ack`,
`completed`, and `failed`.

Each fragment is staged as the current reliable buffer. A correct ACK for an
early fragment advances only to the next fragment. It does not report the
application payload acknowledged and does not advance signon. A wrong reliable
ACK, forged future ACK, stale ACK, or non-covering ACK cannot clear the current
fragment.

The reference resend condition emits the same current descriptor and decoded
source bytes under a later outer sequence while preserving reliable state.
Only the correct final-fragment reliable ACK reports the typed
`resource_manifest` completion and advances signon once to
`resource_manifest_acknowledged`.

At all phases in this milestone:

- `put_in_server=0`
- `spawned=0`
- `active=0`

An oversized complete response is rejected atomically. No transfer or partial
fragment is created, no manifest generation is recorded, and the cached typed
outcome is reused for a repeated request.

## Reassembler behavior

The reassembler owns one fixed assembly, at most 64 fixed fragment slots, and
one fixed 65536-byte output. It validates caller transfer generation, normal
stream type, consistent count, one-based index, packet-local offset/length,
per-fragment size, total unique size, and timeout.

Supported out-of-order indices are stored by index and concatenated only after
all advertised indices are present. An exact duplicate is idempotent and does
not increase the completion count. A changed length is an overlap conflict; a
changed byte sequence is a conflicting duplicate. Invalid input does not
partially advance output or application signon.

Timeout and explicit reset clear all fragment slots and output. Netchan timeout
also clears the current reliable buffer and leaves a typed failed phase.
Authoritative disconnect resets the full netchan. A reused slot begins without
frozen bytes, descriptors, generation, sequence history, timers, counters, or
active fragment phase.

## Changed files

- `CMakeLists.txt`
- `README.md`
- `docs/compatibility/goldsrc_netchan_fragmentation.md`
- `docs/compatibility/goldsrc_resource_manifest.md`
- `docs/handoffs/prompt_237_completion.md`
- `include/game_api/hl_server_module.h`
- `include/network/goldsrc_fragmentation.h`
- `include/network/goldsrc_netchan.h`
- `include/network/goldsrc_resource_manifest.h`
- `scripts/run_goldsrc_fragmented_manifest_proof.ps1`
- `src/game_api/goldsrc_udp_handshake_runtime.inc`
- `src/network/goldsrc_fragmentation.cpp`
- `src/network/goldsrc_netchan.cpp`
- `src/tests/fixtures/goldsrc_resource_manifest_fragmented.tsv`
- `src/tests/goldsrc_fragmentation_tests.cpp`
- `src/tests/goldsrc_netchan_tests.cpp`
- `src/tests/goldsrc_resource_manifest_tests.cpp`

Changed source/documentation/test files: 17.

## Build and unit test validation

The established local Win32 CMake workspace and pinned SDK were used:

```powershell
cmake -S . -B out/build/vs2022-win32-reference-sdk -A Win32

cmake --build out/build/vs2022-win32-reference-sdk `
  --config Release --target `
  hlhost `
  goldsrc_connectionless_tests `
  goldsrc_netchan_tests `
  goldsrc_signon_tests `
  goldsrc_resource_manifest_tests `
  goldsrc_fragmentation_tests

ctest --test-dir out/build/vs2022-win32-reference-sdk `
  -C Release --output-on-failure
```

Sanitized result: Release build PASS; full CTest PASS, 5/5.

`goldsrc_fragmentation_tests` covers:

- exact metadata and transformed-datagram golden vectors;
- truncation at every metadata boundary, byte order, flags, reserved bits,
  zero/over-limit lengths, offsets/ranges, and zeroed output;
- smallest fragmented payload, exact capacity multiples, short final fragment,
  complete source coverage, maximum transfer, one-over rejection, count and
  arithmetic overflow, deterministic plans, frozen bytes, and busy state;
- ordered and supported out-of-order assembly, exact/conflicting duplicates,
  missing first/middle/final, identity/count mismatch, offset/length/total/count
  limits, timeout, success/failure reset, and exact output;
- large/small netchan paths, wrong/future ACK stability, current-fragment
  retransmission, byte identity, final-only application completion, timeout,
  inbound-fragment rejection, disconnect/reset/reuse, and sequence wrap.

## External localhost proofs

All proof traffic was bound to `127.0.0.1`. Each proof launched the real
Release `hlhost` and used a separate external process/socket. Process ownership,
child cleanup, temporary-file cleanup, and repository non-mutation were
verified.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_fragmented_manifest_proof.ps1 `
  -ExecutablePath out/build/vs2022-win32-reference-sdk/Release/hlhost.exe `
  -GameDir out/runtime/valve-fixture `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 60 `
  -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_fragmented_manifest_proof.ps1 `
  -ExecutablePath out/build/vs2022-win32-reference-sdk/Release/hlhost.exe `
  -GameDir out/runtime/valve-fixture `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 90 `
  -NegativeProof -SkipServerOutput
```

Proof A PASS:

- completed connectionless handshake, netchan, `new`, and serverinfo ACK;
- sent the existing `sendres` request;
- received and validated three routeable fragmented datagrams;
- validated outer sequence/ACK progression, fragment indication, session-owned
  identity, index/count, offset/length, reliable state, and transformed bytes;
- reassembled the exact application payload;
- semantically decoded all 46 fixture resources in expected order;
- verified one world resource, no duplicate/missing indices, no unexpected file
  contents, and no proof padding in the manifest;
- sent every required ACK and observed exactly one final manifest completion;
- finished connected and established with no pending reliable bytes, spawn, or
  gameplay activation.

Proof B PASS:

- omitted first, middle, and final fragments from their first assembly attempt;
- induced the reference current-fragment resend condition;
- observed stable identity/reliable state and byte-identical decoded resend
  bytes;
- observed and idempotently suppressed an exact duplicate fragment;
- proved wrong reliable and forged future ACKs left the transfer pending;
- completed the application payload once and advanced signon once;
- generated a separate valid manifest above 65536 bytes, observed typed
  `payload_too_large`, and verified no partial fragment;
- executed the external platform-independent reset/reuse helper;
- verified server responsiveness and clean process/artifact cleanup.

## Regression matrix

The inherited proof commands used the same Release host, local game fixture,
loopback bind, external UDP socket, and cleanup checks.

| Check | Result |
|---|---|
| Release/Win32 build | PASS |
| Full CTest | PASS (5/5) |
| Existing handshake proof | PASS |
| Existing disconnected-slot-reuse proof | PASS |
| Existing normal netchan proof | PASS |
| Existing netchan retransmission/negative proof | PASS |
| Existing serverinfo Proof A | PASS |
| Existing serverinfo Proof B | PASS |
| Existing resource-manifest Proof A | PASS |
| Existing resource-manifest Proof B | PASS |
| Fragmented-manifest Proof A | PASS |
| Fragmented-manifest Proof B | PASS |
| Feature-off query/info acceptance gate | PASS |
| Feature-off query/info drift gate | PASS |
| Normal host behavior changed | 0 |
| PowerShell parser validation | PASS |
| `git diff --check` | PASS |

The feature-off wrapper reported:

- `wrapper_full_run_passed=1`
- `public_socket_opened=0`
- `lan_socket_opened=0`
- `real_client_binary_invoked=0`
- `connect_path_invoked=0`
- `post_connect_serverinfo_path_invoked=0`
- `signon_serverinfo_path_invoked=0`
- `normal_host_behavior_changed=0`

## Stock-client status

`stock_client_tested=no`.

No bounded, reproducible harness was available that launches an unmodified,
legally obtained Half-Life client, drives this exact localhost signon stage,
records only sanitized evidence, and proves cleanup. This does not block the
automated milestone. No stock-client interoperability claim is made.

## Known limitations and non-goals

- Only server-to-client normal reliable fragments are implemented.
- Fragmented client application messages remain unsupported.
- Only one authoritative session and one active fragmented application
  reliable are supported by this slice.
- File transfer, file upload, compression, resource download, custom sprays,
  consistency enforcement, interleaved normal/file streams, and multiple
  queued reliable applications are not implemented.
- Delta descriptions, movevars, baselines, snapshots, packet entities,
  clientdata, usercmd, movement, prediction, `ClientPutInServer`, spawn,
  weapons, damage, gameplay, and changelevel continuity remain outside scope.
- Steam authentication, public-server interaction, master-server
  communication, RCON, voice, `cstrike`, Metamod, AMX Mod X, and complete HLDS
  compatibility are not claimed.

Fragmented resource-manifest acknowledgement does not mean that resource downloads, consistency verification, delta descriptions, baselines, snapshots, spawn, or gameplay are complete.

## Final Git status

- The task contains one intended source commit with subject
  `feat(net): add GoldSrc reliable fragmentation slice`.
- The commit contains exactly the 17 source/documentation/test files listed
  above.
- `out/`, generated binaries, logs, validation reports, local game data, and
  the SDK are excluded.
- No unrelated source change was reset, restored, deleted, or included.
- The branch is intended for one normal non-force push to `origin`.
- No pull request is requested or created.
- After commit and push, the local HEAD and exact remote task-branch head are
  verified separately and reported in the final task response.
