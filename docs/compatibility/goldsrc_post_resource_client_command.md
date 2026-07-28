# GoldSrc post-resource client command

## Scope and evidence

This compatibility slice starts at the Prompt 240 terminal boundary:
`resource_manifest_acknowledged`. It identifies and validates the first
unreliable application command sent by the unmodified Half-Life 1.1.2.2 Steam
build 15961492 client. It does not implement movement, entity baselines,
snapshots, `ClientPutInServer`, spawn, or gameplay.

A bounded pre-implementation loopback observation on `127.0.0.1` established
the following facts:

- the first command is opcode `2`, `clc_move`;
- it is carried in the unreliable portion of an accepted netchan sequence;
- the first observed command occupied 14 application bytes and was not batched
  with another application command;
- its decoded envelope contained zero-percent packet loss, two backup
  commands, one new command, and three `usercmd_t` deltas;
- the stock client repeated fresh `clc_move` commands while waiting for the
  server to continue signon;
- the client did not become spawned or active.

The diagnostic capture was one-shot, was removed before implementation, and
was not retained as a packet dump. The repository regression fixture contains
only the minimum deterministic bytes needed to verify the typed codec.

The contract was cross-checked against the public
[ReHLDS `SV_ParseMove`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_user.cpp)
receive path and the public
[Xash3D `CL_WritePacket`](https://github.com/FWGS/xash3d-fwgs/blob/f2166a2c9def5a613882a404d580b07418ff2065/engine/client/cl_main.c)
and
[`Delta_ReadGSFields`](https://github.com/FWGS/xash3d-fwgs/blob/f2166a2c9def5a613882a404d580b07418ff2065/engine/common/net_encode.c)
behavior.

## Frozen wire contract

`clc_move` is a byte-delimited application command:

1. one-byte opcode (`2`);
2. one-byte protected-body length;
3. one sequence-derived checksum byte;
4. the declared protected body;
5. zero or more separately parsed application commands.

The protected body is transformed with the GoldSrc `COM_Munge` table and the
accepted outer netchan sequence. Only complete four-byte groups are
transformed. After reversal, the protected body contains:

1. packet-loss and voice-loopback flags;
2. backup-command count;
3. new-command count;
4. `backup + new` delta-compressed `usercmd_t` values, oldest first.

The packet-loss value is the low seven bits and is bounded to `0..100`; the
high bit is the voice-loopback flag. The total command count is bounded to
`0..62`, matching the reference server's 64-entry command array guard. The
declared protected body is bounded by its byte-sized length and the enclosing
netchan payload.

The integrity byte is the low byte of the GoldSrc sequence CRC-32 calculation.
It covers at most the first 60 bytes of the untransformed protected body plus
four sequence-selected bytes from the standard CRC-32 table. The selection
offset is `outer_sequence % 0x3fc`. A mismatch rejects the entire application
payload before command state changes.

Each user command uses the delivered runtime `usercmd_t` delta schema. Each
delta starts at a byte boundary with a three-bit mask-byte count, followed by
that many least-significant-bit-first mask bytes and the values for changed
fields. GoldSrc pads each individual `usercmd_t` to the next byte before the
following command begins. Unchanged fields inherit from the preceding command;
the first command inherits a deterministic all-zero value. The decoder:

- requires the canonical `usercmd_t` schema;
- bounds the mask to the schema's field count;
- supports the declared byte, short, integer, float, angle, time-window, and
  string field encodings;
- applies signedness, bit width, premultiply, and postmultiply metadata;
- requires zero per-command alignment padding and no trailing whole bytes;
- stores platform-independent typed values rather than receive-buffer
  pointers or serialized host structures;
- requires unused bits in the declared command body to be zero.

`clc_move` does not itself name a delta snapshot frame. A separately batched
`clc_delta` command would be a different application command and is outside
this slice.

## Dispatch and delivery policy

The application dispatcher consumes the full unreliable payload
sequentially. It accepts `clc_nop` before, between, or after verified
commands, accepts one `clc_move`, and rejects:

- an unknown opcode;
- a second move command in the same payload;
- a truncated envelope or delta bitstream;
- an invalid count, mask, field encoding, checksum, or phase;
- unsupported trailing data.

The netchan accepts transport sequence and acknowledgement state before
application dispatch. Consequently, malformed application data does not undo
an already valid transport acknowledgement and does not clear unrelated
pending reliable server data. Duplicate and out-of-order outer sequences are
suppressed by the existing netchan before application delivery.

The command is legal as pre-spawn input only after the resource manifest is
acknowledged. The state transition is:

`resource_manifest_acknowledged`
→ `awaiting_post_resource_command`
→ `awaiting_server_baseline_or_snapshot`.

The first valid command is decoded, checksum-validated, recorded, and treated
as pre-spawn keepalive input. Later valid moves remain decoded keepalives but
do not advance signon again. No decoded button, angle, movement, or impulse
value is executed or copied into gameplay state.

Reference server behavior decodes the command container but returns before
movement execution when the client is neither spawned nor active. Therefore
there is no command-specific immediate response in this slice. The existing
ordinary netchan acknowledgement is retained. The next concrete boundary is:

`server_baseline_or_snapshot_required`.

At that boundary:

`put_in_server=0`, `spawned=0`, and `active=0`.

Decoding and accepting the first post-resource client command does not mean
that movement, entity baselines, snapshots, `ClientPutInServer`, spawn, or
gameplay are complete.
