# GoldSrc stock fragment-completion observation

## Status

Prompt 239 remains the historical blocked observation: the stock client
reassembled the final resource-manifest fragment, but could not emit the next
sequenced packet because the preceding signon stream had not installed the
required `usercmd_t` delta description. Prompt 240 implemented the canonical
delta-description bootstrap and resolved that boundary.

The mismatch identifier is:

`application_payload_missing_usercmd_delta_description`

No fragment completion was inferred from emission alone, no server state was
advanced without a stock-client acknowledgement, and no claimed correction
was committed.

## Pinned environment

- workspace: `D:\DEV\CPP\HL-Engine`;
- baseline: `f2cbaeab2f05c4780927ef70ad156f9d086e3452`;
- task branch: `codex/goldsrc-stock-fragment-completion-slice`;
- Half-Life SDK: `b1b5cf5892918535619b2937bb927e46cb097ba1`;
- stock client: Half-Life `1.1.2.2`, Steam build `15961492`;
- game directory: the installed, unchanged `valve` directory;
- map: `c0a0`;
- transport: direct IPv4 UDP on a dynamically selected `127.0.0.1` port;
- manifest source: the authoritative runtime registries, not a fixture.

The installed client and game files were treated as read-only. Observation
created no packet capture and retained no screenshot, raw payload, resource
name list, credential, or proprietary file.

## Initial stock observation

The Prompt 238 timeout was reproduced against the Release Win32 host before
any source edit:

`timeout_reproduced=yes`

`fragment_completion_trace_observed=yes`

The bounded semantic trace was:

1. The stock client completed connectionless admission, connected netchan,
   server-info acknowledgement, and the observed `sendres` request batch.
2. The host built one authoritative 308-entry, 7506-byte resource manifest.
3. The host scheduled eight reliable fragments.
4. Fragments one through seven each received a covering client
   acknowledgement with the expected reliable state.
5. The final short fragment was carried by server sequence 18 with reliable
   state 1.
6. The last client packet already in flight covered server sequence 17, not
   sequence 18.
7. The host sent its normal sequence-19 packet after the final fragment.
8. The stock client emitted no subsequent packet, so there was no covering
   acknowledgement for carrier 18.
9. The host remained in
   `resource_manifest_sent_awaiting_ack` and expired the bounded transfer.
10. `put_in_server`, `spawned`, and `active` all remained zero.

The carrier header, fragment count/index, byte ranges, payload transform range,
reliable toggling, and first seven acknowledgement decisions agree with the
pinned public reference implementation. The final fragment was not silently
treated as application completion.

## Earliest mismatch

Immediately after receiving the final fragment, the unchanged client displayed
its built-in Visual C++ assertion from `hw.dll`, `common.c` line 871:

`ppdesc && *ppdesc`

The assertion is on the client's `MSG_WriteUsercmd` path. The pinned public
GoldSrc reconstruction performs a `usercmd_t` delta-registration lookup before
encoding the outgoing command and then dereferences that description. This
explains the exact network symptom: the client has accepted the completed
fragment payload and is preparing its next sequenced packet, but it blocks
before sending the packet that would acknowledge server sequence 18.

The missing prerequisite is visible in the current local send order.
`EncodeGoldSrcServerInfo` emits `svc_serverinfo` and `svc_sendextrainfo`; the
next implemented signon output is the resource response. There is no
delta-description stream between them.

The pinned public reference sends `SV_WriteDeltaDescriptionsToClient` after
server/extra info and before movevars and `SV_SendResources`. Its common
message writer then has a populated `usercmd_t` registration when the client
constructs an outgoing user command:

- [ReHLDS server send order](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1182-L1215)
- [ReHLDS delta-description writer](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/sv_main.cpp#L1023-L1047)
- [ReHLDS user-command writer](https://github.com/rehlds/ReHLDS/blob/0124d56c3d888d922eb045775f71c6682ad1226f/rehlds/engine/common.cpp#L323-L330)

Therefore:

`earliest_completion_mismatch_identified=yes`

`mismatch_category=application_payload_missing_usercmd_delta_description`

This is not a fragment-header, transfer-ID, byte-range, last-marker, transform,
reliable-toggle, acknowledgement-correlation, pacing, or missing
post-fragment-carrier mismatch.

## Frozen correction contract and scope conflict

The smallest coherent correction would be:

1. encode the stock-compatible delta-description set, including `usercmd_t`;
2. send it in the reference-compatible pre-resource signon order;
3. rerun the unchanged stock client and require a real covering
   acknowledgement for the final fragment;
4. only then mark the manifest transfer complete and advance once.

Prompt 239 section 19 explicitly lists delta descriptions as a non-goal.
Implementing even the minimal `usercmd_t` description requires the
delta-description wire codec, metadata definition, sequencing, tests, and
proofs. Substituting a dummy description, suppressing the client assertion,
treating all-fragments-sent as completed, or accepting the non-covering
sequence-17 acknowledgement would fake completion and violate the prompt's
completion rules.

The frozen decision for this branch is consequently:

`correction_identity=blocked_by_delta_description_non_goal`

No fragmentation, netchan, resource-manifest, signon-state, spawn, movement,
or gameplay source was changed.

## Verification and next boundary

The full pre-change baseline was green:

- Release Win32 host and all five test targets: PASS;
- CTest: PASS, 5/5;
- handshake and disconnected-slot-reuse proofs: PASS;
- netchan Proof A/B: PASS;
- server-info Proof A/B: PASS;
- resource-manifest Proof A/B: PASS;
- fragmented-manifest Proof A/B: PASS;
- Prompt 238 continuation Proof A/B: PASS;
- feature-off regression: PASS with
  `normal_host_behavior_changed=0`.

The required new stock completion Proof A/B and their proof script were not
created because there is no permitted correction contract for them to prove.
The final stock fragment-transfer checkpoint remains incomplete:

- `previous_fragment_timeout_resolved=no`;
- `stock_fragment_transfer_completed=no`;
- `stock_client_advanced_past_fragment_completion=no`;
- `next_observed_boundary=missing`;
- `put_in_server=0`;
- `spawned=0`;
- `active=0`.

The next authorized implementation slice must explicitly permit the bounded
pre-resource delta-description stream. Only after that stock-client run
produces the real covering acknowledgement may a later boundary be named.

## Prompt 240 resolution

The final Prompt 240 checkpoint reused the same unmodified Half-Life `1.1.2.2`
client, Steam build `15961492`, installed read-only `valve` directory, `c0a0`,
one client slot, and a dynamically selected loopback port. Production loaded
the installed runtime `delta.lst`; no synthetic delta fixture or copied game
definition was used for this checkpoint.

The host sent the complete seven-table delta-description bundle in the same
logical response as serverinfo, followed by the bounded movevars, CD-track,
and setview tail. The existing reliable fragment sender carried that combined
bootstrap in seven fragments. The stock client acknowledged all seven,
accepted the `usercmd_t` schema, and continued to the authoritative resource
request. The host then sent the 308-entry, 7506-byte runtime resource manifest
in eight fragments, and the client acknowledged all eight.

This is the covering application acknowledgement that was absent in Prompt
239. The prior `ppdesc && *ppdesc` assertion did not recur, the client remained
responsive after resource acknowledgement, and the host stayed stable.
`put_in_server`, `spawned`, and `active` remained zero throughout.

The first subsequent event was an unreliable client application command. The
strict signon decoder intentionally does not parse a command identity when the
reliable bit is absent, so the bounded semantic boundary is:

`unreliable_post_resource_client_command`

The recorded typed rejection reason is `unreliable-command`, with no asserted
string-command identity. This next boundary is outside the delta-description
bootstrap slice and was observed without adding user-command gameplay
decoding, movement, baselines, snapshots, spawn, or gameplay.

Final Prompt 240 stock result:

- `stock_client_tested=yes`;
- `stock_client_accepted_usercmd_delta=yes`;
- `missing_usercmd_delta_resolved=yes`;
- `stock_client_advanced_past_previous_boundary=yes`;
- `next_observed_boundary=unreliable_post_resource_client_command`;
- `put_in_server=0`;
- `spawned=0`;
- `active=0`.
