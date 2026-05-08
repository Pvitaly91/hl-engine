# HL-CL-20260504-299 Netchan Message-Writing Static Inventory

Prompt ID: HL-CL-20260504-299-dedicated-goldsrc-hlds-netchan-message-writing-static-inventory

Compatibility claim level: diagnostic-netchan-message-writing-static-inventory-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

This inventory is static and report-only. It does not run runtime probes, does not capture packets, does not open sockets, does not invoke Steam, does not invoke a real client binary, does not run getchallenge/connect/post-connect/signon paths, and does not implement a post-connect or signon-time serverinfo builder/parser.

Unknown byte-level behavior remains unknown. The closed query/info boundary remains limited to fixture-backed connectionless query/info and must not be reused as post-connect serverinfo, signon-time serverinfo, netchan evidence, real HLDS compatibility evidence, or real Steam Half-Life client compatibility evidence.

## Scan Scope

Primary source scan scope:

- `include/`
- `src/`

Context-only scan scope:

- `fixtures/diagnostic/hlds/serverinfo/`
- `fixtures/diagnostic/hlds/query_info_regression/`
- `docs/diagnostic/hlds/`
- `scripts/run_hlds_query_info_regression.ps1`
- prompt 293, 296, 297, and 298 artifacts

Static source counts from the focused message/netchan pattern set:

| Category | Matches | Files | Notes |
| --- | ---: | ---: | --- |
| message writer tokens | 92 | 3 | `WriteByte`, `WriteChar`, `WriteShort`, `WriteLong`, `WriteString`, `WriteCoord`, `WriteAngle`, `SZ_Write`, `MSG_Write`, and adjacent terms. |
| message reader tokens | 0 | 0 | No `MSG_Read` helper family found in `include/` or `src/`. Parser-like diagnostics exist for query/info only. |
| netchan tokens | 246 | 2 | Mostly summary/blocker/report fields such as `netchan_not_started`; no real `netchan_t`-style implementation found. |
| reliable tokens | 46 | 3 | Mostly blocker/report fields and message destination naming, not a reliable-channel implementation. |
| unreliable tokens | 2 | 1 | Message destination naming only; no unreliable datagram envelope contract found. |
| buffer/message/packet tokens | 68345 | 45 | Broad and noisy because `message` is a common artifact/report term; source inspection narrows the real helpers below. |
| overflow policy tokens | 2 | 1 | No central real message overflow policy found. |

## Message-Writing Helper Inventory

