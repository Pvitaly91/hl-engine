# GoldSrc observed signon continuation contract

## Selected continuation

- Prompt: 238
- Baseline commit: `673afc395ed2966697d2445560fda5578b3f03e3`
- Kind: existing-message correction
- Identity: `batched_sendres_closemenus`
- Original divergence: `batched_sendres_closemenus_rejected`
- Observation source: unmodified Steam Half-Life 1.1.2.2, build ID
  `15961492`, connected directly to a loopback Prompt 237 host

The selected correction is the smallest behavior that can move the observed
client forward. It changes only bounded decoding of the reliable request that
arrives in `awaiting_resource_request`. The existing resource-manifest model,
encoder, reliable transport, fragmentation, acknowledgement, and session
ownership remain the response path.

## Typed input grammar

The decoder continues to accept protocol NOP messages around supported
application messages. It reads client messages sequentially with checked
cursor arithmetic and the existing 1200-byte inbound application limit.

A valid resource-request payload contains:

1. exactly one `clc_stringcmd` whose complete NUL-terminated text is
   `sendres`;
2. either no companion messages, preserving the existing standalone request,
   or exactly two immediately following `clc_stringcmd` messages whose text
   is `closemenus`, one ASCII space, and LF before the NUL terminator;
3. no other non-NOP message or trailing data.

The zero-companion form preserves the existing deterministic probes. The
two-companion form is the exact stock-client observation. A one-companion
form is not observed and remains rejected.

The typed result contains:

- command identity `send_resources`;
- benign companion identity `close_menus`;
- companion count exactly 0 or 2;
- leading, inter-message, and trailing NOP counts only as bounded diagnostics.

The parser rejects atomically:

- missing string terminators;
- empty or over-limit command strings;
- any suffix after `sendres`;
- any `closemenus` suffix other than the observed single-space/LF form;
- `closemenus` before `sendres`;
- exactly one `closemenus` companion;
- more than two `closemenus` companions;
- another `sendres`;
- any other command, including `VModEnable`;
- any unsupported opcode or unknown trailing byte;
- payloads above the existing bounded input capacity.

No command string is executed, forwarded to the host console, or dispatched
to the Game DLL.

## State and exactly-once behavior

The current typed signon state already contains the required phases, so the
correction does not add a parallel or numbered phase:

| Phase | Event | Result |
|---|---|---|
| `awaiting_resource_request` | First valid standalone or observed batched request | Build and queue one frozen manifest; enter `resource_manifest_queued`. |
| `resource_manifest_queued` | First successful reliable/fragment carrier | Enter `resource_manifest_sent_awaiting_ack`. |
| `resource_manifest_sent_awaiting_ack` | Correct final covering reliable ACK | Enter `resource_manifest_acknowledged` once. |

The netchan suppresses duplicate outer sequences. A duplicate valid request
under a newer sequence is classified by the existing signon state and cannot
regenerate or requeue the manifest. A rejected batch cannot advance signon.
Wrong, stale, future, or non-covering acknowledgement state cannot complete
the manifest. Disconnect and authoritative slot reuse clear all request,
manifest, reliable, fragment, and diagnostic state.

At every selected phase:

- `put_in_server=0`;
- `spawned=0`;
- `active=0`.

## Response and transport

The response remains the existing typed resource companion plus resource list.
Runtime data comes from the authoritative loaded map and precache registries,
and size metadata comes from the selected local game directory. The complete
payload remains bounded to 65536 bytes and 1280 resources.

Responses through 1200 bytes retain the ordinary reliable path. Larger legal
responses reuse the existing one-stream fragmentation sender with at most 64
fragments of at most 1024 bytes. Serialized bytes are frozen until the
correlated final acknowledgement. Retransmission cannot rebuild the manifest
or change decoded bytes.

## Verification contract

Unit tests will cover:

- the exact observed three-command payload;
- standalone `sendres` compatibility;
- rejection of a single companion and all non-observed suffixes;
- bounds, terminators, arguments, ordering, count, unsupported commands and
  opcodes, and unsupported trailing data;
- duplicate delivery and wrong-phase behavior;
- deterministic manifest generation and frozen retransmission;
- small reliable and fragmented response paths;
- acknowledgement, reset, slot reuse, and inactive gameplay state.

External Proof A reproduces the exact observed batch against the real host,
uses the existing large fragmented fixture, semantically reassembles all
fragments, and requires one manifest generation and one final
acknowledgement. Proof B covers duplicate/rejected variations,
retransmission, bounds, reset, and continued session responsiveness.

The final stock-client run must prove that the original batch is no longer
rejected, the client reaches the Prompt 237 fragmented-manifest boundary, and
the next completion or divergence event is recorded. That new event is an
observation boundary only; it is not implemented unless it is already part of
this frozen correction.

The final stock run accepted the corrected batch and transmitted the
authoritative manifest through the existing fragmented sender. The stock
client did not complete the fragment acknowledgement sequence, so the next
observed boundary is `stock_fragment_completion_timeout`. Fragment
interoperability is deliberately left for the next bounded task.

Advancing the stock client past this signon boundary does not mean that
baselines, snapshots, ClientPutInServer, spawn, movement, or gameplay are
complete.
