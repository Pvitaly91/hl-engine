# GoldSrc protocol-48 delta-description bootstrap

## Scope and selected contract

This document freezes the bounded protocol contract selected by Prompt 240.
It follows the stock-client boundary recorded in
[`goldsrc_stock_fragment_completion.md`](goldsrc_stock_fragment_completion.md):
the client completed resource-fragment reassembly but could not construct its
next user command because no `usercmd_t` delta description had been installed.
The semantic mismatch was
`application_payload_missing_usercmd_delta_description`.

The selected correction is the canonical protocol-48 bootstrap:

- load delta definitions from the selected runtime game directory;
- validate the complete required seven-table registry;
- encode every table in reference wire order;
- place the complete bundle in the same logical response as serverinfo;
- retain movevars, CD track, and setview immediately after the bundle;
- send the oversized logical response through the existing reliable fragment
  sender; and
- require ordinary correlated reliable acknowledgement before advancing
  signon.

This is a compatibility slice, not a complete signon or gameplay
implementation. The pinned repository baseline is
`f2cbaeab2f05c4780927ef70ad156f9d086e3452`, and the read-only Half-Life SDK
reference is
`b1b5cf5892918535619b2937bb927e46cb097ba1`.

## Runtime definition source

Production selects:

```text
<selected-game-directory>/delta.lst
```

The source category is `runtime_game_dir`. Path selection belongs to the
trusted runtime game-directory resolver; the platform-independent parser
receives bounded text and never opens a caller-controlled path.

The selected local `valve` runtime contains `delta.lst`, all seven required
tables, and a 15-field `usercmd_t` table. The installed definition is not
semantically interchangeable with the pinned public SDK copy:

- installed `clientdata_t` has 50 fields, including `iuser1` and `iuser2`,
  while the SDK copy has 48; and
- installed `entity_state_t.eflags` uses two significant bits, while the SDK
  copy uses one.

The local and SDK `usercmd_t` definitions agree. Production nevertheless loads
the selected runtime file rather than substituting the SDK copy or a
pre-generated packet. Repository tests use small synthetic definitions and do
not copy an installed game file, a complete proprietary definition, or a raw
network capture.

## Verified `delta.lst` grammar

A table declaration has one of these forms:

```text
table_name none
{
    DEFINE_DELTA(field_name, type_expression, significant_bits, premultiply),
    DEFINE_DELTA_POST(
        field_name,
        type_expression,
        significant_bits,
        premultiply,
        postmultiply
    )
}

table_name gamedll encoder_name
{
    ...
}

table_name clientdll encoder_name
{
    ...
}
```

The grammar permits whitespace, blank lines, `//` comments, array-suffixed
field names such as `origin[0]`, `|`-combined type flags, and an optional comma
after the closing parenthesis. The verified base types are `DT_BYTE`,
`DT_SHORT`, `DT_FLOAT`, `DT_INTEGER`, `DT_ANGLE`, `DT_TIMEWINDOW_8`,
`DT_TIMEWINDOW_BIG`, and `DT_STRING`; `DT_SIGNED` is the only verified
modifier. `DEFINE_DELTA` implies a post-multiply value of `1.0`.

The parser is a data parser only. It does not execute encoder names or
commands. It validates table and field names against authoritative typed
layout metadata, converts numeric tokens with overflow checks, requires finite
multipliers, permits exactly one base type plus supported modifiers, and
rejects malformed declarations atomically. Duplicate tables, duplicate fields,
unknown types, unknown layouts, invalid offsets or sizes, invalid bit counts,
overlong names, excessive input, excessive table or field counts, incomplete
declarations, and unsupported conditional encoders return stable typed
failures without mutating session or signon state.

The verified conditional-encoder allowlist is exact:

| Table | Required `gamedll` encoder |
|---|---|
| `entity_state_t` | `Entity_Encode` |
| `entity_state_player_t` | `Player_Encode` |
| `custom_entity_state_t` | `Custom_Encode` |

A known callback paired with the wrong table, an unknown callback, or a
conditional encoder on any other table is rejected. The parser records these
identities as bounded metadata; it does not call them.

