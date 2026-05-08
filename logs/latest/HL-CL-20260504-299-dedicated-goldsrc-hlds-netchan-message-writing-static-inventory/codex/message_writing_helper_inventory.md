# Message-Writing Helper Inventory

Source: docs/diagnostic/hlds/netchan_message_writing_static_inventory.md

The focused inventory found diagnostic callback recorders (FrameMessageBuffer, StubMessageBegin, StubMessageEnd, and StubWrite*), connectionless query/info byte helpers (AppendGoldSrcCString, ReadGoldSrcCString, BuildGoldSrcInfoRequest, BuildGoldSrcInfoResponse, ParseGoldSrcInfoResponse), and pseudo signon report helpers (AppendByteLengthPrefixedText, AppendLittleEndianShort, BuildDedicatedSignon*).

These helpers are diagnostic-only. The callback recorders track payload byte counts and text summaries; they do not serialize a wire buffer. The query/info helpers are valid only for connectionless query/info. The pseudo signon helpers emit prompt-era diagnostic marker families, not real netchan or real signon-time serverinfo packets.

Counts:
- message_writer_symbols_found_count=92
- message_reader_symbols_found_count=0
- reusable_message_helpers_count=13
- diagnostic_only_helpers_count=18
- production_helpers_count=0
- unknown_helpers_count=0

Conclusion: helpers can support future diagnostic fixture/report work, but not a real post-connect/signon serverinfo builder without separate byte-level evidence.
