# Prompt 239 handoff

## Outcome

Prompt 239 is precisely blocked, not complete. Observation proved that the
stock client accepts the final fragmented resource payload and then blocks in
its outgoing `MSG_WriteUsercmd` path because `usercmd_t` has no installed
delta description. The final covering netchan acknowledgement is therefore
never transmitted.

Implementing the prerequisite would violate Prompt 239's explicit
delta-descriptions non-goal. No fragment-only workaround can satisfy the
stock-evidence completion gate without faking completion, so no source
correction, claimed solution commit, or push was made.

## Repository identity

- workspace: `D:\DEV\CPP\HL-Engine`;
- origin: `https://github.com/Pvitaly91/hl-engine.git`;
- baseline commit: `f2cbaeab2f05c4780927ef70ad156f9d086e3452`;
- task branch: `codex/goldsrc-stock-fragment-completion-slice`;
- SDK commit: `b1b5cf5892918535619b2937bb927e46cb097ba1`;
- stock client: Half-Life `1.1.2.2`, Steam build `15961492`;
- local reference server used: no;
- public reference: ReHLDS commit
  `0124d56c3d888d922eb045775f71c6682ad1226f`.

The baseline source worktree contained only the pre-existing untracked `out/`
tree. It was not staged or modified for source control.

## Observation result

The Release Win32 host was run on a dynamically selected `127.0.0.1` UDP port
with the installed read-only `valve` directory, `c0a0`, one client slot, normal
fragment mode, and the authoritative runtime manifest.

The stock client acknowledged fragments one through seven. The last client
packet covered server sequence 17. The host then sent the final short fragment
on server sequence 18 and a normal sequence-19 packet. The client accepted
reassembly but asserted on `ppdesc && *ppdesc` before it could encode the next
outgoing user command. No packet covering server sequence 18 arrived, so the
host correctly remained in `resource_manifest_sent_awaiting_ack` and timed
out. It never put the client in the Game DLL, spawned it, or marked it active.

The exact evidence and reference links are in
`docs/compatibility/goldsrc_stock_fragment_completion.md`.

## Diagnosis

- original boundary: `stock_fragment_completion_timeout`;
- mismatch category:
  `application_payload_missing_usercmd_delta_description`;
- affected component: the pre-resource signon payload/order;
- unaffected components: UDP, connectionless admission, endpoint routing,
  fragment metadata, fragment ranges, payload transform, reliable toggles,
  acknowledgement comparison, retransmission identity, resource enumeration,
  Game DLL lifecycle, spawn, movement, and gameplay;
- smallest correction: implement and send the bounded stock-compatible
  delta-description stream before resources;
- scope blocker: Prompt 239 section 19 says not to implement delta
  descriptions.

The public reference order is server info, extra info, delta descriptions,
movevars, and then resources. The current implementation skips from extra info
to the resource phase. The stock assertion and lack of a post-reassembly
packet are the direct observable consequence.

## Completed verification

The complete pre-change regression matrix passed:

| Check | Result |
|---|---|
| Release Win32 host and five test targets | PASS |
| CTest | PASS (5/5) |
| Handshake proof | PASS |
| Disconnected-slot-reuse proof | PASS |
| Netchan Proof A/B | PASS |
| Server-info Proof A/B | PASS |
| Resource-manifest Proof A/B | PASS |
| Fragmented-manifest Proof A/B | PASS |
| Prompt 238 continuation Proof A/B | PASS |
| Feature-off regression | PASS |
| Normal host behavior changed | 0 |

The new Prompt 239 Proof A/B were not authored or run because their required
stock-compatible completion behavior cannot exist until the prohibited
prerequisite is implemented.

## Missing completion artifacts

Three required completion artifacts remain:

1. the authorized bounded delta-description correction;
2. new unit coverage for the corrected contract;
3. the stock fragment-completion Proof A/B script and passing runs.

The compatibility observation and this handoff are present. Build artifacts,
SDK files, installed client files, proprietary files, and `out/` are not
staged.

## Continuation contract

Resume only with explicit authority to implement the bounded
delta-description prerequisite. Preserve the existing fragment transport.
Require the unmodified stock client to send a real packet covering the final
fragment carrier with the correct reliable state. Do not infer completion from
all-fragments-sent. Once that evidence exists, clear the transfer once,
advance signon once, run the new Proof A/B and the full regression matrix, and
record the first subsequent stock-client event as the next boundary.

Current result:

- `timeout_reproduced=yes`;
- `fragment_completion_trace_observed=yes`;
- `earliest_completion_mismatch_identified=yes`;
- `previous_fragment_timeout_resolved=no`;
- `stock_client_tested=yes`;
- `stock_fragment_transfer_completed=no`;
- `stock_client_advanced_past_fragment_completion=no`;
- `next_observed_boundary_recorded=no`;
- `safe_to_commit=no`;
- `missing_artifact_count=3`;
- `push=not_attempted`;
- `pull_request=none`;
- `blocker=delta_descriptions_explicitly_out_of_scope`.
