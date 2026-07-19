# GoldSrc connectionless UDP handshake vertical slice

## Scope and compatibility target

This slice targets the GoldSrc Half-Life client family, the `valve` game, direct
IPv4 UDP connection, local/LAN use, protocol 48, insecure operation, one client,
and the connectionless `getchallenge`/`connect` exchange only. The proof server
binds an explicitly selected address and port and exits after one accepted
connection or a bounded timeout.

Reference configuration used for the live proof:

- server executable: this repository's Win32 Debug `hlhost.exe`;
- game data: the tracked `valve-fixture` containing the Half-Life server DLL;
- reference protocol family: GoldSrc protocol 48;
- auth envelope: Steam-shaped protocol info (`prot=3`, `raw=steam`) in insecure
  mode, with server Steam ID `0`, secure flag `0`, and no ticket validation;
- network: separate processes and separate IPv4 UDP sockets on `127.0.0.1`;
- observed proof values on 2026-07-19: server port `63852`, client port `50306`,
  challenge `1174052652`, admitted slot `1`.

Wire-compatible with the recorded connectionless handshake fixture used by the external probe; stock-client interoperability has not yet been proven.

## Protocol evidence and constant provenance

No implementation was copied from Xash3D FWGS or ReHLDS. Their source is used
only as protocol documentation and as corroboration for wire constants.

