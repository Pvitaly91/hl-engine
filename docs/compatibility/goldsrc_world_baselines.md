# GoldSrc world/entity baseline bootstrap

## Frozen Prompt 242 contract

This contract was frozen before the Prompt 242 source implementation. It is
limited to the stock-compatible pre-spawn continuation immediately after the
Prompt 241 `clc_move` checkpoint.

The unchanged Half-Life 1.1.2.2 Steam build 15961492 client was rerun against
the Prompt 241 baseline on `127.0.0.1`, with installed `valve` data and
`c0a0`. It completed the protocol-48 delta and resource transfers, delivered
one checksum-valid `clc_move`, and stopped at
`awaiting_server_baseline_or_snapshot`. The client executable and installed
`delta.lst` hashes were unchanged after the bounded run.

Pinned public ReHLDS and Xash3D protocol behavior classifies the missing
continuation as **combined**, not a first snapshot:

1. `svc_spawnbaseline` carries the frozen world/entity baseline bundle;
2. `svc_signonnum 1` is the immediately adjacent signon advance;
3. the next client-side boundary is the reliable `sendents` request.

No `svc_packetentities`, `svc_deltapacketentities`, clientdata, movement
simulation, `ClientPutInServer`, active/spawned player state, or gameplay is
part of this slice.

## Authoritative baseline source

The production builder uses the loaded runtime map and the engine-owned
`EdictStore`, `PrecacheRegistry`, and world-model context. It must not invent
fixture entities for the production path.

The selected entity set and order match the stock server baseline rule:

- entity zero, the world;
- every reserved player slot, even before player activation;
- every remaining non-free edict with a non-zero model index;
- ascending edict index, with no duplicate index.

For each selected edict the engine calls the Game DLL
`pfnCreateBaseline` once. Player entries use the player delta table, beam
custom entities use the custom delta table, and other entries use the normal
entity delta table. `pfnCreateInstancedBaselines` is called once while the
map-level bundle is built. The resulting immutable bundle is reused for
retransmission and per-client delivery; callbacks are not repeated for
network retries.

The bounded representation rejects an entity index outside the protocol-48
11-bit range, an unsupported baseline kind, duplicate entity indices, an
oversized instance count, a missing delta table, or an output overflow.

## Wire contract

The bundle is encoded through the existing protocol-48 bit writer and the
runtime `delta.lst` registry. It begins with `svc_spawnbaseline`. Each entity
record contains its 11-bit entity index, two-bit entity type, and a delta
from the zero state using the selected table. Entity records end with the
16-bit `0xffff` marker. A six-bit instanced-baseline count and any instance
deltas follow. The message is byte padded only after its final bit.

`svc_signonnum 1` follows the complete baseline message in the same immutable
reliable bundle. Existing netchan fragmentation carries the bundle when it
exceeds one datagram; no baseline-specific fragmentation format is added.

## Signon and acknowledgement contract

The new bounded states are:

`awaiting_baseline_bootstrap` → `baseline_bootstrap_queued` →
`baseline_bootstrap_sent_awaiting_ack` →
`baseline_bootstrap_acknowledged` → `awaiting_first_snapshot`.

Only the current session may acknowledge its current reliable generation.
Duplicate client input must not regenerate the bundle, duplicate delivery,
or repeat Game DLL baseline callbacks. Wrong-phase, stale, future, malformed,
and overflow input is rejected without changing lifecycle state. Disconnect
and slot reset discard all per-session delivery state; the immutable map
bundle remains reusable by a fresh session.

Receipt of the subsequent typed `sendents` request proves that the stock
client accepted the baseline and signon advance. The server records that
request and stops at `awaiting_first_snapshot`; it does not fabricate the
snapshot requested by the client.

## Proof boundary

Proof A must exercise the production runtime map source and verify exact
entity selection, callback count, deterministic bytes, message ordering,
fragment completion when required, one valid acknowledgement, one typed
`sendents` delivery, inactive gameplay state, and clean shutdown.

Proof B must cover duplicate/out-of-order/stale/future acknowledgement,
malformed and overflow model input, deterministic reuse, reset and fresh-slot
reuse, no callback repetition, preserved inactivity, responsiveness, and
clean shutdown.

The mandatory final stock-client run must receive and acknowledge the real
runtime baseline bundle, advance beyond
`server_baseline_or_snapshot_required`, and reach
`first_snapshot_required`.

## Verified stock-client adjacency

The final bounded localhost run used the unchanged Half-Life 1.1.2.2 Steam
build 15961492 client, the installed `valve` runtime, `c0a0`, and the runtime
`delta.lst`. The host built 15 baselines from the authoritative map state:
world entity zero, one reserved inactive player slot, and 13 remaining
modeled edicts. `pfnCreateBaseline` ran once per entity and
`pfnCreateInstancedBaselines` ran once; the map produced no instanced
baselines.

The 525-byte `svc_spawnbaseline` plus `svc_signonnum 1` bundle fit in one
reliable carrier. The stock client acknowledged that carrier and then sent
the reliable `sendents` string command. Stock netchan composition placed a
checksum-valid unreliable `clc_move` immediately after `sendents` in the same
application payload. The dispatcher therefore preserves the signon decoder's
strict whole-payload rejection, exposes the exact typed command boundary, and
accepts the composition only when the existing `usercmd_t` decoder validates
the complete trailing move. No generic trailing command execution is added.

The accepted `sendents` request resolved
`server_baseline_or_snapshot_required` and established the next boundary as
`first_snapshot_required`. The host stopped there with
`put_in_server=0`, `spawned=0`, and `active=0`. Client and installed
`delta.lst` hashes were identical before and after the run.

Accepting the world/entity baseline bootstrap does not mean that the first
live snapshot, packet entities, clientdata, ClientPutInServer, spawn,
movement, or gameplay are complete.
