# Message Writing Inventory

Prompt ID: HL-CL-20260504-298-dedicated-goldsrc-hlds-netchan-signon-serverinfo-static-dependency-inventory

No production MSG_Write, SZ_Write, sizebuf, or reusable real network datagram writer was found in source during this inventory. Existing write helpers are diagnostic observation helpers or query/info-specific helpers, not post-connect/signon byte evidence.

| Helper | File | Classification | Future diagnostic fixture value | Real serverinfo gap |
| --- | --- | --- | --- | --- |
| FrameMessageBuffer::Begin | src/game_api/server_frame_loop.cpp | Diagnostic frame message observation | Records callback context | No real packet boundary/framing. |
| FrameMessageBuffer::End | src/game_api/server_frame_loop.cpp | Diagnostic frame message observation | Records callback completion | No real packet flush semantics. |
| FrameMessageBuffer::Abort | src/game_api/server_frame_loop.cpp | Diagnostic frame message observation | Records negative observations | No real failure semantics. |
| FrameMessageBuffer::WriteByte | src/game_api/server_frame_loop.cpp | Diagnostic callback write record | Useful for local callback inventories | No opcode/message id evidence. |
| FrameMessageBuffer::WriteChar | src/game_api/server_frame_loop.cpp | Diagnostic callback write record | Useful for local callback inventories | Signedness unknown. |
| FrameMessageBuffer::WriteShort | src/game_api/server_frame_loop.cpp | Diagnostic callback write record | Useful for local callback inventories | Endianness and field contract unknown. |
| FrameMessageBuffer::WriteLong | src/game_api/server_frame_loop.cpp | Diagnostic callback write record | Useful for local callback inventories | Endianness and field contract unknown. |
| FrameMessageBuffer::WriteAngle | src/game_api/server_frame_loop.cpp | Diagnostic callback write record | Possible entity/baseline observation | Real angle encoding unknown. |
| FrameMessageBuffer::WriteCoord | src/game_api/server_frame_loop.cpp | Diagnostic callback write record | Possible entity/baseline observation | Real coord encoding unknown. |
| FrameMessageBuffer::WriteString | src/game_api/server_frame_loop.cpp | Diagnostic callback write record | Useful for local callback inventories | String termination/encoding evidence missing. |
| FrameMessageBuffer::WriteEntity | src/game_api/server_frame_loop.cpp | Diagnostic callback write record | Possible entity observation | Entity index encoding unknown. |
| StubWriteByte/StubWriteShort/StubWriteLong/StubWriteString | src/game_api/hl_server_module.cpp | Engine callback shim | Bridges game DLL message callbacks into FrameMessageBuffer | No real network packet context. |
| AppendByteLengthPrefixedText | src/game_api/hl_server_module.cpp | Query/info byte helper | Safe for query/info-style fixtures | Not safe for post-connect/signon unless evidence proves length-prefixing. |

Message writing helper groups counted: 13.

Next safe task: narrow netchan/message-writing static inventory before any post-connect or signon serverinfo field contract work.
