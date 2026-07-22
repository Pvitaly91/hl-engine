# GoldSrc resource-manifest signon slice

## Scope and compatibility claim

This vertical slice continues the protocol-48 `valve` signon from commit
`2a8175a8fa9dce7f2c33048254a9dd2a2f5124e8` on branch
`codex/goldsrc-resource-manifest-slice`. The preceding milestone has already
established one authoritative connected session, delivered the reliable
client `new` command exactly once, sent bounded serverinfo, and correlated its
reliable acknowledgement.

The slice accepts only the reference-observed next client request, builds one
bounded resource/precache manifest from authoritative host state, and sends it
through the existing single-payload reliable netchan. It stops when the
netchan acknowledgement covering that manifest is accepted. The client is not
put in the game, spawned, or made active.

This is an independently implemented compatibility slice. It is not a claim
of complete HLDS compatibility. The automated acceptance client is a separate
localhost UDP probe, not a stock Half-Life client.

## Public behavioral evidence

The selected behavior is supported by two independent public engine projects
and Valve's public HLSDK data contract. No source from those projects is
copied into this repository.

- Xash3D FWGS commit
  `f2166a2c9def5a613882a404d580b07418ff2065` sends the GoldSrc
  [`sendres` request at the end of serverinfo processing](https://github.com/FWGS/xash3d-fwgs/blob/f2166a2c9def5a613882a404d580b07418ff2065/engine/client/parse/cl_parse.c#L961-L969).
  Its
  [`CL_ServerCommand` implementation](https://github.com/FWGS/xash3d-fwgs/blob/f2166a2c9def5a613882a404d580b07418ff2065/engine/client/cl_main.c#L3159-L3180)
  shows that the `true` argument writes a `clc_stringcmd` into the reliable
  netchan message.
- ReHLDS commit `0124d56c3d888d922eb045775f71c6682ad1226f`
  provides the matching
  [`sendres` server handler](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1596-L1618)
  and the reference
  [resource-response sequence](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1210-L1260).
- Xash3D's public
  [`protocol.h`](https://github.com/FWGS/xash3d-fwgs/blob/f2166a2c9def5a613882a404d580b07418ff2065/engine/common/protocol.h#L35-L96)
  independently identifies the client string-command and server resource
  messages. Its
  [semantic resource-list decoder](https://github.com/FWGS/xash3d-fwgs/blob/f2166a2c9def5a613882a404d580b07418ff2065/engine/client/parse/cl_parse.c#L1823-L1849)
  corroborates the field meanings and the transition into local resource
  verification/downloading.
- Valve HLSDK commit `b1b5cf5892918535619b2937bb927e46cb097ba1`
  publishes the
  [`resourcetype_t`, resource flags, and `resource_t` contract](https://github.com/ValveSoftware/halflife/blob/b1b5cf5892918535619b2937bb927e46cb097ba1/engine/custom.h#L25-L79).
- ReHLDS and Xash3D independently construct the list in the same category
  order: [ReHLDS construction](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L5658-L5741)
  and [Xash3D construction](https://github.com/FWGS/xash3d-fwgs/blob/f2166a2c9def5a613882a404d580b07418ff2065/engine/server/sv_init.c#L376-L434).
- ReHLDS documents the 12-bit resource index/count domain in
  [`server.h`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/server.h#L86-L90),
  the no-consistency terminator in
  [`sv_user.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_user.cpp#L334-L380),
  and the bounded bit/string behavior in
  [`common.cpp`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/common.cpp#L396-L438).

These projects retain their own licenses. The links are evidence of behavior,
not a representation that this host is ReHLDS, Xash3D, or a historical Valve
engine build.

## Observed client request

The next request is one reliable `clc_stringcmd` whose complete NUL-terminated
text is lowercase `sendres`, with no arguments. Xash3D queues it immediately
after successfully parsing GoldSrc serverinfo. On the wire, the outgoing
client datagram may therefore both acknowledge the reliable serverinfo
carrier and carry the reliable `sendres` command. The server processes the
netchan acknowledgement before delivering that packet's application payload,
so the command is evaluated in the post-serverinfo phase.

The decoder is deliberately not a console-command interpreter. It produces a
typed resource-request event only when all of these conditions hold:

- the session endpoint and netchan sequence are valid;
- the reliable application payload is delivered by netchan;
- the signon phase is awaiting this request;
- the command is exactly `sendres` and is completely terminated;
- no arguments, separators, unsupported trailing command, or arbitrary text
  are present.

A lost acknowledgement may cause the reliable client payload to be sent
again. Netchan suppresses a duplicate outer sequence, and the signon state
suppresses a duplicate typed delivery under a later valid sequence. The
duplicate is acknowledged as required by transport but cannot regenerate,
requeue, or advance the manifest a second time.

## Explicit signon phases

Progress is represented by one typed per-session phase, not by a collection
of unrelated booleans.

| Phase | Entry condition | Permitted advancement |
|---|---|---|
| `serverinfo_acknowledged` | The Prompt 235 serverinfo reliable is correctly acknowledged. | Enters `awaiting_resource_request`. |
| `awaiting_resource_request` | No resource manifest is queued or pending. | The first valid typed `sendres` event builds and queues exactly one manifest. |
| `resource_manifest_queued` | A complete bounded manifest is frozen for this session. | Its first successful reliable carrier enters the sent phase. |
| `resource_manifest_sent_awaiting_ack` | The manifest has a recorded carrier sequence and reliable generation. | Only the correlated covering reliable acknowledgement advances. |
| `resource_manifest_acknowledged` | Netchan cleared the pending manifest reliable exactly once. | Terminal phase for this slice. |

Retransmission preserves `resource_manifest_sent_awaiting_ack`. A normal ACK
with the wrong reliable state, an ACK that does not cover the carrier, an
unsupported command, and a repeated request do not advance the phase.
Disconnect and authoritative slot reuse clear the phase, manifest bytes,
carrier metadata, and all delivery/generation/acknowledgement counters.

Every phase in this slice preserves `put_in_server=0`, `spawned=0`, and
`active=0`.

## Reference response and separate companion

The reference server response begins with a small resource-request companion,
may include a download-location message, and then carries the resource list.
This slice implements the always-present companion and list as separate typed
encoders, then concatenates their validated results into the one existing
reliable application payload.

The companion identifies the current map spawn generation and carries the
reference start index. The selected slice requires that start index to be
zero. Its purpose in the reference protocol is to ask for the client's
customization-resource list and to reject stale map generations. It remains a
separate encoder because it is a different protocol message with different
validation and because client upload processing is outside this milestone.
The deterministic probe advertises no client customization resources.

The optional download-location message is omitted. No FastDL URL is needed by
the minimal fixture, and file downloading is a non-goal. The resource-list
encoder finishes with the reference no-consistency marker. That marker is
present so a compatible decoder can terminate the list correctly; it does not
enable or perform consistency enforcement.

No other signon message is appended. In particular, the slice does not append
movevars, delta descriptions, baselines, view setup, signon-number changes, or
spawn data.

## Manifest data model and deterministic ordering

Each typed manifest entry contains:

| Property | Meaning and validation |
|---|---|
| resource type | One public GoldSrc resource category; the selected runtime flow uses generic, sound, model, decal, and event-script entries. |
| path | Canonical game-relative path with at most 63 visible bytes and no embedded terminator. |
| authoritative index | The original per-category precache index, limited to the 12-bit protocol domain. |
| download size | File size metadata only, limited to the unsigned 24-bit protocol domain. File contents are never included. |
| flags | Only the bounded three-bit resource flags represented by the selected flow. |
| custom identity | Optional 16-byte MD5 field represented by the codec contract; the production manifest builder for this slice does not create custom-upload entries. |
| extension data | Optional bounded reference extension field; the selected production flow leaves it absent. |

Entries are emitted in reference order: generic files, sounds, models, decals,
then event definitions. Within each category, ascending authoritative index is
the deterministic order. Duplicate ownership of a type/index pair is an
error; an entry is never silently renumbered.

Generic files and event definitions carry the reference
fatal-if-missing flag. Decal resource indices use the reference decal-table
offset, whose first wire index is zero; the other runtime precache categories
retain their one-based indices.

The loaded world BSP is required. It is represented as a model at model index
1 with its normalized `maps/...bsp` path. Valve's `t_world` is a fake API type;
the reference resource builders emit the BSP as `t_model`. Sound entries keep
the precache-relative sound name, while file-size lookup uses the corresponding
`sound/` path. Inline model names have no downloadable file body and therefore
carry size zero.

## Authoritative runtime sources

Production data is collected from the host state that the running Game DLL and
map initialization already populated:

- the loaded world context and model `PrecacheRegistry` provide the world and
  model paths and indices;
- `SoundPrecacheRegistry` provides sound paths and indices;
- the stable generic, decal, and event registries provide their paths and
  indices;
- the existing game-directory filesystem provides bounded file-size metadata;
- the current map spawn count provides the companion generation identity.

The builder takes a semantic snapshot once after accepting `sendres`. Encoding
uses that snapshot; retransmission never rereads files, registries, time, or
other mutable state. Missing world state, invalid indices, duplicate ownership,
or an out-of-range size fails the build before anything is queued. A missing
optional resource file is represented with zero size, matching the bounded
engine lookup fallback; no file content is read into the manifest.

Paths are normalized before model validation. Absolute paths, drive or colon
paths, empty segments, current-directory segments, parent traversal, control
bytes, and embedded terminators are rejected. Backslashes are canonicalized to
forward slashes and ASCII letters to lowercase before file-size lookup.
Normalization does not change a resource's authoritative index or category.

## Bounds and atomic failure

- A resource path is limited to 63 visible bytes plus its protocol terminator.
- A type/index pair is unique. Indices are limited to 0 through 4095.
- Download-size metadata is limited to 0 through 16,777,215 bytes.
- The protocol count field is 12 bits; this implementation applies the
  stricter compatibility cap of 1280 entries.
- The complete companion-plus-manifest reliable payload must fit the existing
  1200-byte reliable capacity.
- Every scalar and bit field is written explicitly. No native structure memory
  is serialized, no output byte is left uninitialized, and final padding is
  deterministic.

Validation and capacity checks are atomic. If the semantic list is legal but
cannot fit the current non-fragmented reliable capacity, the encoder returns
the typed `requires_fragmentation` result. It emits no partial manifest,
queues no reliable payload, leaves the signon phase consistent, and does not
increment the generation counter. That typed preparation outcome is cached for
the session, so a repeated reliable request reuses it without rebuilding or
changing signon state. Fragmentation is not added by this task.

## Reliable ACK correlation and retransmission

The combined response is queued through the existing one-payload reliable
channel with typed identity `resource_manifest`. The encoded bytes are frozen
until acknowledgement or session reset. Another application reliable cannot
overwrite them.

The first successful send records the outer carrier sequence and reliable
generation. An incoming packet clears the manifest only when its ordinary ACK
covers a real manifest carrier and its reliable acknowledgement state matches
that generation. A stale ACK, a future ACK, a non-covering ACK, or the wrong
reliable state cannot clear the bytes or advance signon.

When the existing strict resend condition becomes eligible, a later carrier
contains the identical frozen application bytes. Retransmission does not
rebuild the manifest or change its phase. The eventual correct ACK clears the
pending bytes, increments the manifest acknowledgement count once, and enters
`resource_manifest_acknowledged` once. There is no separate application-level
resource-manifest ACK message; this milestone's acknowledgement is the
correlated netchan reliable ACK.

## Test and proof fixture separation

Unit tests create a small synthetic semantic manifest independently of the
production runtime builder. Golden expectations and semantic-decoder checks
remain test data. Production source contains no proof-specific filenames,
captured packet bytes, proprietary game content, or fallback synthetic
manifest.

The external proof starts the real `hlhost` executable with legally available
local game data on `127.0.0.1`. Proof A passes an explicit repository-owned TSV
fixture so its small expected manifest stays deterministic and non-fragmented.
The proof decodes every entry semantically, validates count, order, indices,
world identity, flags, and absence of file content, then sends the correlated
netchan acknowledgement. Proof B separately omits that fixture and builds from
the authoritative runtime registries; the resulting production manifest
exercises the typed `requires_fragmentation` path without truncation.

## Stock-client status

`stock_client_tested=no`.

No reproducible local harness with an unmodified, legally obtained Half-Life
client was available for this milestone. The exact `sendres` selection is
therefore based on the two corroborating public behavioral implementations
above, plus Valve's public resource data contract and a deterministic external
localhost probe. The probe proves this repository's selected contract, but it
does not prove that an unmodified Steam Half-Life client accepts the manifest.

## Limitations and non-goals

- Only one authoritative external session and one pending server reliable
  payload are supported.
- Fragment creation/reassembly and multiple simultaneous reliable payloads are
  not implemented.
- Client resource uploads, custom spray propagation, resource downloads,
  FastDL, and consistency enforcement are not implemented.
- Delta descriptions, movevars, light styles, user messages, baselines,
  snapshots, packet entities, client data, user commands, prediction,
  movement, spawn, and gameplay are not implemented.
- The slice does not call Game DLL `ClientPutInServer` and does not make the
  client spawned or active.
- Steam authentication, secure mode, public-server interaction, master-server
  traffic, RCON, voice, `cstrike`, and plugin compatibility are outside scope.
- The feature remains opt-in; with it disabled, existing host, connectionless,
  netchan, and serverinfo behavior must remain unchanged.

Resource-manifest acknowledgement does not mean that resource download, consistency verification, delta descriptions, baselines, snapshots, spawn, or gameplay are complete.

No proprietary file, machine-specific absolute path, credential, generated
binary, raw capture, or embedded file body belongs in this document or its
fixtures.