| Helper | File | Category | Writes or Reads | Level | Visible width/policy | Ownership and overflow behavior | Stage usage | Status | Safe future use | Confidence | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `FrameMessageBuffer::Begin` / `End` / `Abort` | `src/game_api/server_frame_loop.cpp`, `src/game_api/server_frame_loop.h` | diagnostic observation buffer | writes metadata | text summary plus payload byte count | records destination, message type, origin, entity, completion/abort | owns an active in-memory record; keeps at most 32 completed records; no wire buffer | engine callback observation | diagnostic-only | partial | high | Useful for future diagnostic observation, not a wire builder. |
| `FrameMessageBuffer::WriteByte` | `src/game_api/server_frame_loop.cpp` | message write recorder | write | byte-count only | records 1 payload byte; value kept as text | if no active message, write is ignored; no range or overflow flag | engine callback observation | diagnostic-only | partial | high | Does not serialize a byte stream. |
| `FrameMessageBuffer::WriteChar` | `src/game_api/server_frame_loop.cpp` | message write recorder | write | byte-count only | records 1 payload byte | same as above | engine callback observation | diagnostic-only | partial | high | Width is visible; signedness and wire byte policy are not proven. |
| `FrameMessageBuffer::WriteShort` | `src/game_api/server_frame_loop.cpp` | message write recorder | write | byte-count only | records 2 payload bytes | same as above | engine callback observation | diagnostic-only | partial | high | Endian policy is not recorded. |
| `FrameMessageBuffer::WriteLong` | `src/game_api/server_frame_loop.cpp` | message write recorder | write | byte-count only | records 4 payload bytes | same as above | engine callback observation | diagnostic-only | partial | high | Endian policy is not recorded. |
| `FrameMessageBuffer::WriteAngle` / `WriteCoord` | `src/game_api/server_frame_loop.cpp` | message write recorder | write | byte-count only | records 4 payload bytes each | same as above | engine callback observation | diagnostic-only | partial | high | Float/coord conversion policy is not a byte-level contract. |
| `FrameMessageBuffer::WriteString` | `src/game_api/server_frame_loop.cpp` | message write recorder | write | text plus byte count | records `value.size() + 1` | no max string length or sanitization in the recorder | engine callback observation | diagnostic-only | partial | high | Captures a null-terminated size assumption, but not encoding beyond local text. |
| `FrameMessageBuffer::WriteEntity` | `src/game_api/server_frame_loop.cpp` | message write recorder | write | byte-count only | records 4 payload bytes | same as above | engine callback observation | diagnostic-only | partial | high | Entity wire encoding remains unknown. |
| `FrameMessageBuffer::CompletedPreview` / `LastCompletedSince` / `Summarize` | `src/game_api/server_frame_loop.cpp` | safe preview/reporting | read diagnostic records | text-level | summary string only | bounded by completed record deque | diagnostics | diagnostic-only | yes for reporting | high | Good for future report-only checks; not a packet parser. |
| `StubMessageBegin` / `StubMessageEnd` | `src/game_api/hl_server_module.cpp` | engine function table shim | writes diagnostic message events | callback metadata | connects game DLL message callbacks into `FrameMessageBuffer` | in-memory observation only | engine callback bootstrap | diagnostic-only | partial | high | Does not start netchan or deliver a real client message. |
| `StubWriteByte` / `StubWriteChar` / `StubWriteShort` / `StubWriteLong` | `src/game_api/hl_server_module.cpp` | engine function table shim | write | callback recorder | delegates to `FrameMessageBuffer` and records callbacks | no central wire buffer | engine callback bootstrap | diagnostic-only | partial | high | Width is visible through the frame recorder; byte order is not. |
| `StubWriteAngle` / `StubWriteCoord` / `StubWriteString` / `StubWriteEntity` | `src/game_api/hl_server_module.cpp` | engine function table shim | write | callback recorder | string null pointer is converted to empty local string | no wire buffer; no network send | engine callback bootstrap | diagnostic-only | partial | high | Useful for observing game DLL intent, not for real network encoding. |
| `ResolveUserMessageId` / `TryParseMessageWriteValue` / `BuildObservedMessageCallbackPath` / `BuildObservedPayloadWrites` | `src/game_api/hl_server_module.cpp` | diagnostic message analysis | reads diagnostic summaries | text-level | parses recorded callback summaries | no byte stream | message callback diagnostics | diagnostic-only | yes for reports | high | Supports observed message reporting such as screen fade/text message diagnostics. |
| `AppendGoldSrcCString` / `ReadGoldSrcCString` | `src/game_api/hl_server_module.cpp` | query/info string helper | write/read | byte-level C string | appends bytes followed by `0x00`; reader scans until terminator | vector-owned bytes; no general overflow policy | connectionless query/info only | diagnostic-only | yes for query/info only | high | This belongs to the closed connectionless query/info family, not post-connect or signon serverinfo. |
| `BuildGoldSrcInfoRequest` / `IsGoldSrcInfoRequest` / `BuildGoldSrcInfoResponse` / `ParseGoldSrcInfoResponse` | `src/game_api/hl_server_module.cpp` | connectionless query/info helper | write/read | byte-level query/info | uses marker/header, opcode/tag, C strings, and bounded byte fields | vector-owned bytes; query/info-specific validation | connectionless query/info | diagnostic-only | yes for query/info only | high | Already covered by the query/info regression boundary. |
| `BuildConnectionlessTextPacket` / `ExtractConnectionlessText` | `src/game_api/hl_server_module.cpp` | connectionless text helper | write/read | text packet bytes | connectionless marker plus text | vector/string helper | connectionless diagnostics | diagnostic-only | partial | high | Not a netchan packet envelope. |
| `AppendByteLengthPrefixedText` | `src/game_api/hl_server_module.cpp` | pseudo-packet string helper | write | byte-level diagnostic text | clamps text to 255 and prefixes one byte length | vector-owned bytes | pseudo signon/wiremap diagnostics | diagnostic-only | partial | medium | Must not be treated as real signon string evidence. |
| `AppendLittleEndianShort` | `src/game_api/hl_server_module.cpp` | pseudo-packet numeric helper | write | byte-level 16-bit integer | writes low byte then high byte | vector-owned bytes | pseudo signon/wiremap diagnostics | diagnostic-only | partial | medium | Visible little-endian helper, but only in diagnostic pseudo surfaces. |
| `BuildDedicatedSignonWiremapPacketBytes` / `BuildDedicatedSignonBurstBytes` / `BuildDedicatedSignonStreamBytes` / `BuildDedicatedSignonStreamWindowBytes` | `src/game_api/hl_server_module.cpp` | pseudo signon byte report | write | byte-level diagnostic pseudo-packets | `HLWM`, `HLWB`, `HLWS`, `HLWW` style marker families and small counters | vector-owned bytes; local diagnostic framing | signon diagnostics, not real signon | diagnostic-only | no for real wire; partial for future pseudo fixtures | medium | These are prompt-era diagnostic packets and do not prove real netchan framing. |
| `BuildDedicatedSignonMessageBoundaries` / `BuildDedicatedSignonMessageFetchBytes` | `src/game_api/hl_server_module.cpp` | pseudo signon message boundary report | read/write diagnostic bytes | byte-level diagnostic pseudo-packets | scans diagnostic markers and emits offsets/lengths | vector-owned bytes | signon diagnostics, not real signon | diagnostic-only | no for real wire; partial for reports | medium | Useful as evidence-gap history only. |

