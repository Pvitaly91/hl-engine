# Policy Review Note

PROMPT-ID: HL-CL-20260504-277-dedicated-goldsrc-hlds-production-loopback-connectionless-socket-pump-localhost-client-smoke-harness

This prompt is diagnostic-only. It is not a real client smoke test, does not launch a Steam Half-Life client, and does not claim HLDS-compatible client compatibility.

The client added here is a bounded localhost UDP datagram harness. It sends getchallenge-shaped and connect-shaped diagnostic datagrams to the explicit diagnostic frame-wired pump from prompt 276, observes challenge-shaped and serverinfo-shaped diagnostic responses, and records the result as machine-readable proof fields.

Safety policy:
- The harness is disabled by default and requires explicit diagnostic probe enablement.
- Client and server sockets are loopback-only.
- Public socket policy is blocked before socket open.
- Non-loopback client policy is denied before socket open.
- No real Steam client binary is invoked.
- No Steam auth, netchan, reliable channel, signon state, resources, baselines, or client admission are started.
- All socket-opening scenarios close client and server sockets deterministically.

This is safe after prompt 276 because prompt 276 already proved explicit diagnostic-only bounded frame-pump wiring, loopback bind policy, shutdown cleanup, repeated start/stop, and malformed/protocol/userinfo rejection. Prompt 277 only adds a small localhost UDP client harness around that explicit diagnostic surface.

Before any real client smoke attempt, the project still needs an explicit production policy review, durable challenge cache policy for real endpoints, final protocol/version compatibility policy, production userinfo policy, real serverinfo wire-format validation, netchan sequencing/ack, reliable/unreliable channel setup, resource/model/sound/event baselines, a signon state machine, client spawn/put-in-server handling, Steam auth or explicit no-auth LAN diagnostic mode, and a separate real-client-smoke planning prompt.
