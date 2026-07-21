# First GoldSrc serverinfo signon slice

## Scope, baseline, and compatibility claim

This vertical slice targets one external IPv4 UDP client for the Half-Life
`valve` game, GoldSrc protocol 48, and insecure dedicated-server operation. It
starts after the existing connectionless handshake and minimal netchan have
established one authoritative connected session. It accepts the client's first
reliable `new` command, queues one reliable serverinfo response, and stops after
that response is acknowledged.

The selected `hl-engine` task branch is
`codex/goldsrc-signon-serverinfo-slice`. Its selected base is the full netchan
baseline commit `1e65c5a48d505b6e0d89dcb4e6716281d8f7f4c3`
(`feat(net): add minimal GoldSrc netchan reliable slice`). The current signon
work is a direct continuation of that baseline.

The physics/Jolt work at
`96db2244ff8b4a50670bc4306cd5273d224f077a` is preserved separately on the
surrounding `hl-client` history and its
`codex/physprop-wmodel-jolt-demo` local/remote branch. That commit is not in the
selected `hl-engine` ancestry and was not merged or copied into this slice.

This is a bounded compatibility slice, not a complete GoldSrc signon. The
automated proofs use an independently implemented external UDP probe. An
unmodified stock Half-Life client has not been tested, so stock-client
interoperability remains unproven.

## Public protocol evidence

The implementation is independently written. Public engine sources are used
as behavioral documentation and were not copied into this repository:

