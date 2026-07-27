# Prompt 240 completion handoff

## Outcome

Prompt 240 is complete. The host now loads and validates the protocol-48 delta
definitions from the selected runtime game directory, encodes the canonical
seven-table delta-description bundle, and places it in the same logical
bootstrap response as serverinfo. The existing reliable fragment sender
carries that response without a parallel transport path.

The unmodified Half-Life client accepted the `usercmd_t` description, the
Prompt 239 `ppdesc && *ppdesc` assertion did not recur, and the client
acknowledged all bootstrap and resource fragments. The first subsequent event
is recorded as:

`unreliable_post_resource_client_command`

The strict decoder reports `reason=unreliable-command` and intentionally does
not claim a string-command identity for an unreliable payload.

## Repository and carried state

- workspace: `D:\DEV\CPP\HL-Engine`;
- origin: `https://github.com/Pvitaly91/hl-engine.git`;
- baseline commit: `f2cbaeab2f05c4780927ef70ad156f9d086e3452`;
- task branch: `codex/goldsrc-delta-description-slice`;
- Half-Life SDK commit:
  `b1b5cf5892918535619b2937bb927e46cb097ba1`;
- stock client: Half-Life `1.1.2.2`, Steam build `15961492`;
- carried Prompt 239 files: 2;
- final intended task files: 30.

The two carried files are
`docs/compatibility/goldsrc_stock_fragment_completion.md` and
`docs/handoffs/prompt_239_completion.md`. The latter remains the historical
blocked diagnosis. No Prompt 239 observation was discarded or weakened.

The SDK remained read-only, clean, and at the pinned commit. The installed
client and game data also remained read-only. No generated build output,
installed game file, complete installed `delta.lst`, capture, credential, or
proprietary path is included in the commit.

## Original and corrected boundary

The original symptom was `stock_fragment_completion_timeout`. Prompt 239
proved that the actual mismatch was:

`application_payload_missing_usercmd_delta_description`

The client had reassembled the resource manifest, but its outgoing
`MSG_WriteUsercmd` path could not find `usercmd_t`; it asserted before sending
the packet that would acknowledge the final resource fragment.

Prompt 240 reproduced that boundary before implementation. The correction
adds the missing coherent bootstrap rather than treating fragment emission as
completion or installing a fabricated empty table.

## Runtime delta source and model

Production loads `<selected-game-directory>/delta.lst`, so the source category
is `runtime_game_dir`. The installed runtime definition was available and
contained all seven required tables and a complete 15-field `usercmd_t`.
Production does not substitute the repository fixtures or the SDK copy.

The platform-independent parser accepts only the verified `delta.lst` grammar
and uses bounded text, table, field, and name limits. Numeric conversion,
field types, bit counts, multipliers, duplicates, authoritative structure
layouts, and the exact conditional-encoder allowlist are validated before
session state changes. Stable typed failures cover malformed or unsupported
input.

The runtime registry contains 219 fields:

| Table | Fields |
|---|---:|
| `clientdata_t` | 50 |
| `entity_state_t` | 52 |
| `entity_state_player_t` | 49 |
| `custom_entity_state_t` | 19 |
| `usercmd_t` | 15 |
| `weapon_data_t` | 20 |
| `event_t` | 14 |

The canonical wire order is:

1. `event_t`
2. `weapon_data_t`
3. `usercmd_t`
4. `custom_entity_state_t`
5. `entity_state_player_t`
6. `entity_state_t`
7. `clientdata_t`

The meta-delta model explicitly encodes `fieldType`, `fieldName`,
`fieldOffset`, `fieldSize`, `significant_bits`, `premultiply`, and
`postmultiply`. Each descriptor is a delta from zero, bits are least
significant first, field count is an unsigned 16-bit value, and finite
multipliers are scaled by 4000 and truncated toward zero. Host structure bytes
are never serialized.

The exact parser, wire model, bounds, conditional encoders, and pinned public
reference provenance are documented in
`docs/compatibility/goldsrc_delta_descriptions.md`.

## Bootstrap placement and transport

The selected placement is `same_serverinfo_bundle`:

1. `svc_serverinfo` and its trailing secure byte;
2. `svc_sendextrainfo`;
3. seven `svc_deltadescription` messages;
4. `svc_newmovevars`;
5. `svc_cdtrack`;
6. `svc_setview`.

Resources remain a later response to the client's resource request. The
adjacent tail uses a bounded typed encoder with the reference wire order,
public cvar names, finite-value checks, a view-entity index range of 1 through
899, and a bounded sky name.

The complete logical payload is frozen while reliable transmission is
pending. Small legal payloads use the ordinary reliable path; the canonical
runtime bundle reuses the existing bounded fragmentation path. Wrong and
non-covering acknowledgements cannot advance signon, retransmissions are
byte-identical, and a correct covering acknowledgement advances once.