## Canonical table set and order

The selected runtime registry contains 219 fields:

| File and registration order | Table | Runtime field count | Wire order |
|---:|---|---:|---:|
| 1 | `clientdata_t` | 50 | 7 |
| 2 | `entity_state_t` | 52 | 6 |
| 3 | `entity_state_player_t` | 49 | 5 |
| 4 | `custom_entity_state_t` | 19 | 4 |
| 5 | `usercmd_t` | 15 | 3 |
| 6 | `weapon_data_t` | 20 | 2 |
| 7 | `event_t` | 14 | 1 |

The reference server registers the tables in file order but inserts each
registration at the head of its registry. It writes that registry from head to
tail, producing this wire order:

```text
event_t
weapon_data_t
usercmd_t
custom_entity_state_t
entity_state_player_t
entity_state_t
clientdata_t
```

Within each table, the reference parser reverses its temporary linked list
before building the descriptor array, so field order remains the order in
`delta.lst`. Registration order is therefore not wire order. `movevars_t` is
not an eighth `svc_deltadescription` table in this GoldSrc bootstrap; movevars
retain their separate service message.

All seven tables are required before transmission. An immediately useful
`usercmd_t`-only response is rejected as an incomplete registry rather than
being treated as the canonical bundle.

## Service message and meta-delta wire model

`svc_deltadescription` is service opcode decimal `14`. Each table is encoded
independently:

1. one byte containing opcode 14;
2. the table name as an ordinary NUL-terminated byte string;
3. a fresh bitstream containing an unsigned 16-bit field count; and
4. one meta-delta record per field.

Each field descriptor is encoded as a delta from a zeroed descriptor. A record
starts with a three-bit count of following mask bytes. Mask bit `i` represents
meta field `i`; bits and integer values are written least-significant bit first.
The seven-field meta model needs at most one mask byte. Marked values follow in
ascending meta-field order:

| Meta index | Member | Meta type | Wire width and transform |
|---:|---|---|---|
| 0 | `fieldType` | unsigned `DT_INTEGER` | 32 bits |
| 1 | `fieldName` | `DT_STRING` | NUL-terminated 8-bit characters |
| 2 | `fieldOffset` | unsigned `DT_INTEGER` | 16 bits |
| 3 | `fieldSize` | unsigned `DT_INTEGER` | 8 bits; reference parsed descriptors use value 1 |
| 4 | `significant_bits` | unsigned `DT_INTEGER` | 8 bits |
| 5 | `premultiply` | unsigned `DT_FLOAT` | 32-bit integer value after multiplication by `4000.0` |
| 6 | `postmultiply` | unsigned `DT_FLOAT` | 32-bit integer value after multiplication by `4000.0` |

The two floating-point members are not serialized as host IEEE-754 memory.
For the verified non-negative runtime values, the encoder multiplies the
finite value by `4000.0` and truncates toward zero into an unsigned 32-bit wire
integer; it does not round or reinterpret a signed integer bit pattern. The
decoder reads that unsigned value and divides by `4000.0`. Negative values and
products outside the unsigned 32-bit range are rejected. Offsets are the
verified protocol-structure offsets supplied by typed layout metadata; native
C++ structure bytes are never serialized.

The GoldSrc wire type flags are:

| Flag | Value |
|---|---:|
| `DT_BYTE` | `0x00000001` |
| `DT_SHORT` | `0x00000002` |
| `DT_FLOAT` | `0x00000004` |
| `DT_INTEGER` | `0x00000008` |
| `DT_ANGLE` | `0x00000010` |
| `DT_TIMEWINDOW_8` | `0x00000020` |
| `DT_TIMEWINDOW_BIG` | `0x00000040` |
| `DT_STRING` | `0x00000080` |
| `DT_SIGNED` | `0x80000000` |

The host descriptor also contains runtime `flags` and statistics members, but
they are absent from the seven-field meta description and are not sent.
General signed delta values use the GoldSrc sign-bit-plus-magnitude operation;
the meta fields above are unsigned.

