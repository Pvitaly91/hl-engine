@{
    SuiteName = 'signon-neighbor-surfaces'
    Entries = @(
        @{
            Name = 'signon-message-cursor-carried-checkpoint-resume-token'
            Groups = @('resume-token')
            EnabledByDefault = $true
            Mode = 'happy-gate'
            Notes = 'Covers carried checkpoint resume-token issuance before a claim is attempted.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_token_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_token_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'tokenPurpose=future-resume-claim-only'
                        'claimValidationImplemented=no'
                        'carriedCheckpointResumeTokenIssued=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedTokenState=issued-not-claimed'
                        'parsedClaimValidationImplemented=no'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedTokenState=issued-not-claimed'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=5'
                        'tokenPurpose=future-resume-claim-only'
                        'claimValidationImplemented=no'
                        'carriedCheckpointResumeTokenIssued=1'
                    )
                    Probe = @(
                        'attempts=6'
                        'accepted=1'
                        'rejected=5'
                        'lastRejectReason=already-token-issued'
                        'parsedTokenState=issued-not-claimed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=5'
                        'lastRejectReason=already-token-issued'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-resume-token-claim'
            Groups = @('resume-token')
            EnabledByDefault = $true
            Mode = 'happy-gate'
            Notes = 'Covers deterministic loopback claiming of the carried checkpoint resume token.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_token_claim_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_token_claim_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'claimValidation=deterministic-loopback-only'
                        'carriedCheckpointResumeTokenClaimed=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedClaimStatus=accepted'
                        'parsedClaimValidation=deterministic-loopback-only'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedClaimStatus=accepted'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=4'
                        'claimValidation=deterministic-loopback-only'
                        'carriedCheckpointResumeTokenClaimed=1'
                    )
                    Probe = @(
                        'attempts=5'
                        'accepted=1'
                        'rejected=4'
                        'lastRejectReason=already-claimed'
                        'parsedClaimStatus=accepted'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=4'
                        'lastRejectReason=already-claimed'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-resume-allow'
            Groups = @('claimed-checkpoint', 'claimed-checkpoint-resume-allow')
            EnabledByDefault = $true
            Mode = 'happy-gate'
            Notes = 'Checks that a claimed checkpoint still allows one bounded resume request.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_allow_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_allow_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=yes'
                        'denialReason=none'
                        'remainingMessageCount=1'
                        'nonExhaustedClaimedCheckpointResumeAllowed=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedResumeAllowed=yes'
                        'parsedResumePolicy=claimed-checkpoint-non-exhausted-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimed-checkpoint-non-exhausted-resume-allowed'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=1'
                        'resumeAllowed=yes'
                        'denialReason=none'
                        'remainingMessageCount=1'
                        'nonExhaustedClaimedCheckpointResumeAllowed=1'
                    )
                    Probe = @(
                        'attempts=2'
                        'accepted=1'
                        'rejected=1'
                        'lastRejectReason=invalid-claimed-resume-allow-request'
                        'parsedResumePolicy=claimed-checkpoint-non-exhausted-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=1'
                        'lastRejectReason=invalid-claimed-resume-allow-request'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-checkpoint-bridge'
            Groups = @('claimed-checkpoint', 'claimed-checkpoint-bridge')
            EnabledByDefault = $true
            Mode = 'happy-gate'
            Notes = 'Checks bridge materialization from the claimed cursor into the claimant checkpoint.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_bridge_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'bridgePolicy=claimed-cursor-materialized-into-claimant-checkpoint'
                        'currentMessageCount=2'
                        'remainingMessageCount=0'
                        'claimedCheckpointBridgeMaterialized=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedBridgePolicy=claimed-cursor-materialized-into-claimant-checkpoint'
                        'parsedRemainingMessageCount=1'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedBridgePolicy=claimed-cursor-materialized-into-claimant-checkpoint'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=2'
                        'bridgePolicy=claimed-cursor-materialized-into-claimant-checkpoint'
                        'currentMessageCount=2'
                        'remainingMessageCount=0'
                        'claimedCheckpointBridgeMaterialized=1'
                    )
                    Probe = @(
                        'attempts=3'
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-checkpoint-bridged'
                        'parsedBridgePolicy=claimed-cursor-materialized-into-claimant-checkpoint'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-checkpoint-bridged'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-allow'
            Groups = @('claimed-checkpoint', 'claimed-checkpoint-bridge', 'claimed-checkpoint-resume-allow')
            EnabledByDefault = $true
            Mode = 'happy-gate'
            Notes = 'Checks resume-allow behavior once the claimed-checkpoint bridge has been materialized.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_allow_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_allow_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=yes'
                        'denialReason=none'
                        'currentMessageCount=2'
                        'remainingMessageCount=0'
                        'signonMessageCursorCarriedCheckpointClaimedCheckpointResumeReady=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedResumeAllowed=yes'
                        'parsedResumePolicy=claimant-checkpoint-non-exhausted-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimant-checkpoint-non-exhausted-resume-allowed'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=1'
                        'resumeAllowed=yes'
                        'denialReason=none'
                        'currentMessageCount=2'
                        'remainingMessageCount=0'
                        'signonMessageCursorCarriedCheckpointClaimedCheckpointResumeReady=1'
                    )
                    Probe = @(
                        'attempts=2'
                        'accepted=1'
                        'rejected=1'
                        'lastRejectReason=invalid-claimed-checkpoint-resume-allow-request'
                        'parsedResumePolicy=claimant-checkpoint-non-exhausted-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=1'
                        'lastRejectReason=invalid-claimed-checkpoint-resume-allow-request'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-range'
            Groups = @('claimed-checkpoint', 'claimed-checkpoint-bridge', 'claimed-checkpoint-resume-range')
            EnabledByDefault = $true
            Mode = 'happy-gate'
            Notes = 'Checks range-fetch delivery after the claimed-checkpoint bridge is active.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_range_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'remainingMessageCountAfterResumeFetch=0'
                        'claimedCheckpointBridgeResumeRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedSemanticTags=completion-staged-records-ready-content'
                        'parsedResumePolicy=claimant-checkpoint-positive-resume-range-complete'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimant-checkpoint-positive-resume-range-complete'
                    )
                }
                gate = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=2'
                        'lastFetchedMessageIndices=3'
                        'remainingMessageCountAfterResumeFetch=0'
                        'claimedCheckpointBridgeResumeRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=3'
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-checkpoint-resume-ranged'
                        'parsedResumePolicy=claimant-checkpoint-positive-resume-range-complete'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-checkpoint-resume-ranged'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-eof'
            Groups = @('claimed-checkpoint', 'claimed-checkpoint-bridge', 'claimed-checkpoint-resume-eof')
            EnabledByDefault = $true
            Mode = 'happy-gate'
            Notes = 'Checks EOF/exhaustion reporting after a claimed-checkpoint bridge resume.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_eof_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'remainingMessageCount=0'
                        'claimedCheckpointBridgeResumeExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedExhausted=yes'
                        'parsedResumePolicy=claimant-checkpoint-resumed-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimant-checkpoint-resumed-terminal-eof'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=2'
                        'eof=yes'
                        'exhausted=yes'
                        'remainingMessageCount=0'
                        'claimedCheckpointBridgeResumeExhausted=1'
                    )
                    Probe = @(
                        'attempts=3'
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-checkpoint-resume-eof'
                        'parsedResumePolicy=claimant-checkpoint-resumed-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-checkpoint-resume-eof'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-checkpoint-resumed-denial'
            Groups = @('claimed-checkpoint', 'claimed-checkpoint-bridge', 'resumed-denial')
            EnabledByDefault = $true
            Mode = 'happy-gate'
            Notes = 'Checks truthful denial reporting after the claimed-checkpoint bridge has fully exhausted.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resumed_denial_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=no'
                        'denialReason=claimant-checkpoint-resumed-exhausted'
                        'eof=yes'
                        'exhausted=yes'
                        'claimedCheckpointBridgeResumedDenied=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedResumeAllowed=no'
                        'parsedDenialReason=claimant-checkpoint-resumed-exhausted'
                        'parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=3'
                        'resumeAllowed=no'
                        'denialReason=claimant-checkpoint-resumed-exhausted'
                        'eof=yes'
                        'exhausted=yes'
                        'claimedCheckpointBridgeResumedDenied=1'
                    )
                    Probe = @(
                        'attempts=4'
                        'accepted=1'
                        'rejected=3'
                        'lastRejectReason=already-claimed-checkpoint-resumed-denied'
                        'parsedResumePolicy=claimant-checkpoint-resumed-exhausted-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=3'
                        'lastRejectReason=already-claimed-checkpoint-resumed-denied'
                    )
                }
            }
        }
    )
}
