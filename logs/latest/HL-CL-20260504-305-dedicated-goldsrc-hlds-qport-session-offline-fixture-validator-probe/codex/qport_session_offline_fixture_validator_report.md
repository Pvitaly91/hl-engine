# Qport/session offline fixture validator report

Prompt: HL-CL-20260504-305-dedicated-goldsrc-hlds-qport-session-offline-fixture-validator-probe
Branch: codex/HL-CL-20260401-081-target-runtime-completion-state
Pre-change HEAD: c830498ceba33c4bd38342edf91217bfb304722d
Source commit: 70c4a99b2eace8c549f8379e4f60af4af034d626
Compatibility claim level: diagnostic-qport-session-offline-fixture-validator-only; no real Steam Half-Life or HLDS-compatible client compatibility claimed

The probe is disabled by default and only runs when `--hlds-qport-session-offline-fixture-validator-probe` is present. It reads the offline qport/session policy and fixture files, performs deterministic metadata and safety checks, and emits the `hlds_qport_session_offline_fixture_validator` summary line.

The validator does not implement capture, packet capture, a real wire builder, a real wire parser, socket behavior, real client behavior, getchallenge/connect/post-connect/signon runtime paths, netchan runtime, auth, reliable/unreliable channel startup, resource baseline startup, signon state, or client admission.

Runtime proof set: all 13 required scenarios passed. The happy path loaded 5 fixture files and kept real-client, socket, capture, qport-promotion, address-scoped challenge promotion, connect, post-connect, signon, netchan, and normal host behavior markers at zero.

Next prompt: HL-CL-20260504-306-dedicated-goldsrc-hlds-qport-session-no-client-capture-policy-gate.