No production `MSG_Write*`, `MSG_Read*`, `SZ_Write`, `sizebuf_t`, real bit writer, real netchan packet writer, reliable buffer writer, or unreliable buffer writer was found in `include/` or `src/`.

## Netchan Static Inventory

| Symbol or term | File | Role | Active/diagnostic/artifact/unknown | Direction | Tied buffers | Sequence/ack fields | Reliable/unreliable tie | Stage tie | Byte evidence present? | Blocker | Safe future action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `netchan_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | safety/blocker summary field | diagnostic/report | none | none | none | marks absent channel | post-connect/signon blockers | no | real netchan not implemented | keep as blocker until separate evidence prompt. |
| `reliable_channel_not_started` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | safety/blocker summary field | diagnostic/report | none | none | none | marks absent reliable state | post-connect/signon blockers | no | reliable envelope unknown | inventory sequence/ack separately. |
| `resource_baselines_not_sent` | `include/game_api/hl_server_module.h`, `src/game_api/hl_server_module.cpp` | safety/blocker summary field | diagnostic/report | server to client in future only | none | none | baseline dependency marker | signon/resource blockers | no | baseline contract unknown | keep blocked before signon evidence. |
| `MSG_ONE_UNRELIABLE` and related message destinations | `src/game_api/server_frame_loop.h`, `src/game_api/server_frame_loop.cpp` | message destination label | diagnostic observation | server callback intent only | `FrameMessageBuffer` record | none | name includes unreliable destination | engine callback observation | no real channel bytes | no unreliable envelope | use only as observed callback metadata. |
| `DedicatedSignonWiremapSequenceId` and pseudo packet counters | `src/game_api/hl_server_module.cpp` | pseudo sequence/counter reporting | diagnostic pseudo-packet | server report to diagnostic probe | `std::vector<unsigned char>` | local diagnostic sequence id only | no reliable channel | pseudo signon diagnostics | only pseudo bytes | not a netchan sequence contract | do not reuse as real netchan. |
| `Netchan` / `netchan` search hits in prompt artifacts | prompt artifact files | historical blocker statements | artifact/report | none | none | none | no real channel | report-only | no | artifact text is not source evidence | cite only as policy history. |

Not found in source: `net_chan`, real netchan channel object, fragment handling, split packet handling, packet loss tracking, flow tracking implementation, client/server channel send/receive functions, real sequence/ack encoder, real reliable buffer, and real unreliable buffer.

## Message-Writing Dependency Map

| Future need | Required byte primitive | Helper found? | Helper path/symbol | Evidence confidence | Stage | Missing info | Safe next task |
| --- | --- | --- | --- | --- | --- | --- | --- |
| marker/header | byte array append | yes for query/info and pseudo diagnostics | `BuildGoldSrcInfoResponse`, pseudo signon builders | high for query/info, medium for pseudo | connectionless / pseudo only | real post-connect/signon marker or netchan envelope | netchan sequence/ack static contract inventory. |
| opcode/message id | byte or short write | partial | `FrameMessageBuffer::WriteByte`, `StubWriteByte`, query/info response tag | high for observation, high for query/info | diagnostic observation / query/info | actual `svc_serverinfo` id and framing for target stage | serverinfo field contract after netchan envelope evidence. |
| protocol/version | byte/long/short, unknown | partial | query/info protocol byte; pseudo helpers | high for query/info only | query/info / pseudo | post-connect/signon protocol field width/order | message field contract inventory later. |
| server count/spawn count | short/long, unknown | partial | pseudo signon helpers reference slot/session/spawn fields | medium | pseudo signon diagnostics | real width, endian, source field, stage | static field contract only after netchan contract. |
| map name | C string or length-prefixed string | partial | `AppendGoldSrcCString`, `AppendByteLengthPrefixedText` | high for query/info, medium for pseudo | query/info / pseudo | real signon string policy | message-writing policy fixture contract. |
| game directory | C string or length-prefixed string | partial | `AppendGoldSrcCString` for query/info | high for query/info | connectionless query/info | post-connect/signon string policy | serverinfo field contract later. |
| hostname | C string or length-prefixed string | partial | `AppendGoldSrcCString` for query/info | high for query/info | connectionless query/info | post-connect/signon hostname source and encoding | field contract later. |
| max clients / player slot | byte/short, unknown | partial | query/info byte fields; diagnostic slot fields | high for query/info | query/info / diagnostics | real stage width/order | field contract later. |
| checksum / CRC | short/long/hash, unknown | no sufficient helper | none found as real serverinfo helper | low | unknown | field source, width, endian | static checksum/CRC inventory if needed. |
| model baseline | bit/byte message, unknown | no | no real baseline writer found | low | signon/baseline | full baseline message contract | baseline/resource message static inventory. |
| sound baseline | bit/byte message, unknown | no | no real sound baseline writer found | low | signon/baseline | full baseline message contract | baseline/resource message static inventory. |
| event baseline | bit/byte message, unknown | no | no real event baseline writer found | low | signon/baseline | full baseline message contract | baseline/resource message static inventory. |
| resource list | byte/string/CRC, unknown | no | `resource_baselines_not_sent` only | low | signon/resource | full resource message contract | baseline/resource message static inventory. |
| client data | user message writes, unknown | partial observation only | `FrameMessageBuffer`, `StubWrite*` | medium for callback observation | diagnostic callback | real client data message contract | message-writing policy fixture contract. |
| disconnect/reject message | connectionless text or netchan message, unknown | partial | connectionless text helpers and diagnostic reject summaries | medium | connectionless diagnostics | real post-connect rejection framing | reject/disconnect byte contract inventory later. |
| reliable channel envelope | sequence/ack/reliable state | no | none found | low | netchan | sequence number, ack, reliable bit, fragment policy | netchan sequence/ack static contract inventory. |
| unreliable datagram envelope | sequence/ack/unreliable payload | no | none found | low | netchan | packet header and payload framing | netchan sequence/ack static contract inventory. |
| netchan sequence/ack | numeric fields and state machine | no | only pseudo sequence id and report fields | low | netchan | real channel state and byte order | netchan sequence/ack static contract inventory. |

## Buffer And Overflow Policy Inventory

| Policy area | Found now | Classification | Missing before real byte fixtures |
| --- | --- | --- | --- |
| fixed-size buffers | yes in local diagnostic socket paths and arrays | diagnostic/runtime-local, not central message writer | real max packet/message sizes per stage. |
| dynamic byte buffers | yes, `std::vector<unsigned char>` in query/info and pseudo signon builders | diagnostic byte helpers | central bounds policy and stage-specific max length. |
| sizebuf-like abstraction | no source implementation found | missing | equivalent of size, max size, overflow flag, write cursor, and clear policy. |
| overflow flags | no central message overflow flag found | missing | deterministic failure behavior when message grows too large. |
| bounds checks | partial in query/info parser and pseudo helpers | diagnostic-only | reusable field-specific validation policy for future fixtures. |
| message max size constants | no central real message max found | missing | per-stage max message size and packet split policy. |
| string truncation | `AppendByteLengthPrefixedText` clamps at 255 for pseudo diagnostics | diagnostic-only | real string encoding and maximums for post-connect/signon. |
| string termination | `AppendGoldSrcCString` appends `0x00` for query/info; `FrameMessageBuffer::WriteString` counts `size + 1` | query/info and callback observation | stage-specific string encoding and termination proof. |
| endian helpers | `AppendLittleEndianShort` only for pseudo diagnostics | diagnostic-only | real numeric endian policy for serverinfo/netchan fields. |
| safe previews | diagnostic preview helpers exist | report-only | byte-level previews for future fixtures must remain non-evidence unless validated. |

## Stage-Specific Reuse Assessment

| Stage or message family | Reusable now? | Diagnostic-only? | Requires byte evidence? | Requires runtime? | Requires real client? | Risk |
| --- | --- | --- | --- | --- | --- | --- |
| connectionless query/info | yes, within closed boundary only | yes | already fixture-backed for query/info | no for rerun wrapper dry-run; loopback runtime already guarded | no | low if kept scoped. |
| post-connect serverinfo | no | yes for previews only | yes | later, only after policy | no now | high. |
| signon-time serverinfo | no | yes for pseudo signon reports only | yes | later, only after policy | no now | high. |
| netchan reliable messages | no | no real implementation found | yes | later | no now | high. |
| netchan unreliable messages | no | no real implementation found | yes | later | no now | high. |
| baseline/resource messages | no | blocker/report only | yes | later | no now | high. |
| reject/disconnect messages | partial for connectionless diagnostics | yes | yes for post-connect/signon | later | no now | medium. |
| user message callback observation | partial | yes | no for report-only observation; yes for real wire | no | no | medium. |

## Netchan And Message Blocker Ranking

| Rank | Blocker | Severity | Evidence | Smallest safe next task | Real client needed now? | Runtime needed now? | Public/LAN needed? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | missing netchan sequence/ack contract | high | no source implementation for real netchan sequence/ack; only blocker fields and pseudo counters | static netchan sequence/ack contract inventory | no | no | no |
| 2 | missing reliable/unreliable envelope contract | high | `reliable_channel_not_started` and destination names only | map reliable/unreliable state terms and envelope assumptions | no | no | no |
| 3 | missing post-connect serverinfo byte field contract | high | prompt 298 and current inventory keep byte evidence insufficient | serverinfo message field contract inventory after netchan envelope review | no | no | no |
| 4 | missing signon-time serverinfo byte field contract | high | unresolved signon remains not buildable | signon field contract inventory after netchan envelope review | no | no | no |
| 5 | missing message writer policy for strings/numeric fields | medium-high | `AppendGoldSrcCString`, `AppendByteLengthPrefixedText`, and `AppendLittleEndianShort` are stage-specific diagnostics | message-writing policy fixture contract | no | no | no |
| 6 | missing baseline/resource message contract | medium-high | `resource_baselines_not_sent` remains true; no baseline writer found | baseline/resource message static inventory | no | no | no |
| 7 | missing reject/disconnect byte contract | medium | connectionless text exists, post-connect/signon rejection framing unknown | reject/disconnect byte contract inventory | no | no | no |
| 8 | unknown central buffer overflow policy | medium | no `sizebuf`/overflow flag implementation found | design diagnostic overflow policy before builders | no | no | no |
| 9 | unknown real endian policy | medium | only pseudo little-endian short helper found | field-specific endian evidence inventory | no | no | no |
| 10 | real client capture remains forbidden | high policy | current boundary forbids Steam/real client/public/LAN | keep blocked until explicit capture policy prompt | yes later, not now | later only | no public/LAN |

## Conclusions

The repo contains useful diagnostic message observation helpers and query/info byte helpers, but it does not contain a real post-connect/signon serverinfo wire builder, a real serverinfo parser, a real netchan packet writer, a real reliable/unreliable envelope implementation, or a central sizebuf/overflow policy. The query/info boundary remains closed and diagnostic-only.

Recommended next prompt:

HL-CL-20260504-300-dedicated-goldsrc-hlds-netchan-sequence-ack-static-contract-inventory

Rationale: before a post-connect or signon-time serverinfo field contract can be designed safely, the packet envelope around those messages still needs a static netchan sequence/ack contract inventory. The existing message-writing helpers are clear enough to classify as diagnostic observation/query-info/pseudo helpers, but not clear enough to support a real serverinfo byte contract.
