# GoldSrc protocol-48 reliable fragmentation slice

## Scope and compatibility claim

This milestone adds one bounded server-to-client normal-stream fragment
transfer to the existing opt-in protocol-48 localhost signon. Its first
production consumer is a resource-manifest response whose complete encoded
application payload is larger than the existing 1200-byte non-fragmented
reliable capacity and no larger than 65536 bytes.

The implementation is transport-generic. The fragment sender sees an immutable
reliable payload and a typed payload kind; it does not interpret resource paths
or manifest fields. The platform-independent reassembler is used by unit tests
and the separate localhost probe. Production acceptance of fragmented
client-to-server application messages remains unsupported: valid fragment
metadata can be decoded for classification, but the authoritative server
netchan rejects it before sequence, ACK, activity-time, signon, or gameplay
state changes.

This is an independently implemented compatibility slice, not a claim of
complete HLDS compatibility. It implements the normal fragment stream only.
File transfer, compression, client uploads, multiple simultaneous fragment
streams, interleaving another application reliable, later signon, spawn, and
gameplay are outside scope.

## Public behavioral evidence

The wire and resend behavior were selected from two independent public engine
implementations. No source from either project is copied into this repository.

- ReHLDS commit `0124d56c3d888d922eb045775f71c6682ad1226f`
  writes the fragment flag and the two stream descriptors immediately after
  the eight-byte netchan header in
  [`net_chan.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_chan.cpp#L475-L503).
  It transforms the complete post-header region with the low sequence byte in
  the [same transmit path](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_chan.cpp#L532-L546).
- ReHLDS defines the server-to-client fragment bounds, reliable payload bound,
  `MAKE_FRAGID`, and its high/low 16-bit accessors in
  [`net.h`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net.h#L278-L308).
  Its fixed receiver validation bounds index/count, length, offset, packet
  range, and stream overlap in
  [`net_chan.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_chan.cpp#L628-L675).
- ReHLDS selects retransmission only when the peer ACK has advanced beyond the
  last reliable carrier and the reliable acknowledgement state still differs,
  as shown in
  [`net_chan.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net_chan.cpp#L295-L305).
  Its fragment queue copies the selected buffer into the reliable buffer, so
  that condition resends the current reliable fragment rather than a selective
  set.
- Xash3D FWGS commit `009855c193c951da7068af0cb2cc14817375efbc`
  independently defines the same packed fragment ID in
  [`net_chan.c`](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/net_chan.c#L20-L29),
  writes the same stream metadata in its
  [transmit path](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/net_chan.c#L1714-L1741),
  applies `COM_Munge2` after the header in that
  [same path](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/net_chan.c#L1785-L1797),
  and uses the same
  [retransmission condition](https://github.com/FWGS/xash3d-fwgs/blob/009855c193c951da7068af0cb2cc14817375efbc/engine/common/net_chan.c#L1524-L1535).

The reference implementations can compress normal messages and can combine
regular reliable bytes with fragment bytes. Compression and coexistence are
explicit non-goals here. This slice emits one frozen, uncompressed normal
fragment at a time and stalls another application reliable until completion.

## Exact selected wire shape

All integer fields are written little-endian with explicit byte operations.
Native C++ structures are never serialized.

| Offset | Width | Selected meaning |
|---|---:|---|
| 0 | 4 | Raw sequence word. Low 30 bits are the outer sequence, bit 30 says fragment metadata is present, and bit 31 says a reliable payload is present. |
| 4 | 4 | Raw acknowledgement word. Low 30 bits are the ordinary ACK, bit 30 is reserved/unsupported, and bit 31 is the peer's reliable acknowledgement state. |
| 8 | 1 | Normal-stream presence. This slice writes `1`. |
| 9 | 4 | Normal `fragid`: current fragment index in the high 16 bits, total fragment count in the low 16 bits. The current index is one-based. |
| 13 | 2 | Packet-local start position inside the reliable payload following all stream metadata. This slice writes zero. |
| 15 | 2 | Normal fragment payload length. |
| 17 | 1 | File-stream presence. This slice writes `0`. |
| 18 | variable | Exactly the selected frozen source range. |

The metadata is therefore 10 bytes for a normal-only fragment. There is no
distinct wire transfer ID and no declared full-payload-size field. The packed
`fragid` names one indexed buffer within a counted set; transfer ownership is
the authoritative session and its one-in-flight invariant. The sender and
reassembler also carry a caller-owned monotonically increasing transfer
generation, but that value is deliberately not serialized.

The complete region beginning at byte 8—metadata and application bytes—is
transformed by the existing GoldSrc Munge2 operation using the low byte of the
outer sequence. Only complete four-byte groups are transformed, matching the
existing codec; any trailing one to three bytes retain their encoded value.
The decoder reverses that exact range before parsing metadata.

The selected fragment payload capacity is:

```text
min(reference server-to-client maximum 1024,
    routeable limit 1400 - base header 8 - normal metadata 10)
= 1024 bytes
```

Thus a maximum fragment datagram is 1042 bytes and stays below the 1400-byte
routeable limit. A fragmented transfer is necessarily larger than 1200 bytes,
so even its short final packet is already large enough that the reference
minimum-datagram padding rule does not add application bytes. This
implementation emits no fragment padding.

## Bounds and deterministic planning

The sender uses fixed-capacity storage:

- one frozen application payload of at most 65536 bytes;
- one deterministic plan of at most 64 normal fragments;
- one current reliable fragment of at most 1024 bytes;
- one active transfer per authoritative session;
- no allocation or map keyed by wire-controlled identifiers.

Payloads from 1 through 1200 bytes retain the prior non-fragmented reliable
path. A payload from 1201 through 65536 bytes is copied once, then covered in
ascending 1024-byte source ranges. The final range may be shorter. Checked
addition and multiplication guard plan arithmetic. Empty payloads, payloads
over the configured total bound, count overflow, encoding failure, and a
second transfer while active return stable typed results without changing the
active plan, reliable state, or signon state.

The full payload is frozen before the first send. Source mutation after queueing
cannot change a fragment or retransmission. Every descriptor is initialized,
every source byte belongs to exactly one plan entry, and unused output storage
is zeroed.

The typed sender phase is one of:

| Phase | Meaning |
|---|---|
| `none` | No transfer has been planned for the session. |
| `planned` | Frozen bytes and all descriptors are ready; no fragment is staged. |
| `sending` | A prior fragment was acknowledged and the next one is ready to stage. |
| `sent_awaiting_ack` | The current fragment is frozen in the reliable channel. |
| `completed` | Every fragment received the required reliable acknowledgement; the frozen full payload has been cleared. |
| `failed` | Timeout or internal staging failure cleared all payload bytes. |

Stable counters distinguish transfers planned/completed/expired and fragments
sent/resent/acknowledged. A session reset intentionally clears those counters
with all other fragment state.

## ACK, retransmission, and application completion

Only one fragment is staged in the existing reliable buffer at a time. Staging
a new fragment toggles the session reliable sequence exactly once. An ordinary
outer packet is still sent when a covering ACK has the wrong reliable state;
once a newer acknowledged server sequence exists with the mismatch still
present, the reference resend condition retransmits the current fragment.

Retransmission:

- uses a new outer sequence;
- preserves the current reliable state;
- preserves the packed fragment ID, length, and decoded source bytes;
- does not rebuild the application payload or plan;
- does not advance the fragment index or signon phase.

A correct reliable ACK for an early fragment clears only that current reliable
buffer and moves the sender to the next planned fragment. The application
payload kind is not reported acknowledged at that point. Only the correct
reliable ACK for the final fragment produces the one application-level
`resource_manifest` acknowledgement, clears pending transport bytes, and
advances signon once to `resource_manifest_acknowledged`.

Future, stale, non-covering, and wrong-state ACKs cannot complete the transfer.
Rejected traffic does not update accepted activity or fragment progress time.
There is no selective fragment ACK protocol and no whole-set resend invented
by this implementation.

## Bounded reassembler

The platform-independent reassembler owns one fixed assembly:

- configured total-payload, fragment-count, and per-fragment limits;
- at most 64 fixed 1024-byte fragment slots;
- one fixed 65536-byte completed output;
- one caller-supplied transfer generation;
- creation and last-valid-progress timestamps.

The wire has no full-transfer-size field. Consistency therefore means that the
session transfer generation and advertised fragment count remain unchanged,
each one-based index is valid, each packet-local offset/length stays inside the
decoded packet payload, and the sum of unique fragment lengths stays inside the
configured total bound. Completion occurs only when every advertised index is
present. Output is the exact concatenation in fragment-index order.

The receiver accepts supported out-of-order indices, consistent with the
reference receiver's indexed insertion/completion model. An exact repeated
index with identical length and bytes returns `duplicate` and does not change
the received count or output. A changed length is an overlap conflict; changed
bytes are a conflicting duplicate. Transfer mismatch, count inconsistency,
bad offset/length, unsupported file stream, total overflow, and malformed
metadata do not partially advance the assembly.

An incomplete assembly expires after 30 seconds without valid progress and
resets all slots and output. Explicit reset after success or failure produces a
clean reusable object.

## Runtime ownership, timeout, and reset

The authoritative player slot owns the netchan, and the netchan owns the one
fragment sender. The host network pump retains its existing receive and send
budgets. Each valid fragment ACK schedules at most one next fragment datagram,
so no unbounded send loop or proof-only gameplay frame pump exists.

The runtime checks fragment timeout from the host frame clock. Timeout clears
the current reliable buffer, fails the transfer, records the typed reason, and
terminates the opt-in proof path without putting the client in the game. An
authoritative disconnect calls the existing netchan reset. Slot reuse therefore
starts with no frozen bytes, descriptors, transfer generation, sequence
history, timers, counters, or active phase.

Unknown endpoints never reach a session fragment state. Valid client-to-server
fragment packets remain rejected before sequence/ACK mutation. Malformed
fragment metadata is rejected by the codec before netchan processing.

## Resource manifest integration

The resource companion and typed manifest are still built and encoded exactly
once. A small response keeps the Prompt 236 non-fragmented wire path unchanged.
A large legal response is queued with payload kind `resource_manifest`; the
transport decides whether to fragment it. Resource ordering, indices, world
model identity, signon phase, and application bytes are unaffected.

The deterministic large fixture is legal manifest data, not proof padding. Its
encoded companion-plus-list payload requires three fragments and remains below
65536 bytes. A valid manifest whose complete response exceeds 65536 bytes
returns an atomic typed `payload_too_large`/`requires_fragmentation` outcome:
no transfer is created, no partial fragment is emitted, no generation is
recorded, and a repeated request reuses the cached failure.

The legacy Prompt 236 negative proof intentionally retains its earlier
unfragmented-capacity scenario when it requests the negative mode without an
explicit fixture. That proof-only selection continues to demonstrate the old
typed 1200-byte rejection contract. Normal production integration and the new
explicit-fixture proof use the verified 65536-byte transfer bound.

## Tests and external proofs

`goldsrc_fragmentation_tests` covers exact metadata and datagram vectors,
truncation at every field boundary, byte order, flags, reserved bits, bounds,
transformation range, deterministic zeroed output, planner coverage and
overflow, busy-state immutability, ordered/out-of-order reassembly, exact and
conflicting duplicates, missing first/middle/final fragments, identity/count
mismatch, offset/length/total/count limits, timeout/reset, frozen bytes,
wrong/future ACK stability, byte-identical resend, final-only application
completion, session reset/reuse, and outer-sequence wrap.

`scripts/run_goldsrc_fragmented_manifest_proof.ps1` starts the real Win32
`hlhost`, uses a separate UDP socket, and binds only to `127.0.0.1`.

- Proof A completes handshake, netchan establishment, `new`, serverinfo ACK,
  and `sendres`; validates every fragment and routeable bound; reassembles and
  semantically decodes the exact fixture; acknowledges all fragments; and
  verifies final connected-but-not-spawned state and clean process/artifact
  cleanup.
- Proof B omits first, middle, and final fragments from the probe's first
  assembly attempt; creates the reference resend condition; verifies stable
  identity/state and byte-identical decoded retransmissions; suppresses an
  observed exact duplicate; proves wrong and future ACKs leave the transfer
  pending; completes exactly once; runs a separate valid oversized fixture
  rejection; and executes the external platform-independent reset/reuse helper.

No raw packet bytes, proprietary file contents, or unbounded data are written
to the repository or final proof summaries.

## Stock-client status and non-goals

`stock_client_tested=no`.

No reproducible harness with an unmodified, legally obtained Half-Life client
was available. The exact wire selection is supported by the two public
implementations above and by deterministic external UDP proofs, but stock
client interoperability remains unverified.

This milestone does not implement file fragments, resource download,
compression/decompression, custom sprays, consistency enforcement, multiple
simultaneous streams or large reliable payloads, fragmented client
application messages, delta descriptions, baselines, snapshots, user
commands, movement, spawn, or gameplay. Steam authentication, public/LAN
serving, master-server traffic, RCON, voice, `cstrike`, and plugin compatibility
remain outside scope.

Fragmented resource-manifest acknowledgement does not mean that resource download, consistency verification, delta descriptions, baselines, snapshots, spawn, or gameplay are complete.
