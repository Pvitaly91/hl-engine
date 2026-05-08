# Buffer And Overflow Policy Inventory

Source: docs/diagnostic/hlds/netchan_message_writing_static_inventory.md

Found:
- FrameMessageBuffer owns diagnostic records and retains at most 32 completed messages.
- FrameMessageBuffer::AddWrite records payload byte counts and write text only when an active message exists.
- std::vector<unsigned char> byte buffers are used by query/info and pseudo signon diagnostic helpers.
- AppendByteLengthPrefixedText clamps diagnostic pseudo text to 255 bytes.
- AppendLittleEndianShort writes a diagnostic pseudo little-endian short.

Missing:
- central sizebuf-style abstraction
- real overflow flag
- real message max size constants
- central real network message writer
- central bit writer
- real reliable/unreliable channel buffer
- stage-specific post-connect/signon string and numeric policy

Conclusion: buffer and overflow policy remains insufficient for real post-connect/signon byte builders.