- ReHLDS commit `0124d56c3d888d922eb045775f71c6682ad1226f`,
  [`sv_main.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1107-L1187),
  documents the serverinfo field sequence, checksum transformation key,
  client-library digest, game/map strings, and the companion extra-info
  message. The same pinned file documents the
  [client-library MD5 and map CRC sources](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L6469-L6493)
  and the [`new` command entry point](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1509-L1536).
- The pinned ReHLDS
  [`net_chan.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_chan.cpp#L269-L542)
  transmit path and
  [receive path](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_chan.cpp#L689-L818)
  are the already-selected reference for reliable acknowledgement and strict
  resend behavior.
- Xash3D FWGS commit `009855c193c951da7068af0cb2cc14817375efbc`,
  [`protocol.h`](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/protocol.h),
  independently corroborates the GoldSrc protocol message identifiers, while
  its
  [`munge.c`](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/munge.c#L24-L140)
  corroborates the reversible GoldSrc transformation family.

These upstream projects retain their own licenses. The links are provenance
for protocol behavior, not a claim that this host is ReHLDS, Xash3D, or a
historical Valve engine build.

## Accepted client command and exactly-once delivery

The application decoder runs only after endpoint routing, netchan validation,
payload transformation, sequence validation, and reliable delivery handling.
It accepts exactly one NUL-terminated client string command whose complete text
is lowercase `new`. Protocol no-ops may precede or follow that single command.
The command must arrive in a reliable client payload. Additional commands,
arguments, separators, non-no-op trailing data, unsupported opcodes, missing
termination, or control characters do not advance signon.

Exactly-once behavior is enforced at two layers:

1. the netchan rejects a duplicate outer sequence before application delivery;
2. the per-session signon state suppresses a retransmitted reliable `new` after
   its first delivery, even when it arrives under a newer valid outer sequence.

The serverinfo context is generated and the payload is queued before the
signon state commits the first delivery. A successful first delivery therefore
generates and queues serverinfo once. A freshly admitted authoritative slot
starts with reset netchan, signon, payload, and diagnostic state, so a reused
disconnected slot cannot inherit delivery state from an earlier session.

## Explicit signon phases

| Phase | Entry condition | Allowed advancement |
|---|---|---|
| `none` | Fresh or reset authoritative session | Netchan establishment enters `awaiting_new`. |
| `awaiting_new` | Initial reliable netchan exchange is acknowledged | The first accepted reliable `new` queues serverinfo. |
| `serverinfo_queued` | Serverinfo context and bounded payload are committed | The first successful reliable send enters `serverinfo_sent_awaiting_ack`. |
| `serverinfo_sent_awaiting_ack` | Serverinfo has a recorded reliable carrier | Only the correlated correct reliable acknowledgement advances the phase. |
| `serverinfo_acknowledged` | The pending serverinfo reliable is cleared | Terminal phase for this slice; repeated transitions are idempotent. |

Invalid transitions do not skip phases. Retransmitting the already queued
payload does not count as a second serverinfo generation, send transition, or
client-command delivery.

## Serverinfo semantic contract

All 32-bit scalar fields are encoded explicitly in little-endian order. Text
fields are NUL-terminated protocol strings. The payload contains exactly one
serverinfo message followed by its required extra-info companion; the local
decoder accepts no trailing message or byte.

| Order | Field | Protocol type | Runtime source in this slice |
|---:|---|---|---|
| 1 | serverinfo message identifier | unsigned byte | Protocol constant for serverinfo. |
| 2 | protocol version | unsigned 32-bit integer | Fixed to protocol 48. |
| 3 | map spawn count | unsigned 32-bit integer | Authoritative server state's map-initialization generation; incremented per initialization and wrapped from the maximum value to 1. |
| 4 | transformed map checksum | unsigned 32-bit integer | Canonical checksum of the loaded BSP, transformed with the zero-based player index. |
| 5 | client-library identity | 16-byte MD5 digest | Contents of the resolved client library selected for the running game directory. |
| 6 | maximum clients | unsigned byte | Normalized authoritative `maxclients`, restricted to 1 through 255. |
| 7 | player index | unsigned byte | Authoritative one-based slot minus one; it must be less than `maxclients`. |
| 8 | deathmatch | boolean byte | True only when coop is disabled and deathmatch is enabled. |
| 9 | game directory | protocol string | Normalized authoritative mod name; `valve` for this target. |
| 10 | hostname | protocol string | Final authoritative hostname after server cvar initialization. |
| 11 | map model path | protocol string | Loaded world context model path, constrained to a normalized `maps/name.bsp` form without traversal. |
| 12 | mapcycle | protocol string | Current `mapcyclefile` cvar value. |
| 13 | secure mode | boolean byte | Fixed false; secure mode is outside this slice. |
| 14 | extra-info message identifier | unsigned byte | Required protocol companion identifier. |
| 15 | fallback game directory | protocol string | Empty for this target. |
| 16 | cheats allowed | boolean byte | Fixed false; a true value is rejected by the bounded codec. |

The implementation intentionally stops this semantic stream after the
extra-info companion. It does not append the later delta descriptions,
movevars, resources, baselines, view setup, or other messages present in a full
server signon.

## Checksum and client-library identity

The canonical map checksum is calculated from the authoritative loaded BSP30
file. The parser requires a complete BSP30 header and validates every lump
range against the file. It initializes the CRC32 accumulator to
`0xFFFFFFFF`, processes lump payloads 1 through 14 in logical lump-index order,
ignores lump 0, and does not apply a final XOR. Before serialization, the
canonical value is transformed with the GoldSrc COM_Munge3-compatible
player-index key; the inverse is used by the semantic decoder.

The client-library identity is the ordinary 16-byte MD5 of the complete
resolved client DLL file used by the host. It is streamed from that file at
runtime rather than copied from a fixture, inferred from a filename, or filled
with a constant. Failure to open or read either identity source prevents
serverinfo from being queued.

## Bounds and validation

- A client signon application payload and an encoded serverinfo payload are
  each capped at 1200 bytes, matching the selected reliable-buffer limit.
- Client command text is capped at 64 bytes; this slice still accepts only the
  exact three-character `new` command.
- Game directory, hostname, map model path, mapcycle, and fallback directory
  are capped at 63, 255, 63, 1023, and 63 bytes respectively, excluding their
  terminators.
- Required strings must be non-empty. Embedded NULs and disallowed control
  characters are rejected before encoding.
- The map model path must use the `maps/` prefix and `.bsp` suffix and cannot
  contain backslashes or parent traversal.
- The BSP must be version 30 and no larger than 512 MiB. Header and lump
  arithmetic are checked before file data is processed.
- A missing or all-zero map checksum or client-library digest is rejected.
- Only boolean values zero and one decode successfully. Secure and cheats-on
  modes are deliberately unsupported.
- The encoder uses fixed-capacity storage and checked writes. It fails as a
  whole rather than truncating a field or emitting a partial payload.

## Golden fixture provenance

The unit-test serverinfo fixture is a small, hand-authored protocol-48 semantic
fixture spelled independently of the encoder from the pinned public field
contract. It contains synthetic names, a synthetic checksum, and the digest of
synthetic test text; it is not a capture and contains no game binary data.
Encoder equality, decoder semantics, round-trip behavior, immutable input,
field boundaries, and signon transitions are checked against it.

The BSP checksum fixture constructs a minimal synthetic BSP30 image in memory
with deliberately different physical and logical lump order. The digest tests
use standard MD5 test vectors plus synthetic client-library text. Temporary
files are created only for file-reader parity and are removed by the tests.

The external proof does not embed a map, client library, capture, or recorded
packet fixture. It receives an operator-supplied legally obtained game
directory, computes the expected identities from those runtime files, and
compares the complete decoded semantics. No absolute machine path is part of
the compatibility contract.

## Reliable acknowledgement and retransmission

Serverinfo is queued as the single pending reliable payload with an explicit
`serverinfo` payload kind. The netchan records the sequence that carries it.
An incoming packet clears it only when the ordinary acknowledgement covers a
real sent carrier and the reliable acknowledgement toggle matches the local
reliable generation. The runtime advances signon only when netchan reports
that the acknowledged reliable kind is `serverinfo`; an unrelated or stale
acknowledgement cannot advance the phase.

A wrong reliable toggle does not clear the bytes. Under the selected strict
netchan rule, retransmission becomes eligible after an accepted ordinary ACK
has advanced beyond the last reliable carrier while the reliable toggle still
does not match. The next outgoing packet carries the same pending serverinfo
bytes under a new outer sequence. There is no timer-only resend in this slice.
The eventual correct acknowledgement clears the payload and advances signon
exactly once.

## Reproduction commands

Run these commands from the surrounding HLengine workspace root, with this
repository checked out as its `host` directory. `<valve-game-directory>` must
name a legally obtained local `valve` game directory. Every repository and
build reference below is relative; no machine-specific path is required.

Build and unit tests:

```powershell
cmake --preset vs2022-win32
cmake --build --preset vs2022-debug --target `
  hlhost goldsrc_connectionless_unit_tests goldsrc_netchan_unit_tests `
  goldsrc_signon_unit_tests
ctest --test-dir '.\out\build\vs2022-win32\host' `
  -C Debug --output-on-failure
```

Serverinfo Proof A, external success path:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File '.\host\scripts\run_goldsrc_serverinfo_proof.ps1' `
  -ExecutablePath '.\out\build\vs2022-win32\host\Debug\hlhost.exe' `
  -GameDir '<valve-game-directory>' -BindAddress 127.0.0.1 `
  -Port 0 -TimeoutSeconds 30 -SkipServerOutput
```

Serverinfo Proof B, retransmission and rejection path:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File '.\host\scripts\run_goldsrc_serverinfo_proof.ps1' `
  -ExecutablePath '.\out\build\vs2022-win32\host\Debug\hlhost.exe' `
  -GameDir '<valve-game-directory>' -BindAddress 127.0.0.1 `
  -Port 0 -TimeoutSeconds 30 `
  -ExerciseServerInfoRetransmitAndInvalidCommands -SkipServerOutput
```

Required pre-existing regressions:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File '.\host\scripts\run_goldsrc_udp_handshake_proof.ps1' `
  -ExecutablePath '.\out\build\vs2022-win32\host\Debug\hlhost.exe' `
  -GameDir '<valve-game-directory>' -BindAddress 127.0.0.1 `
  -Port 0 -TimeoutSeconds 30 -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File '.\host\scripts\run_goldsrc_udp_handshake_proof.ps1' `
  -ExecutablePath '.\out\build\vs2022-win32\host\Debug\hlhost.exe' `
  -GameDir '<valve-game-directory>' -BindAddress 127.0.0.1 `
  -Port 0 -TimeoutSeconds 30 -ExerciseDisconnectedSlotReuse `
  -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File '.\host\scripts\run_goldsrc_netchan_proof.ps1' `
  -ExecutablePath '.\out\build\vs2022-win32\host\Debug\hlhost.exe' `
  -GameDir '<valve-game-directory>' -BindAddress 127.0.0.1 `
  -Port 0 -TimeoutSeconds 30 -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File '.\host\scripts\run_goldsrc_netchan_proof.ps1' `
  -ExecutablePath '.\out\build\vs2022-win32\host\Debug\hlhost.exe' `
  -GameDir '<valve-game-directory>' -BindAddress 127.0.0.1 `
  -Port 0 -TimeoutSeconds 30 -ExerciseReliableRetransmit `
  -SkipServerOutput

powershell -NoProfile -ExecutionPolicy Bypass `
  -File '.\host\scripts\run_hlds_query_info_regression.ps1' `
  -Mode all -NoBuild `
  -ExecutablePath '.\out\build\vs2022-win32\host\Debug\hlhost.exe' `
  -GameDir '<valve-game-directory>'
```

## Sanitized expected proof summaries

Proof A must end with the following stable semantic result. Dynamic port,
challenge, checksum, digest, and process identifiers are deliberately omitted:

```text
connectionless_handshake=pass
netchan_established=true
client_new_delivered=1
serverinfo_decode=pass
serverinfo_acked=true
signon_phase=serverinfo_acknowledged
session_count=1
put_in_server=0
spawned=0
active=0
clean_shutdown=1
proof_a=pass
```

Proof B must end with this stable result:

```text
serverinfo_retransmitted=true
retransmitted_payload_identical=true
wrong_reliable_ack_rejected=true
future_ack_rejected=true
duplicate_client_reliable_suppressed=true
malformed_client_message_rejected=true
unsupported_command_rejected=true
endpoint_hijack_rejected=true
client_new_deliveries=1
serverinfo_generations=1
session_count=1
put_in_server=0
spawned=0
active=0
server_still_responsive=true
clean_shutdown=1
proof_b=pass
```

These are pass criteria, not a stock-client compatibility claim. The proof
runner must use the actual `hlhost` executable in a separate process, a
separate external UDP socket, and the production handshake, session, netchan,
and signon paths. It also verifies clean process ownership, temporary-file
cleanup, and no repository-file mutation.

## Stock-client blocker

Stock-client status: **untested**. The exact blocker is the absence of an
automated, reproducible harness that launches an unmodified legally obtained
Half-Life client, drives this signon stage on isolated loopback, records a
sanitized result, and proves deterministic process and artifact cleanup. The
PowerShell UDP probe proves the implemented protocol contract but is not a
stock client. Until that automated proof exists and passes, this document
makes no stock Half-Life, Steam client, full HLDS, public-server, or gameplay
compatibility claim.

## Known limitations and mandatory boundary

- Only one external client and one in-flight server reliable payload are
  supported; there is no general reliable queue.
- Fragment creation, fragment reassembly, split payloads, downloads, endpoint
  migration, and later signon batches are outside this slice.
- Resources, consistency checks, delta descriptions, movevars, baselines,
  snapshots, packet entities, clientdata, events, user messages, user commands,
  movement, prediction, spawn, and gameplay are not implemented here.
- The client remains authoritative-session state `connected` with
  `put_in_server=0`, `spawned=0`, and `active=0` after serverinfo ACK.
- Steam ticket verification, secure mode, master-server traffic, RCON, voice,
  uploads, `cstrike`, Metamod, AMX Mod X, and non-Windows socket backends are
  outside this milestone.
- The serverinfo feature is opt-in. With it disabled, the existing normal host
  and connectionless query/info behavior must remain unchanged.

Serverinfo acknowledgement does not mean that resources,
baselines, snapshots, spawn, or gameplay are complete.

No proprietary Valve binary, packet capture, absolute machine path, build
output, or credential belongs in this document or its fixtures.
