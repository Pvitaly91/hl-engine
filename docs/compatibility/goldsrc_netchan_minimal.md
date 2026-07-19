# Minimal GoldSrc protocol-48 netchan slice

## Scope and compatibility statement

This slice targets one external UDP client fixture using the `valve` game,
GoldSrc protocol 48, direct IPv4 UDP, and insecure mode. It starts only after
the connectionless `getchallenge` / `connect` admission has created the one
authoritative session. The implementation baseline is branch
`origin/codex/goldsrc-udp-handshake`, commit
`84fa3ab2b052cf060b6859ec21245603243e0e40` (`Implement real GoldSrc UDP
handshake slice`).

The implemented transport is deliberately pre-signon: one server reliable
payload may be in flight, fragmentation is rejected, and the session stays
`connected` with `put_in_server=0`, `spawned=0`, and `active=0`.

Wire-compatible with the documented minimal netchan fixture used by the
external probe. Stock-client signon interoperability has not yet been proven.

## Pinned protocol evidence

The implementation was written independently. The following public sources
are behavioral evidence, not copied implementation:

- ReHLDS commit `0124d56c3d888d922eb045775f71c6682ad1226f`:
  [`net_chan.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_chan.cpp#L187-L221)
  initializes outgoing sequence 1; its transmit/process paths document the two
  32-bit words, reliable/fragment bits, padding, `COM_Munge2`, ACK clearing,
  and strict resend condition
  ([transmit](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_chan.cpp#L269-L542),
  [receive](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_chan.cpp#L689-L818));
  [`net.h`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net.h#L302-L391)
  defines the 1200-byte reliable limit and channel sequence/toggle fields;
  [`net_ws.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_ws.cpp#L187-L202)
  defines full IPv4 endpoint comparison; and
  [`sv_main.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L3937-L3961)
  routes sequenced server packets only after that full endpoint comparison.
- Xash3D FWGS commit `009855c193c951da7068af0cb2cc14817375efbc`:
  [`cl_main.c`](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/client/cl_main.c#L1276-L1292)
  puts `qport` in connection metadata and
  [requires an exact server endpoint](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/client/cl_main.c#L2849-L2879)
  for sequenced packets;
  [`net_chan.c`](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/net_chan.c#L1683-L1794)
  documents the same header and transform and explicitly omits the extra
  client qport word for `gs_netchan`;
  [`munge.c`](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/munge.c#L24-L89)
  provides the `COM_Munge2` table and reversible block algorithm; and
  [`protocol.h`](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/protocol.h#L21-L87)
  defines `svc_nop = 1` and `clc_nop = 1` (with protocol 48 declared
  [here](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/protocol.h#L285-L288)).

ReHLDS is the routing precedent selected for this milestone. Xash3D's server
can instead authenticate a GoldSrc packet by base address and then adopt a
changed source port; arbitrary endpoint migration is intentionally not part of
this slice.

## Wire format

Every sequenced packet in either direction begins with the same eight-byte
base header. Both fields are unsigned 32-bit little-endian words; no unaligned
host-endian reads are used.

| Offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 4 | `sequence_word` | Bits 0-29: sequence; bit 30: fragment present; bit 31: reliable payload present. |
| 4 | 4 | `ack_word` | Bits 0-29: acknowledged server sequence; bit 30: reserved; bit 31: reliable-ACK toggle. |
| 8 | remainder | transformed payload | Direction-specific protocol messages, transformed with the packet sequence. |

The numeric sequence mask is `0x3fffffff`. Sequence bit 31 is a
**reliable-present indication**, not the reliable toggle itself. Receiving a
packet with that bit set toggles the receiver's incoming reliable sequence.
ACK bit 31 returns the receiver's current incoming reliable toggle; a pending
server reliable is cleared only when this bit matches the server's local
reliable toggle and the ordinary ACK covers the sequence that carried it.

Sequence bit 30 (`0x40000000`) announces fragment metadata. Fragment creation
and reassembly are unsupported: such a datagram returns
`unsupported_fragment` before channel state or activity time changes. ACK bit
30 is not a second fragment bit; it is reserved and is rejected as
`unsupported_reserved_flags`, also before transport-state mutation.

The decoder accepts an exact eight-byte header as the minimum packet. Runtime
output is padded to at least 16 total bytes. The codec caps a complete
sequenced UDP datagram at 1400 bytes (1392 bytes after the base header), inside
the transport's existing 2048-byte receive limit; larger sequenced datagrams
are rejected rather than truncated. The one pending reliable buffer is
separately capped at 1200 bytes.

### Direction and qport decision

Client-to-server and server-to-client packets use the same eight-byte header
and have no extra wire qport/channel field. Xash3D's transmit path writes a
two-byte qport only for a client channel that is *not* `gs_netchan`; ReHLDS
reads the two long words directly and its server routes by full remote address.
This is also why all fixtures below begin payload at byte 8.

The optional protocol-info `qport` parsed during `connect` remains stored in
authoritative session metadata for future compatibility work, but it is not
serialized, parsed, or compared on sequenced packets in this milestone. The
router requires the exact admitted IPv4 address and UDP source port. Unknown
or changed endpoints receive no amplification response, create no session,
and cannot update an existing session. The external probe therefore uses one
UDP socket for challenge, connect, and all netchan traffic. Two clients behind
the same IPv4 address remain distinct by source port.

## Initial and reliable state

A newly admitted session starts with:

```text
outgoing_sequence=1
incoming_sequence=0
local_reliable_toggle=0
incoming_reliable_toggle=0
incoming_reliable_ack_toggle=0
highest_accepted_ack=0
transport_phase=none
```

The first queued payload is exactly one byte, `01` (`svc_nop`). Queueing it
toggles the local reliable state from 0 to 1 once and moves the phase to
`awaiting_first_reliable_ack`. The first transmit uses outer sequence 1,
sets sequence bit 31 because reliable bytes are present, records sequence 1 as
the carrier, and advances the next outgoing sequence to 2. The sequence bit
does not encode the value 1 of the toggle.

Only one reliable payload may be pending. An empty payload, a payload over
1200 bytes, or a second queue attempt is rejected without changing the first
buffer or toggling reliable state again. Normal output pads the one-byte
`svc_nop` with seven more `svc_nop` bytes, yielding the decoded eight-byte
payload `01 01 01 01 01 01 01 01` and a 16-byte datagram. Client proof packets
likewise contain one `clc_nop` plus seven `clc_nop` padding bytes. Both opcodes
are numerically 1, but their protocol direction is distinct.

`svc_nop` is a no-op. It does not send `svc_signonnum`, does not change a
signon stage, and does not put, spawn, or activate the client.

## Payload transformation

The base header is never transformed. `COM_Munge2` is applied starting at
offset 8 for `datagram_size - 8` bytes, using the low byte of the decoded
30-bit packet sequence as the key. Only complete four-byte groups are changed;
any trailing one to three payload bytes remain byte-for-byte unchanged.

The 16-byte table is:

```text
05 61 7A ED 1B CA 0D 9B 4A F1 64 C7 B5 8E DF A0
```

For each four-byte block, the forward form reads a little-endian word, XORs
the sequence key, byte-swaps it, XORs byte `j` with
`0xA5 | (j << j) | j | table2[(block_index + j) & 0x0f]`, then XORs the
resulting word with the bitwise complement of the key. Decode applies the
inverse ordering. This range, cast-to-low-byte key, table, and algorithm are
corroborated independently by the pinned ReHLDS `net_chan.cpp` and Xash3D
`net_chan.c` / `munge.c` sources above.

The local golden vector uses decoded bytes `00 01 02 03 04 05 06 07` and
sequence `0x12345678` (key `0x78`):

```text
decoded: 00 01 02 03 04 05 06 07
munged:  21 1A 01 78 65 06 15 3C
```

The bounded on-wire fixtures below are additional golden vectors derived from
the same pinned algorithm.

## Sequence, ACK, and resend rules

Sequence comparisons operate in the 30-bit ring. For candidate `c` and
baseline `b`, distance is `(c - b) & 0x3fffffff`; `c` is newer only when the
distance is nonzero and below the half range `0x20000000`. Thus an exact
duplicate is rejected, an older/ambiguous packet is out of order, and
`0x3fffffff -> 0` is a valid one-step wrap. A forward gap is accepted and adds
`distance - 1` to the dropped-packet count. Rejected duplicate or out-of-order
payload is never delivered.

Validation is atomic with respect to sequence, reliable state, accepted ACK,
phase, and activity time. The endpoint, complete header, size, flags,
transformed payload (currently only `clc_nop` bytes), incoming sequence, and
ACK are checked before those fields change. Rejection diagnostics may still
increment. ACK 0 is the initial sentinel. Any other ACK must identify a
sequence actually sent by this channel; an unsent future ACK is rejected, and
an ACK older than the highest already accepted ACK is stale. Neither can clear
reliable bytes or keep the session alive.

Sent-sequence validation is relative to the latest outgoing sequence and keeps
only the unambiguous half-range of the 30-bit ring. The ACK 0 sentinel does not
identify a sent packet, cover reliable bytes, or trigger resend until outgoing
sequence 0 has actually been emitted. Immediately before wrap, an ACK 0 after
a previously accepted nonzero ACK is therefore future; immediately after
sequence `0x3fffffff -> 0` is sent, it is a valid covering ACK.

A pending reliable clears only when an otherwise accepted packet both:

1. ACKs the recorded reliable carrier sequence or a newer sent sequence; and
2. carries ACK bit 31 equal to the current local reliable toggle.

The reference-compatible resend test is deliberately strict. A mismatch at an
ACK exactly equal to the reliable carrier records the mismatch but does not by
itself resend. Resend becomes eligible when the highest accepted ordinary ACK
is **newer than** the last reliable carrier and the reliable-ACK toggle still
differs. The next server packet then carries the identical pending bytes under
a new outer sequence, preserves the reliable toggle, records the new carrier,
and increments the resend counter. There is no timer-only resend invented for
this slice.

## Exact bounded hex fixtures

All multi-byte words below are little-endian. Each 16-byte dump is the complete
on-wire UDP payload after `COM_Munge2`, not decoded payload. The transform
decodes the last eight bytes of every normal fixture to eight `01` no-ops.

### Normal acknowledgement

Fresh server packet S1: `seq=1`, `ack=0`, reliable present, fragment absent,
reliable-ACK toggle 0.

```text
01 00 00 80 00 00 00 00 5A 19 01 00 1A 01 11 40
```

Correct client ACK: `seq=1`, `ack=1`, no reliable payload, reliable-ACK toggle
1. This clears S1 and establishes the pre-signon channel.

```text
01 00 00 00 01 00 00 80 5A 19 01 00 1A 01 11 40
```

### Reference resend flow

This flow starts from a fresh S1 above. Before a client ACK, the server emits
ordinary S2: `seq=2`, `ack=0`, no reliable payload and both toggle bits clear.

```text
02 00 00 00 00 00 00 00 59 19 01 03 19 01 11 43
```

Wrong reliable ACK C1: `seq=1`, ordinary `ack=2`, no client reliable payload,
old reliable-ACK toggle 0. ACK 2 is a sequence the server really sent and is
newer than S1's reliable carrier, so the packet is accepted but cannot clear
the pending bytes; it makes the strict resend condition true.

```text
01 00 00 00 02 00 00 00 5A 19 01 00 1A 01 11 40
```

Reliable resend S3: `seq=3`, `ack=1`, reliable present with the same decoded
eight no-op bytes, fragment absent, reliable-ACK toggle 0.

```text
03 00 00 80 01 00 00 00 58 19 01 02 18 01 11 42
```

Final correct C2: `seq=2`, `ack=3`, no client reliable payload,
reliable-ACK toggle 1. It covers the new reliable carrier and clears it.

```text
02 00 00 00 03 00 00 80 59 19 01 03 19 01 11 43
```

### Rejected inputs

Malformed seven-byte datagram (one byte short of the base header):

```text
00 00 00 00 00 00 00
```

Unsupported fragment header: `seq=1` plus sequence bit 30, `ack=1` plus the
reliable-ACK bit. It is rejected from these eight bytes without attempting
fragment allocation or payload decoding.

```text
01 00 00 40 01 00 00 80
```

Reserved ACK-bit-30 header:

```text
01 00 00 00 01 00 00 40
```

Forged future ACK, applied to a fresh channel after only S1 was sent:
`seq=1`, unsent `ack=2`, and the otherwise-correct reliable-ACK toggle 1. It
must not clear S1 despite the matching toggle.

```text
01 00 00 00 02 00 00 80 5A 19 01 00 1A 01 11 40
```

Sending any otherwise-valid fixture from a second UDP socket is the endpoint
hijack fixture; it is discarded before decode-to-session state and receives no
reply.

## Reproduction commands

Run from the surrounding HLengine workspace root, with this repository checked
out as its `host` directory as described in the repository README. These
commands use only relative paths and placeholders; `<valve-game-directory>`
must identify a legally obtained game directory usable by the dedicated host.

```powershell
Set-Location '<HLengine-workspace-root>'

cmake --preset vs2022-win32
cmake --build --preset vs2022-debug --target `
  goldsrc_connectionless_unit_tests goldsrc_netchan_unit_tests hlhost
ctest --test-dir '.\out\build\vs2022-win32\host' `
  -C Debug --output-on-failure
```

Existing connectionless and slot-reuse proofs:

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
```

Normal netchan Proof A and retransmission/negative Proof B:

```powershell
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
```

Feature-off query/info regression:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File '.\host\scripts\run_hlds_query_info_regression.ps1' `
  -Mode all -NoBuild `
  -ExecutablePath '.\out\build\vs2022-win32\host\Debug\hlhost.exe' `
  -GameDir '<valve-game-directory>'
```

## Known limitations

- Only one external client and one in-flight reliable server payload are
  supported. There is no general reliable queue.
- Only `svc_nop` / `clc_nop` payloads are accepted by this vertical slice.
- Fragment creation, fragment reassembly, split payload handling, file
  transfer, and endpoint migration are unsupported.
- `svc_nop` does not advance signon. There is no serverinfo, signon stage,
  resource list/download, consistency exchange, baseline, delta description,
  snapshot, packet-entity, clientdata, event, user-message, string-command,
  usercmd, movement, prediction, spawn, or gameplay support here.
- Steam authentication, SteamNetworking, master traffic, RCON, voice, uploads,
  `cstrike`, Metamod, AMX Mod X, and non-Windows socket backends are outside
  this milestone.
- An unmodified stock Half-Life client was **not tested**. No stock-client
  signon, resources, snapshots, movement, gameplay, full HLDS, or full GoldSrc
  interoperability claim is made.