An exact reliable `dropclient\n` resets netchan, signon, serverinfo, delta,
combined-bootstrap, fragment, and resource state. Proof B requires one
pending-state reset followed by a distinct loopback endpoint reusing the same
slot in the same host process.

## Automated verification

The final Release build and test commands were:

```powershell
cmake --build out/build/vs2022-win32-reference-sdk `
  --config Release --parallel

ctest --test-dir out/build/vs2022-win32-reference-sdk `
  -C Release --output-on-failure
```

The build passed and CTest passed 6/6:

1. `goldsrc_connectionless_tests`;
2. `goldsrc_netchan_tests`;
3. `goldsrc_fragmentation_tests`;
4. `goldsrc_signon_tests`;
5. `goldsrc_resource_manifest_tests`;
6. `goldsrc_delta_description_tests`.

Delta Proof A:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_goldsrc_delta_description_proof.ps1 `
  -ExecutablePath out/build/vs2022-win32-reference-sdk/Release/hlhost.exe `
  -GameDir out/runtime/valve-fixture `
  -BindAddress 127.0.0.1 -Port 0 -TimeoutSeconds 60 `
  -SkipServerOutput
```

Proof A passed. It first reproduced deterministic missing-`usercmd_t`
rejection, then decoded all seven transmitted tables, verified every
synthetic descriptor including the complete 15-field `usercmd_t`, checked
table order and the bootstrap tail, acknowledged the bootstrap, continued to
resources, and shut down cleanly.

Delta Proof B used the same command plus `-NegativeProof`. It passed
byte-identical retransmission, withheld/wrong/non-covering acknowledgement,
duplicate-trigger suppression, missing-table and malformed-definition
rejection, oversized fragmentation, exact pending-state reset, same-process
fresh admission and slot reuse, valid acknowledgement, responsiveness, and
clean shutdown.

The final inherited proof matrix also passed:

| Check | Result |
|---|---|
| handshake | PASS |
| disconnected-slot reuse | PASS |
| netchan Proof A/B | PASS |
| serverinfo Proof A/B | PASS |
| resource-manifest Proof A/B | PASS |
| fragmented-manifest Proof A/B | PASS |
| Prompt 238 continuation Proof A/B | PASS |
| feature-off acceptance and drift | PASS |
| PowerShell parser validation | PASS |
| `git diff --check` | PASS |

Feature-off verification recorded `wrapper_full_run_passed=1`, no public or
LAN socket, no real-client invocation, all connection/signon opt-ins disabled,
and `normal_host_behavior_changed=0`.

Every automated proof bound only to `127.0.0.1`, retained no raw packet dump,
and finished with `put_in_server=0`, `spawned=0`, and `active=0`.

## Stock-client result

The initial pre-change run reproduced the Prompt 239 boundary with the same
unmodified client and installed runtime data. It reached resource fragment
completion and asserted because `usercmd_t` had not been registered.

The final run used the installed runtime `delta.lst`, not a fixture:

- delta source: `runtime_game_dir`;
- parsed tables: 7;
- `usercmd_t`: present and accepted;
- bootstrap fragments acknowledged: 7/7;
- resource-manifest fragments acknowledged: 8/8;
- bootstrap acknowledged: yes;
- resource manifest acknowledged: yes;
- previous `ppdesc` assertion: absent;
- client responsive after resource acknowledgement: yes;
- host stable: yes;
- session count: 1;
- `put_in_server=0`;
- `spawned=0`;
- `active=0`.

The final client and host processes were identified by exact owned PIDs and
were cleaned without touching unrelated processes. Temporary sanitized logs
were removed, and installed input hashes were unchanged.

The previous missing-schema boundary is resolved. The first new observed event
is `unreliable_post_resource_client_command`. It is recorded for a later
bounded slice; Prompt 240 does not decode it for gameplay.

## Artifact and Git scope

The intended staged set contains 30 text/source files. The pre-existing
untracked `out/` tree is explicitly excluded. A stat-only worktree report for
`src/tests/goldsrc_resource_manifest_tests.cpp` has no content or mode
difference and is not part of the commit.

Before the authorized commit:

- build artifacts staged: no;
- SDK files staged: no;
- proprietary files staged: no;
- installed client files staged: no;
- packet captures staged: no;
- unrelated changes reverted: no.

After the single authorized commit and normal push, the tracked worktree is
expected to be clean with only the excluded untracked `out/` tree remaining.
The local and remote branch heads must match exactly, and no pull request is
created.

## Limitations

This slice transmits protocol schemas and the inseparable bootstrap tail. It
does not implement user-command gameplay decoding, movement, prediction,
entity or instance baselines, snapshots, packet entities, clientdata updates,
events, resource downloads, consistency enforcement, Game DLL
`ClientPutInServer`, spawn, weapons, damage, or gameplay.

Delivering the delta-description bootstrap does not mean that actual usercmd
processing, movement, entity baselines, snapshots, ClientPutInServer, spawn,
or gameplay are complete.