| Constant or shape | Origin |
|---|---|
| `FF FF FF FF` connectionless prefix and the `getchallenge steam` / quoted protocol-48 `connect` grammar | Public Xash3D protocol notes ([upstream connectionless documentation](https://github.com/FWGS/xash3d-fwgs/blob/master/Documentation/protocol/02-connectionless.md)) and client implementation ([upstream Xash3D client](https://github.com/FWGS/xash3d-fwgs/blob/master/engine/client/cl_main.c)) |
| Protocol version `48` | GoldSrc mode in the same Xash3D client and its protocol declarations |
| Challenge marker `A00000000`, accept marker `B`, reject marker byte `9` | Public Xash3D protocol declarations ([upstream protocol header](https://github.com/FWGS/xash3d-fwgs/blob/master/engine/common/protocol.h)) |
| Challenge response fields `<challenge> 3 0 0` and terminating linefeed/NUL | ReHLDS `SVC_GetChallenge`: auth protocol 3, insecure server Steam ID 0, secure flag 0 ([upstream server source](https://github.com/rehlds/ReHLDS/blob/master/rehlds/engine/sv_main.cpp)) |
| Accept shape `B <userid> "<remote-ip:port>" <secure> <build>` | ReHLDS connection acceptance path in `sv_main.cpp` |
| Accept build field `5971` | Xash3D GoldSrc client parses field 4 as the server build and uses `>= 5971` as its documented extended-usercmd threshold. This slice records `5971` as a deterministic compatibility field; it is not a claim that `hlhost` is that historical engine build. |
| Challenge identity is remote IPv4 host, not source port | ReHLDS challenge lookup compares base addresses; the local table consequently permits a port change for the same IPv4 host. |

The public sources carry their own licenses. This repository's implementation is
an independently written bounded codec, challenge table, and socket adapter.

## Recorded byte fixture

All dumps preserve the complete UDP datagram. Dynamic fields from the successful
run are shown with their recorded values.

### 1. Client challenge request

Decoded body: `getchallenge steam\n\0`

```text
FF FF FF FF 67 65 74 63 68 61 6C 6C 65 6E 67 65 20 73 74 65 61 6D 0A 00
```

The prefix, ASCII command, linefeed, and terminal NUL are fixed.

### 2. Server challenge response

Decoded body: `A00000000 1174052652 3 0 0\n\0`

```text
FF FF FF FF 41 30 30 30 30 30 30 30 30 20 31 31 37 34 30 35 32 36 35 32 20 33 20 30 20 30 0A 00
```

`1174052652` varies and is generated from the Windows system cryptographic RNG.
The marker, auth protocol `3`, insecure Steam ID `0`, secure flag `0`, linefeed,
and NUL are fixed for this target.

### 3. Client connect request

Decoded line (followed by five binary auth-tail bytes):

```text
connect 48 1174052652 "\prot\3\unique\-1\raw\steam\cdkey\00000000000000000000000000000000" "\name\udp_probe\model\gordon"\n
```

```text
FF FF FF FF 63 6F 6E 6E 65 63 74 20 34 38 20 31 31 37 34 30 35 32 36 35 32 20 22 5C 70 72 6F 74 5C 33 5C 75 6E 69 71 75 65 5C 2D 31 5C 72 61 77 5C 73 74 65 61 6D 5C 63 64 6B 65 79 5C 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 22 20 22 5C 6E 61 6D 65 5C 75 64 70 5F 70 72 6F 62 65 5C 6D 6F 64 65 6C 5C 67 6F 72 64 6F 6E 22 0A 00 01 7F 80 FF
```

The protocol version and quoted-field grammar are fixed. The challenge,
protocol/user info values, player name, optional `qport`/`ext` pairs, and binary
tail vary. The binary tail is preserved but is intentionally not authenticated.

### 4. Server accept response

Decoded body: `B 1 "127.0.0.1:50306" 0 5971\0`

```text
FF FF FF FF 42 20 31 20 22 31 32 37 2E 30 2E 30 2E 31 3A 35 30 33 30 36 22 20 30 20 35 39 37 31 00
```

The slot/user ID is `1` for this one-client target. The quoted remote endpoint
varies with the client's actual source port. Secure mode `0`, compatibility build
field `5971`, and terminal NUL are fixed by this slice.

### 5. Rejection response

Shape: `FF FF FF FF 39 <bounded printable reason> 00`. Reasons are sanitized and
limited to 127 bytes. Examples include `unsupported_protocol`,
`challenge_unknown`, `challenge_expired`, `challenge_endpoint_mismatch`,
`challenge_consumed`, `duplicate-client`, and `server-full`.

## Implemented behavior

- move-only Win32 IPv4 UDP adapter with exact bind, nonblocking receive,
  sender preservation, 2048-byte datagram limit, suppression of peer-triggered
  `SIO_UDP_CONNRESET`, and clean RAII shutdown;
- module-owned socket lifetime with an externally driven, nonblocking host-owned
  network-frame pump that processes at most eight complete datagrams per frame,
  starts its timeout on the first pump, and closes at explicit finish or RAII
  shutdown; this finite slice does not advance DLL gameplay frames;
- strict connectionless framing and command dispatch with no console execution;
- bounded quoted connect and info-string parsing (255 bytes, 32 pairs, 63-byte
  keys, 127-byte values), numeric overflow checks, sanitized player logging,
  exact final counters, and logarithmically sampled unauthenticated diagnostics;
- 64-entry, 30-second, IPv4-host-scoped challenge table with deterministic
  expiry/oldest eviction and consume-on-success replay protection;
- BCrypt-backed runtime challenge generation and injectable test generation;
- mutation-free admission planning through the existing capacity/free-slot
  rules, followed only after validation by the existing
  `AdmitDedicatedLoopbackPreauthPlayer` authoritative slot/lifecycle commit seam
  (the legacy function name predates this real socket path);
- disconnected authoritative slots are replaced with fresh per-session state
  before lifecycle transitions, preventing inherited spawn/signon metadata;
- the same authoritative slot stores the actual endpoint, protocol, qport or
  source-port channel identifier, challenge, auth mode, extensions, protocol
  info, and user info;
- final lifecycle state `connected`, with `put_in_server=0`, `spawned=0`, and
  `active=0`; the challenge is consumed only after successful admission.

## Live proof

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_goldsrc_udp_handshake_proof.ps1 `
  -ExecutablePath 'G:\DEV\СPP\HLengine\out\build\vs2022-win32\host\Debug\hlhost.exe' `
  -GameDir '.\logs\latest\HL-CL-20260411-160-dedicated-goldsrc-signon-carried-checkpoint-claimed-checkpoint-resume-allow-surface\runtime\valve-fixture' `
  -BindAddress 127.0.0.1 -TimeoutSeconds 30 -SkipServerOutput
```

Recorded result:

```text
goldsrc_udp_probe: port=63852
goldsrc_udp_probe: challenge=1174052652
goldsrc_udp_probe: response=accept endpoint=127.0.0.1:50306
goldsrc_udp_probe: session=count=1,state=connected,connected=1,put_in_server=0,spawned=0,active=0,protocol=48,metadata=present
goldsrc_udp_probe: server=frames=4,max_datagrams_per_frame=8,datagrams=2,challenges=1,connects=1,accepted=1,rejected=0,session_count=1,state=connected,clean_shutdown=1
goldsrc_udp_probe: result=pass
```

The final server summary recorded `frames=4`, `max_datagrams_per_frame=8`,
`datagrams=2`, `challenges=1`, `connects=1`, `accepted=1`, `rejected=0`,
`session_count=1`, and `clean_shutdown=1`.

An additional run with `-ExerciseDisconnectedSlotReuse` seeded the legacy
synthetic lifecycle, admitted the real UDP client into its disconnected slot 2,
and passed with that admitted slot still `spawned=0` and `active=0`.

## Intentionally unsupported

This slice does not implement netchan sequence/ack fields, reliable transport,
fragmentation, reassembly, signon messages, resources, consistency checks,
baselines, snapshots, packet entities, clientdata, user commands, movement,
gameplay, Steam ticket verification, master-server traffic, RCON, voice, uploads,
server-browser queries, `cstrike`, Metamod, or AMX Mod X. The proof policy stops
the host-owned network-frame pump after the first accepted session; no
post-connect packet is sent before the module-owned transport is closed during
host finish.
