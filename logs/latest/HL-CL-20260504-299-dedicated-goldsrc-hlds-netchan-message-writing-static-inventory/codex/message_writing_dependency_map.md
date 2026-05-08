# Message-Writing Dependency Map

Source: docs/diagnostic/hlds/netchan_message_writing_static_inventory.md

Rows covered: marker/header, opcode/message id, protocol/version, server count/spawn count, map name, game directory, hostname, max clients/player slot, checksum/CRC, model baseline, sound baseline, event baseline, resource list, client data, disconnect/reject message, reliable channel envelope, unreliable datagram envelope, and netchan sequence/ack.

Summary:
- Query/info marker/header, strings, and response fields have diagnostic byte helpers inside the closed connectionless query/info boundary.
- Pseudo signon report helpers have byte-vector helpers, but they are not real signon or netchan evidence.
- Real reliable envelope, unreliable envelope, and netchan sequence/ack helpers were not found.
- Baseline/resource contracts were not found.

Dependency map rows count: 18

Safe next task: static netchan sequence/ack contract inventory.
