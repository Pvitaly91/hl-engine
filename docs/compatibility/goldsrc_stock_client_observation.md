# GoldSrc stock-client observation

## Scope and safety

Prompt 238 performs the first interoperability checkpoint with an unmodified,
legally installed Steam Half-Life client. The observation used:

- `hl-engine` baseline branch `codex/goldsrc-netchan-fragmentation-slice`;
- baseline commit `673afc395ed2966697d2445560fda5578b3f03e3`;
- task branch `codex/goldsrc-stock-client-continuation-slice`;
- Valve HLSDK commit `b1b5cf5892918535619b2937bb927e46cb097ba1`;
- the registered local Steam library's existing `steamapps/common/Half-Life`
  installation;
- Half-Life patch version `1.1.2.2`, Steam build ID `15961492`;
- game directory `valve` and map `c0a0`;
- the Prompt 237 Release `hlhost.exe`.

The server bound a dynamically selected UDP port on `127.0.0.1`. The client
was launched unchanged with a direct loopback connection. Temporary host
stdout/stderr files lived in the operating-system temporary directory and
were deleted after each bounded run. The client process and directly owned
host process were terminated and verified absent after each observation.

No client binary, game file, credential, packet capture, raw datagram, or
temporary log is stored in the repository. The installed client and Valve
HLSDK were read-only inputs.

An official `hlds.exe` is present in the registered Half-Life installation,
but it was not needed for this checkpoint and was not launched. The behavioral
comparison uses the stock-client observation plus the pinned public ReHLDS and
Xash3D sources.

## Observation method

The baseline host was run with the existing opt-in resource-manifest path and
bounded negative-proof lifetime so it would remain available long enough to
observe traffic after the first rejection. Existing semantic diagnostics
reported admission, netchan, reliable acknowledgement, signon phase, and
typed decoder results.

The first run established that the client launched, admitted one authoritative
session, established netchan, delivered `new`, accepted serverinfo, and
continued sending sequenced traffic. A shorter repeat isolated the first
application rejection. One observation-only diagnostic was then added at the
existing rejection seam. It records at most four opcode identities or bounded
command-name tokens plus a bounded semantic suffix classification, never
arbitrary argument text or raw bytes. Repeated short runs with the same client
build and server configuration froze both the message identities and the
otherwise-hidden companion terminators before the accepted grammar was
finalized.

## Sanitized initial result

The unmodified client completed:

1. connectionless challenge and connect;
2. one authoritative connected session;
3. reliable netchan establishment;
4. reliable client `new`;
5. serverinfo receipt and reliable acknowledgement.

Immediately after serverinfo acknowledgement, the client sent one reliable
application payload containing three client string-command messages in this
order:

1. `sendres`;
2. `closemenus` followed by exactly one space and LF before its NUL
   terminator;
3. the same exact `closemenus` form.

The Prompt 237 decoder accepts exactly one string command surrounded only by
NOP messages. It therefore returned `multiple_commands` for the complete
payload and did not deliver the otherwise valid typed `sendres` event. No
resource manifest was generated or transmitted in the baseline stock-client
run.

Approximately one second later, the same client sent a separate reliable
`VModEnable` string command. The existing bounded decoder returned
`unsupported_command`. That later command is not the earliest divergence and
is not part of the selected continuation.

The authoritative session remained:

- `put_in_server=0`;
- `spawned=0`;
- `active=0`.

## First divergence

- Observation complete: `yes`
- First divergence identified: `yes`
- Category: `client_request_not_supported`
- Semantic identifier: `batched_sendres_closemenus_rejected`
- Last completed signon phase:
  `serverinfo_acknowledged` / `awaiting_resource_request`
- Earliest expected event: typed resource request delivery from the first
  `sendres` command in the reliable batch
- Actual event: atomic `multiple_commands` rejection
- Confidence: high

This corrects an earlier interoperability assumption: the deterministic
Prompt 236/237 probe sends `sendres` alone, while this stock client coalesces
the resource request with two benign UI-close command messages.

## Public reference correlation

The implementation remains independently written. Public sources are used as
behavioral evidence:

