# Qport Session Implementation Execution Plan Validation Comparison

Prompt: $promptId

The happy scenario validates only an implementation-skeleton execution plan. Rejected scenarios prove missing dependencies and unsafe requested actions do not create a plan.

| Check | Expected | Happy value | Result |
| --- | ---: | ---: | --- |
| implementation_execution_plan_created | 1 | 1 | pass |
| implementation_execution_plan_validated | 1 | 1 | pass |
| policy_review_passed | 1 | 1 | pass |
| execution_skeleton_ci_drift_gate_passed | 1 | 1 | pass |
| final_execution_policy_gate_passed | 1 | 1 | pass |
| runtime_skeleton_ci_drift_gate_passed | 1 | 1 | pass |
| offline_fixture_validator_passed | 1 | 1 | pass |
| capture_policy_gate_passed | 1 | 1 | pass |
| dry_run_validator_passed | 1 | 1 | pass |
| wrapper_validation_passed | 1 | 1 | pass |
| capture_implementation_added | 0 | 0 | pass |
| capture_executed | 0 | 0 | pass |
| capture_runtime_executed | 0 | 0 | pass |
| socket_open_attempted | 0 | 0 | pass |
| datagram_sent | 0 | 0 | pass |
| datagram_received | 0 | 0 | pass |
| real_client_binary_invoked | 0 | 0 | pass |
| netchan_runtime_started | 0 | 0 | pass |

All 25 proof manifests are under manifests/; all 25 parsed proof summaries are under summaries/.