Ending a table's bitstream writes the ceiling number of bytes and clears
unused high bits in the final byte. There is no descriptor terminator, table
terminator, opaque tail, or host-alignment padding. The next service opcode
starts on a byte boundary.

## Bootstrap placement

The selected logical order is:

```text
svc_serverinfo
serverinfo trailing secure byte
svc_sendextrainfo
seven svc_deltadescription messages
svc_newmovevars
svc_cdtrack
svc_setview
```

This is `same_serverinfo_bundle`: the reference `SV_SendServerinfo` writes the
extra-info companion, delta bundle, movevars, CD track, and setview into one
logical `new` response. Physical fragmentation across multiple netchan packets
does not change that ordering or logical ownership.

The inseparable tail is encoded explicitly rather than copied from a native
`movevars_t`. `svc_newmovevars` is opcode 44 and carries, in order, 16
little-endian 32-bit floats (`gravity` through `waveHeight`), one footsteps
byte, eight further floats (`rollangle`, `rollspeed`, three sky-color values,
and three sky-vector values), and a NUL-terminated sky name. Opcode 32 then
carries the CD audio track twice, followed by opcode 5 and the one-based view
entity as a little-endian 16-bit value. The view entity is bounded to the
public GoldSrc `MAX_EDICTS` index domain of 1 through 899, every float must be
finite, and the sky name is bounded to 31 bytes. Runtime cvars supply available
values. The public names are `edgefriction` and `mp_footsteps`; the `sv_`
spellings in public reference source are C++ symbols, not alternate cvar
names. Reference defaults fill absent engine-owned cvars, including
`sv_rollspeed=0`, and an absent or empty `sv_skyname` uses `desert` without
changing the feature-off global cvar registry.

The resource response is later and separate. It begins only after the client's
resource request and contains `svc_resourcerequest` followed by
`svc_resourcelist`. Public references do not establish a separate late delta
payload as the canonical contract, so this implementation does not defer delta
registration until after serverinfo acknowledgement or resource transfer.

## Bounds, ownership, and transport

The parser, registry, meta encoder, and bundle encoder use typed records and
checked fixed-width arithmetic. Their selected production interfaces are
`GoldSrcDeltaLayoutRegistry`, `GoldSrcDeltaRegistry`, and
`GoldSrcDeltaParseResult`, with
`EncodeGoldSrcCanonicalDeltaBundle` and
`DecodeGoldSrcCanonicalDeltaBundle` providing the bounded semantic wire
boundary. The layout registry supplies verified names, offsets, and sizes; the
parsed registry owns validated runtime table and field values; parse and codec
results carry stable typed outcomes instead of partial state.

The fixed limits are:

| Limit | Value |
|---|---:|
| delta-definition input bytes | 65536 |
| parsed tables | 32 |
| fields per table | 56 |
| table or field name bytes, excluding NUL | 31 |
| canonical required tables | 7 |
| encoded canonical bundle bytes | 65536 |

Input text, table count, field count, names, numeric values, and output
capacity are checked before state mutation. The selected bundle must contain
exactly the seven required table identities once each, even though the bounded
parser can represent additional validated input tables.

Encoding is atomic and deterministic:

- a failure emits no partial bundle;
- unused output bits are zero;
- field values are encoded explicitly rather than by structure-memory copy;
- the complete logical payload is frozen before reliable transmission;
- retransmission uses byte-identical frozen data; and
- disconnect or authoritative slot reuse clears registry, bundle, reliable,
  fragment, and signon-owned state.

The bounded reliable GoldSrc `dropclient\n` string command uses the existing
authoritative dedicated lifecycle transition. The line feed is part of the
exact command identity; variants and the unrelated text `disconnect` are
rejected. A valid command clears the netchan, active fragment sender, signon
phase, serverinfo context, parsed registry, encoded bundle, combined bootstrap,
and resource response before the slot can be admitted again. The same-process
negative proof requires that reset to happen while a fragmented bootstrap is
still pending, then requires a distinct loopback endpoint to reuse the same
slot with pristine session-owned counters.