- ReHLDS commit `0124d56c3d888d922eb045775f71c6682ad1226f`
  repeatedly reads client opcodes until the bounded message is exhausted in
  [`SV_ExecuteClientMessage`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_user.cpp#L1852-L1890).
  Each `clc_stringcmd` is parsed separately rather than making the whole
  datagram a single-command grammar.
- The same ReHLDS commit registers and handles `sendres` through the
  pre-spawn resource path in
  [`SV_SendRes_f`](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1596-L1618).
- Xash3D FWGS commit
  `f2166a2c9def5a613882a404d580b07418ff2065` queues reliable `sendres`
  immediately after parsing GoldSrc serverinfo in
  [`CL_ParseServerData`](https://github.com/FWGS/xash3d-fwgs/blob/f2166a2c9def5a613882a404d580b07418ff2065/engine/client/parse/cl_parse.c#L961-L969).

The stock-client observation supplies the exact `closemenus` companion
identities, ordering, and single-space/LF suffix. The references corroborate
that a server must iterate bounded client messages and must not discard the
valid earlier `sendres` solely because another client string-command message
follows it.

## Selected bounded correction

The next slice will extend the existing typed signon decoder, not introduce a
generic command dispatcher. In `awaiting_resource_request` it will accept:

- the existing exact standalone `sendres`;
- the observed ordered batch containing one exact `sendres` followed by
  exactly two `closemenus` companions, each with exactly one space and LF
  before its NUL terminator.

The result will expose one typed resource-request event and a bounded benign
companion count. It will never execute `closemenus`, forward arbitrary text,
or route a command to the console or Game DLL. Arguments, reordering,
one companion instead of the observed pair, additional messages, a second
`sendres`, unsupported opcodes, and the later `VModEnable` remain rejected.

The existing signon phases already express the coherent exchange:

`awaiting_resource_request` -> `resource_manifest_queued` ->
`resource_manifest_sent_awaiting_ack` ->
`resource_manifest_acknowledged`.

No spawn, gameplay, or parallel signon state is introduced. After this
correction, the same unmodified client will be rerun to verify that it reaches
the fragmented resource manifest, determine whether it completes the
acknowledgement sequence, and record the next observable boundary.

## Final stock-client verification

The final verification used the same registered Half-Life 1.1.2.2 client,
Steam build `15961492`, `valve` game directory, `c0a0` map, direct loopback
launch, and authoritative runtime manifest source. Normal fragmented-manifest
mode was used; the negative-proof mode intentionally preserves the older
unfragmented production-size rejection and is not the stock interoperability
configuration.

The old batch rejection did not recur. The host:

- decoded the exact observed batch;
- delivered one typed resource request;
- accepted exactly two typed `close_menus` companions without executing them;
- built one authoritative 308-entry resource manifest;
- froze and queued the 7506-byte response;
- transmitted the response through the existing fragmented reliable sender;
- remained connected with `put_in_server=0`, `spawned=0`, and `active=0`.

The stock client did not acknowledge completion of that fragmented transfer.
After eight carrier sends, the existing bounded fragment deadline expired.
The host reported the typed fragment-transfer timeout and performed a clean
controlled shutdown. It did not crash, create another session, enter the Game
DLL player lifecycle, or repeat the corrected batch rejection.

Final checkpoint:

- `stock_client_tested=yes`
- `stock_client_reached_previous_boundary=yes`
- `previous_divergence_resolved=yes`
- `stock_client_advanced_past_previous_boundary=yes`
- next divergence category: `fragment_completion_mismatch`
- next semantic boundary: `stock_fragment_completion_timeout`
- `next_observed_boundary_recorded=yes`

This timeout is evidence for the next task, not a reason to widen the current
slice. No fragment behavior, spawn path, or gameplay behavior was changed in
Prompt 238.

## Prompt 241 post-resource command checkpoint

Prompt 241 reused the same unmodified Half-Life 1.1.2.2 client, Steam build
`15961492`, installed read-only `valve` data, `c0a0`, and direct loopback
launch. The host loaded the installed runtime `delta.lst`; no repository
fixture replaced the production schema. The client and host used an isolated
temporary working directory, and all traffic remained on `127.0.0.1`.

Before implementation, the first application payload after the Prompt 240
resource boundary was observed as an unreliable opcode `2` command. It was
not batched with another application command and repeated in fresh outer
sequences while the client waited for server continuation. An independent
bounded decode, correlated with pinned public ReHLDS and Xash3D behavior,
identified the command as `clc_move`: zero-percent packet loss, two backup
commands, one new command, and three runtime-schema `usercmd_t` deltas. The
message includes the sequence-derived checksum used by GoldSrc move
containers.

The final stock run completed:

1. challenge and connect;
2. established protocol-48 netchan;
3. the seven-table runtime delta-description bootstrap;
4. seven of seven bootstrap fragment acknowledgements;
5. the batched resource request;
6. eight of eight resource-manifest fragment acknowledgements;
7. final resource acknowledgement;
8. one typed, checksum-valid `clc_move` delivery.

The host treated the decoded move as pre-spawn keepalive input and advanced
exactly once to `awaiting_server_baseline_or_snapshot`. It did not execute
movement, copy decoded input into gameplay state, or invoke the Game DLL
player lifecycle. The final authoritative state remained:

- `session_count=1`;
- `put_in_server=0`;
- `spawned=0`;
- `active=0`;
- `server_still_responsive=true`;
- clean host shutdown.

The old `unreliable_post_resource_client_command` boundary is therefore
resolved. The next observed boundary is
`server_baseline_or_snapshot_required`: after validated pre-spawn move input,
the stock client is waiting for the server-side baseline or snapshot
continuation that this slice intentionally does not fabricate.

The installed client executable and runtime delta definition were hashed
before and after the run and remained unchanged. Only the exact owned client
and host process IDs were cleaned, the isolated temporary directory was
removed, and no capture, raw packet dump, installed asset, credential, or
machine-specific log was retained.
