# Envelope To Message Writer Mapping

| Envelope candidate | Required primitives | Helper found? | Helper confidence | Missing primitive | Missing policy | Risk |
| --- | --- | --- | --- | --- | --- | --- |
| Reliable server message envelope | sequence/ack, reliable flag, payload length, byte/string writers | payload observation only | low | sequence, ack, reliable bit, length, resend buffer | ordering, width, overflow, retransmit | critical |
| Unreliable server datagram envelope | sequence/ack, payload placement, byte/string writers | `MSG_ONE_UNRELIABLE` label only | low | packet header, payload boundary | ordering, width, overflow | critical |
| Client command envelope | client sequence, qport/session, command payload | no real helper | low | qport, command payload, ack relation | command routing | critical |
| Ack-only envelope | sequence/ack header | no | low | ack field and empty payload rule | empty-packet policy | high |
| Split/fragment envelope | fragment id/count/offset, payload chunks | no | low | all fragment fields | split threshold/reassembly | high |
| Signon message envelope | reliable envelope plus `svc_serverinfo` fields | pseudo helpers only | low | real message id/framing | reliable placement, field order | critical |
| Post-connect serverinfo envelope | first packet envelope plus serverinfo fields | preview only | low | first packet header and message id | stage placement | critical |
| Baseline/resource envelope | baseline/resource ids and payload writers | no | low | baseline ids and field writers | reliable/baseline order | high |
| Reject/disconnect envelope | reason string plus stage envelope | connectionless text only | medium for text, low for envelope | post-connect reject envelope | stage-specific disconnect policy | high |

envelope_message_writer_rows_count: 9
