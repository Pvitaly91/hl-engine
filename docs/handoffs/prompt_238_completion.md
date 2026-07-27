# Prompt 238 completion handoff

## Result

Prompt 238 completed the first bounded stock-client correction on:

- baseline branch: `codex/goldsrc-netchan-fragmentation-slice`;
- baseline commit: `673afc395ed2966697d2445560fda5578b3f03e3`;
- task branch: `codex/goldsrc-stock-client-continuation-slice`;
- Valve HLSDK commit:
  `b1b5cf5892918535619b2937bb927e46cb097ba1`.

The source worktree started at the exact verified baseline with only the
pre-existing untracked `out/` tree. The SDK remained read-only and unchanged.

## Stock client and observation

The registered Steam installation supplied an unmodified Half-Life client:

- patch version: `1.1.2.2`;
- Steam build ID: `15961492`;
- game directory: `valve`;
- map: `c0a0`;
- launch: direct connection to a dynamically selected `127.0.0.1` UDP port.

The installed launcher reports file/product version `1.1.1.1`; `steam.inf`
and the registered Steam app manifest provide the more specific game patch
and build identity above.

An official local `hlds.exe` was available but was not used. Stock-client
observation plus pinned public ReHLDS and Xash3D behavioral sources were
sufficient. Steam was already running and required no credential automation
or manual approval.

Each observation started the Release `hlhost.exe`, launched the unchanged GUI
client, bounded the run, stopped only the newly launched client and directly
owned host, and removed temporary stdout/stderr files from the operating-system
temporary directory. Traffic was loopback-only. No capture, proprietary file,
credential, or raw packet data was stored.

## Initial boundary

The stock client completed:

1. connectionless challenge/connect;
2. one authoritative connected session;
3. netchan establishment;
4. reliable `new`;
5. serverinfo receipt and reliable acknowledgement.

It then sent one reliable application payload containing:

1. exact `sendres`;
2. `closemenus`, one ASCII space, LF, then NUL;
3. the same exact `closemenus` form.

The Prompt 237 decoder rejected the whole payload as multiple commands before
delivering `sendres`. Bounded follow-up diagnostics froze the two companion
suffixes without recording arbitrary arguments or packet bytes.

- observation complete: `yes`;
- divergence category: `client_request_not_supported`;
- original semantic identifier: `batched_sendres_closemenus_rejected`;
- last completed phase: `awaiting_resource_request`;
- confidence: high.

A later standalone `VModEnable` request was also observed and remains outside
this slice.

## Implemented correction

The existing platform-independent signon decoder now accepts only:

- the inherited exact standalone `sendres`; or
- exact `sendres` followed by exactly two observed `closemenus` companions.

The typed decode result reports the primary resource request, the
`close_menus` companion identity, and count two. It rejects:

- one companion;
- more than two companions;
- a companion without the observed suffix;
- alternate whitespace, arguments, order, or case;
- a second `sendres`;
- incomplete, over-limit, control-invalid, or unsupported input;
- unsupported opcodes and trailing data.

Companion strings are never executed, forwarded to the console, or dispatched
to the Game DLL. The correction reuses the existing per-session signon state,
manifest builder, reliable queue, fragmentation sender, acknowledgement
correlation, disconnect reset, and slot-reuse reset. It adds no transport,
client registry, signon machine, fragmentation path, or generic command
dispatcher.

Because this is an existing-message correction before the Prompt 237 terminal
phase, the coherent state path remains:

`awaiting_resource_request` -> `resource_manifest_queued` ->
`resource_manifest_sent_awaiting_ack` ->
`resource_manifest_acknowledged`.

Small manifests use the ordinary reliable path. Larger legal manifests use
the existing bounded 64-fragment sender. Serialized response bytes remain
frozen until acknowledgement.

## Tests and localhost proofs

The existing `goldsrc_signon_tests` target now covers the exact observed
fixture, standalone compatibility, companion count and suffix bounds,
incomplete input, over-limit input, ordering, unsupported variants, trailing
data, duplicate delivery, wrong phases, acknowledgement, and reset behavior.
The inherited resource and fragmentation tests continue to cover deterministic
encoding, capacity failures, small/large transport selection, retransmission,
sequence wrap, reliable toggles, and clean reset.

