# Client Observed Query/Info Response Comparison

Prompt: HL-CL-20260504-289-dedicated-goldsrc-hlds-connectionless-query-info-byte-level-loopback-query-response-swap

The diagnostic client sent a localhost-only connectionless query/info request and observed the byte-level response produced by the prompt 288 path integration. The observed response matched the byte builder output and parsed as the selected connectionless query/info fixture.

| Item | Value |
| --- | --- |
| Selected fixture | connectionless_query_info_candidate |
| Stage | connectionless_query |
| Response bytes | 67 |
| Safe hex preview | FFFFFFFF6D3132372E302E302E313A3000484C656E67696E6520546573742053657276657200633061300076616C76650048616C662D4C696665000004306477000000 |
| Safe text preview | ....m127.0.0.1:0|HLengine Test Server|c0a0|valve|Half-Life||.0dw||| |
| Client received response | 1 |
| Client response shape valid | 1 |
| Query/info is post-connect serverinfo | no |
| Query/info is signon-time serverinfo | no |
| Public socket opened | 0 |
| Real client binary invoked | 0 |

The comparison is diagnostic-only and does not imply HLDS/Steam client compatibility because it exercises a deterministic localhost query/info datagram and response only, with no real client, no public network, no auth, no netchan, no signon, no resource baselines, and no client admission.
