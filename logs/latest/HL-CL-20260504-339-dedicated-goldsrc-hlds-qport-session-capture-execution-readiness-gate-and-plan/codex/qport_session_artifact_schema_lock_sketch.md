# Qport/session Artifact Schema Lock Sketch

This is a plan-only requirement sketch. It defines future artifact schema requirements without collecting qport/session byte evidence.

## Required Future Artifact Fields

- Prompt ID, branch, source commit, artifact commit, run label, and scenario.
- All readiness gate prerequisite pass/fail fields.
- All allowed-now fields.
- All executed/opened/sent/received/runtime/client/evidence/compatibility fields.
- Timeout and cleanup policy version.
- Socket policy review status and datagram policy review status.
- Explicit compatibility claim level.

## Lock Conditions

- A future prompt must reject schema removal, field rename, or type drift unless a separate schema-lock policy prompt approves the change.
- A future prompt must not treat a schema lock as permission to execute capture or open sockets.
