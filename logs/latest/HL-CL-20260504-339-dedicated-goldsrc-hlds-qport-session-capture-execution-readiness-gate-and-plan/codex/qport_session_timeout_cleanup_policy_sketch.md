# Qport/session Timeout Cleanup Policy Sketch

This is a plan-only requirement sketch. It is not capture execution and opens no socket.

## Required Future Policy Points

- Every future no-client execution readiness path must define bounded startup, wait, retry, and shutdown windows before any execution prompt is considered.
- Timeout defaults must be deterministic and included in generated artifacts.
- Cleanup must describe how temporary files, in-memory state, and incomplete summaries are handled after rejection or timeout.
- Cleanup must preserve evidence that no socket, datagram, real-client, connect, signon, or netchan runtime path was started.
- Any timeout policy change must be reviewed before any capture execution prompt can claim readiness.