For the selected 219-field runtime registry, the delta bundle alone has a
conservative lower bound of 5762 bytes even when every nonzero field offset is
omitted from the estimate. This already exceeds the public reference's
1400-byte routeable packet limit and 3990-byte ordinary reliable-message limit.
A `usercmd_t`-only message is approximately 454 bytes, but it is not the
selected coherent bundle.

The full canonical bundle therefore reuses the existing bounded normal-stream
fragment sender and its 65536-byte frozen-payload limit. It does not create a
second UDP path, reliable queue, fragment format, client registry, or parallel
signon machine. Application completion still requires the correct covering
reliable acknowledgement for the final fragment; merely planning or emitting
all fragments cannot advance signon.

## Public protocol provenance

The implementation is independently written. These pinned public sources are
behavioral references:

- ReHLDS commit `0124d56c3d888d922eb045775f71c6682ad1226f`:
  [service opcodes](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/net.h#L150-L207),
  [delta types and descriptor model](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/delta.h#L33-L94),
  [meta-delta schema](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/delta.cpp#L70-L97),
  [mask generation](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/delta.cpp#L500-L620),
  [field representation](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/delta.cpp#L622-L765),
  [meta-record framing](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/delta.cpp#L838-L865),
  [`delta.lst` grammar](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/delta.cpp#L1183-L1521),
  [table writer](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1023-L1047),
  [bootstrap placement](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1096-L1202),
  [resource separation](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1210-L1259),
  [`new` response fragmentation](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1509-L1617),
  [registration order](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L8158-L8185),
  [movevar source values](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1048-L1077),
  [movevar wire order](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L991-L1021),
  and [the `dropclient` server command](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L8258-L8262).
- Xash3D FWGS commit `9f14c48787918a183e266df17c36bbf80092368e`
  independently corroborates
  [opcode 14](https://github.com/FWGS/xash3d-fwgs/blob/9f14c48787918a183e266df17c36bbf80092368e/engine/common/protocol.h#L32-L38),
  the [seven-field GoldSrc meta model](https://github.com/FWGS/xash3d-fwgs/blob/9f14c48787918a183e266df17c36bbf80092368e/engine/common/net_encode.c#L313-L455),
  [definition grammar](https://github.com/FWGS/xash3d-fwgs/blob/9f14c48787918a183e266df17c36bbf80092368e/engine/common/net_encode.c#L650-L845),
  [mask decoding](https://github.com/FWGS/xash3d-fwgs/blob/9f14c48787918a183e266df17c36bbf80092368e/engine/common/net_encode.c#L1456-L1517),
  [GoldSrc table decoding](https://github.com/FWGS/xash3d-fwgs/blob/9f14c48787918a183e266df17c36bbf80092368e/engine/common/net_encode.c#L2038-L2073),
  and [the GoldSrc `dropclient\n` wire command](https://github.com/FWGS/xash3d-fwgs/blob/9f14c48787918a183e266df17c36bbf80092368e/engine/client/cl_main.c#L1633-L1654).
- Valve's public Half-Life SDK commit
  `b1b5cf5892918535619b2937bb927e46cb097ba1` documents
  [runtime `delta.lst` placement](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/network/Delta.txt),
  the [public `usercmd_t` definition](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/network/delta.lst#L206-L223),
  and the [public command structure](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/common/usercmd.h#L21-L39).

These projects retain their own licenses. The references document behavior;
they do not make this host ReHLDS, Xash3D, or a historical Valve engine.

## Explicit non-goals

This slice does not implement user-command gameplay decoding, movement,
prediction, entity or instance baselines, snapshots, packet entities,
clientdata updates, events, resource downloads, full consistency enforcement,
Game DLL `ClientPutInServer`, spawn, weapons, damage, or gameplay. It also does
not implement Steam authentication, public-server or master-server traffic,
RCON, voice, `cstrike`, Metamod, or AMX Mod X compatibility.

Delivering the delta-description bootstrap does not mean that actual usercmd
processing, movement, entity baselines, snapshots, ClientPutInServer, spawn,
or gameplay are complete.