Continuation Proof A:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_signon_continuation_proof.ps1 `
  -ExecutablePath out/build/vs2022-win32-reference-sdk/Release/hlhost.exe `
  -GameDir out/runtime/valve-fixture `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 90 `
  -SkipServerOutput
```

Proof A reproduced the exact observed batch, passed the complete Prompt 237
boundary, received three fragments, reassembled and semantically decoded all
46 ordered resources, acknowledged completion, delivered the continuation
once, generated the manifest once, remained connected and inactive, and shut
down cleanly.

Continuation Proof B:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_signon_continuation_proof.ps1 `
  -ExecutablePath out/build/vs2022-win32-reference-sdk/Release/hlhost.exe `
  -GameDir out/runtime/valve-fixture `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 60 `
  -NegativeProof -SkipServerOutput
```

Proof B verified delayed/non-covering acknowledgement, byte-identical
retransmission, duplicate-request suppression, incomplete and unsupported
input rejection, response bounds, state reset, exactly one delivery, one
session, continued responsiveness, inactive gameplay state, and clean
shutdown.

## Final stock-client result

The final normal-mode run used the same client build, game directory, map, and
authoritative runtime manifest source as the initial observation.

The previous batch rejection did not recur. The host decoded the exact batch,
delivered one resource request, recorded two companions, built one 308-entry
authoritative manifest, froze a 7506-byte response, and sent it through the
existing fragmented reliable path. The stock client therefore reached and
advanced past the old divergence into the Prompt 237 fragmented-manifest
boundary.

The stock client did not complete the fragment acknowledgement sequence.
After eight carrier sends, the bounded fragment transfer expired. The host
reported the typed timeout and performed a controlled clean shutdown without
spawning or activating the client.

- `stock_client_tested=yes`;
- `stock_client_reached_previous_boundary=yes`;
- `previous_divergence_resolved=yes`;
- `stock_client_advanced_past_previous_boundary=yes`;
- next divergence category: `fragment_completion_mismatch`;
- next boundary: `stock_fragment_completion_timeout`;
- `put_in_server=0`;
- `spawned=0`;
- `active=0`.

The next task should compare the stock client's fragment acknowledgement
expectations with the existing Prompt 237 carrier metadata and completion
rules. It must observe before changing fragment behavior. Spawn remains a
non-goal until the stock client completes this boundary.

## Final regression matrix

| Check | Result |
|---|---|
| Release Win32 `hlhost` and all five test targets | PASS |
| Full CTest | PASS (5/5) |
| Existing handshake proof | PASS |
| Existing disconnected-slot-reuse proof | PASS |
| Existing normal netchan proof | PASS |
| Existing netchan retransmission Proof B | PASS |
| Existing serverinfo Proof A/B | PASS |
| Existing resource-manifest Proof A/B | PASS |
| Existing fragmented-manifest Proof A/B | PASS |
| Continuation Proof A/B | PASS |
| Required stock-client checkpoint | PASS for the bounded correction |
| Feature-off query/info acceptance gate | PASS |
| Feature-off query/info drift gate | PASS |
| Normal host behavior changed | 0 |
| PowerShell parser validation | PASS |
| `git diff --check` | PASS |

The feature-off wrapper reported `wrapper_full_run_passed=1`,
`public_socket_opened=0`, `lan_socket_opened=0`,
`real_client_binary_invoked=0`, all connect/post-connect/signon opt-ins zero,
and `normal_host_behavior_changed=0`.

## Artifact and Git scope

The task changes 11 repository files: decoder/runtime types and tests, the two
existing manifest proof scripts, the new continuation proof, two compatibility
documents, and this handoff. Before commit:

- only those task files are intended for staging;
- the pre-existing untracked `out/` tree remains unstaged;
- build artifacts staged: `no`;
- SDK staged: `no`;
- proprietary client/game files staged: `no`;
- capture files staged: `no`;
- unrelated changes reverted: `no`.

The authorized commit message is:

`feat(net): add next observed GoldSrc signon continuation`

Advancing the stock client past this signon boundary does not mean that
baselines, snapshots, ClientPutInServer, spawn, movement, or gameplay are
complete.
