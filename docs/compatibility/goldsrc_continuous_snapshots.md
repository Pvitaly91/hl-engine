# GoldSrc continuous snapshots

## Scope and evidence

This note records the protocol contract used by the Prompt 244 implementation.
It was written before the implementation was started.  The evidence order was:

1. Half-Life 1.1.2.2 (Steam build 15961492) connected to the Prompt 243 host on
   `127.0.0.1`;
2. public ReHLDS behavior at commit
   `0124d56c3d888d922eb045775f71c6682ad1226f`;
3. public Xash3D behavior at commit
   `009855c193c951da7068af0cb2cc14817375efbc`;
4. the repository-owned delta tables and deterministic fixtures.

The pre-change stock-client observation accepted the first full snapshot,
repeatedly sent `clc_delta` for that frame, continued sending `clc_move`, and
remained connected while waiting for a newer frame.  It therefore reproduced
`continuous_snapshot_cadence_required`.  No adjacent lifecycle transition was
required merely to reach this boundary.

## Verified cadence contract

Snapshots are application payloads carried by the normal unreliable netchan
stream.  They are not signon-reliable data and a lost snapshot is not
retransmitted.  A later snapshot is generated instead.  The milestone uses a
bounded 20 Hz rate (50 ms interval), within the reference server's bounded
10--30 updates-per-second policy.  Scheduling is driven by monotonically
advancing host server time, emits at most one snapshot per network pump, and
does not accumulate an unbounded catch-up burst.

Every continuous bundle has this order:

1. `svc_time` with the current server time;
2. `svc_clientdata`;
3. either `svc_packetentities` for a full frame or
   `svc_deltapacketentities` for a delta frame.

`svc_clientdata` is present in every frame.  With a valid acknowledged base it
contains a one-bit previous-frame marker, the base frame's low eight bits, and
a delta from that frame's semantic clientdata.  An unchanged clientdata record
therefore legally has an empty field mask.  A full fallback uses the zero
clientdata baseline.

## Frame references and history

The carrier netchan sequence is the full server-frame identifier.  It uses the
existing 30-bit modular sequence space and advances once for each successfully
built carrier.  The client exposes only its low eight bits in `clc_delta`.

Resolution searches the session's sent-frame history, not a global counter.
The history remains bounded at 64 frames, which is below the 256-value low-byte
generation period.  A reference may resolve only to a sent candidate in that
history.  The newest non-future candidate is selected; multiple candidates are
ambiguous and rejected.  A duplicate is idempotent.  A result that would move
the acknowledged base backwards is stale and rejected.  Unknown, future,
ambiguous, stale, and evicted references do not mutate the selected base or
session activity.  All ordering uses explicit 30-bit modular arithmetic, so
both low-eight and full-identifier wrap are defined.

The newest valid client-acknowledged history frame is the only delta source.
The newest sent but unacknowledged frame is never selected.  No acknowledged
frame, an evicted frame, or an invalid semantic frame causes a full-snapshot
fallback.  A rejected client reference never becomes valid merely because a
full fallback was sent.

## Delta packet entities

`svc_deltapacketentities` carries:

- the complete current semantic entity count;
- the acknowledged base frame's low-eight identifier;
- strictly ascending entity operations;
- a zero 16-bit entity-number end marker.

The entity lists are compared with a bounded linear merge:

- only in the current frame: Add, encoded from the established entity
  baseline with a forced delta record;
- in both frames and semantically changed: Update, encoded from the base
  frame's entity state;
- only in the base frame: Remove;
- unchanged: omitted from the wire delta and copied from the base during
  reconstruction.

The entity-number header follows the delta form: a removal bit, followed by
either a six-bit positive delta from the previous encoded entity number or an
11-bit absolute number.  Non-removals then carry the custom-entity bit and,
when instance baselines exist, the instance-baseline selector.  Normal
entities use `entity_state_t`, player entities use `entity_state_player_t`,
and custom entities use `custom_entity_state_t`.  An incompatible table-kind
change is rejected rather than silently reinterpreted.

Applying the ordered operations to the acknowledged frame always reconstructs
the complete current entity set and its transmitted entity count.  An empty
operation stream is valid for a static world.

## Loss, fallback, and lifecycle rules

If frame N is acknowledged and N+1 is lost, N+2 is still built from N.  The
same rule handles several lost frames.  Once a later valid frame is
acknowledged it becomes the next base.  History eviction removes old semantic
frames; if the selected base is no longer present, the next frame is full.
Reliable signon state is independent and is not reset by snapshot loss.

Disconnect disables scheduling and clears frame history and acknowledgement
state.  Reusing the slot begins with a fresh scheduler.  With the feature
disabled no continuous scheduler is entered.

The current visibility policy remains
`runtime_map_modeled_nonplayer_baselines_no_pvs`.  The pipeline does not
fabricate a player entity and retains:

    put_in_server=0
    spawned=0
    active=0

These findings verify the continuous snapshot, delta packet entities, frame
reference resolution, and loss-recovery contracts used by this milestone.

Stable continuous full and delta snapshots do not mean that Game DLL
ClientPutInServer, player spawn, PM_Move, prediction, player replication,
weapons, damage, or gameplay are complete.
