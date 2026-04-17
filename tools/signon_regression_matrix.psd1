@{
    SuiteName = 'signon-neighbor-surfaces'
    Profiles = @{
        default = @{
            Description = 'Runs the enabled-by-default neighboring-surface matrix entries.'
            Groups = @()
            Surfaces = @()
            FullMatrix = $false
        }
        'checkpoint-extended' = @{
            Description = 'Runs the checkpoint-extended logical neighboring-surface group.'
            Groups = @('checkpoint-extended')
            Surfaces = @()
            FullMatrix = $false
        }
        'full-expanded' = @{
            Description = 'Runs every declarative neighboring-surface matrix entry.'
            Groups = @()
            Surfaces = @()
            FullMatrix = $true
        }
    }
    Entries = @(
        @{
            Name = 'signon-message-cursor-carried-checkpoint-resume-token'
            Groups = @('default-neighbors', 'checkpoint-core', 'checkpoint-extended', 'full-expanded', 'resume-token')
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
            Groups = @('default-neighbors', 'checkpoint-core', 'checkpoint-extended', 'full-expanded', 'resume-token')
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
            Groups = @('default-neighbors', 'checkpoint-core', 'checkpoint-extended', 'claimed-checkpoint', 'full-expanded', 'claimed-checkpoint-resume-allow')
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
            Groups = @('default-neighbors', 'checkpoint-core', 'checkpoint-extended', 'claimed-checkpoint', 'full-expanded', 'claimed-checkpoint-bridge')
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
            Groups = @('default-neighbors', 'checkpoint-core', 'checkpoint-extended', 'claimed-checkpoint', 'full-expanded', 'claimed-checkpoint-bridge', 'claimed-checkpoint-resume-allow')
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
            Groups = @('default-neighbors', 'checkpoint-core', 'checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded', 'claimed-checkpoint-bridge', 'claimed-checkpoint-resume-range')
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
            Groups = @('default-neighbors', 'checkpoint-core', 'checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded', 'claimed-checkpoint-bridge', 'claimed-checkpoint-resume-eof')
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
            Groups = @('default-neighbors', 'checkpoint-core', 'checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded', 'claimed-checkpoint-bridge', 'resumed-denial')
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
        @{
            Name = 'signon-message-cursor-carried-range'
            Groups = @('carryover-chain', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks that the carried cursor can deliver the final bridged range into the target session.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_range_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_range_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'carriedCursorRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedTargetActivated=1'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                    )
                }
                gate = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'carriedCursorRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedTargetActivated=1'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-eof'
            Groups = @('carryover-chain', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks truthful terminal EOF reporting once the carried target cursor exhausts.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_eof_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_eof_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'carriedCursorExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedResumePolicy=carried-target-terminal-eof-no-resume'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-target-terminal-eof-no-resume'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'carriedCursorExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedResumePolicy=carried-target-terminal-eof-no-resume'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-target-terminal-eof-no-resume'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-resume-allow'
            Groups = @('carryover-chain', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks that the carried cursor still exposes one bounded non-exhausted resume allowance.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_resume_allow_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_resume_allow_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=yes'
                        'remainingMessageCount=1'
                        'nonExhaustedCarriedCursorResumeAllowed=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedResumeAllowed=yes'
                        'parsedResumePolicy=carried-target-non-eof-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-target-non-eof-resume-allowed'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=yes'
                        'remainingMessageCount=1'
                        'nonExhaustedCarriedCursorResumeAllowed=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedResumeAllowed=yes'
                        'parsedResumePolicy=carried-target-non-eof-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-target-non-eof-resume-allowed'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint'
            Groups = @('carryover-chain', 'checkpoint-core', 'checkpoint-extended', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks carryover materialization from the source cursor into the target checkpoint state.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'remainingMessageCount=0'
                        'carriedCursorCheckpointed=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedBridgePolicy=carried-cursor-materialized-into-target-checkpoint'
                        'parsedTargetActivated=1'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedBridgePolicy=carried-cursor-materialized-into-target-checkpoint'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'remainingMessageCount=0'
                        'carriedCursorCheckpointed=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedBridgePolicy=carried-cursor-materialized-into-target-checkpoint'
                        'parsedTargetActivated=1'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedBridgePolicy=carried-cursor-materialized-into-target-checkpoint'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-resume-allow'
            Groups = @('carryover-chain', 'checkpoint-core', 'checkpoint-extended', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks that the target carried checkpoint still allows one bounded resume fetch.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_allow_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_allow_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=yes'
                        'remainingMessageCount=1'
                        'nonExhaustedCarriedCheckpointResumeAllowed=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedResumeAllowed=yes'
                        'parsedResumePolicy=carried-checkpoint-non-exhausted-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-non-exhausted-resume-allowed'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=yes'
                        'remainingMessageCount=1'
                        'nonExhaustedCarriedCheckpointResumeAllowed=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedResumeAllowed=yes'
                        'parsedResumePolicy=carried-checkpoint-non-exhausted-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-non-exhausted-resume-allowed'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-resume-range'
            Groups = @('carryover-chain', 'checkpoint-core', 'checkpoint-extended', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks range delivery after the carried target checkpoint resumes from its bounded cursor.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_range_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_range_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'carriedCheckpointResumeRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedResumePolicy=carried-checkpoint-target-resume-range-complete'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-target-resume-range-complete'
                    )
                }
                gate = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'carriedCheckpointResumeRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedResumePolicy=carried-checkpoint-target-resume-range-complete'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-target-resume-range-complete'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-resume-eof'
            Groups = @('carryover-chain', 'checkpoint-core', 'checkpoint-extended', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks EOF reporting once the carried target checkpoint resume consumes the last staged record.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_eof_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_eof_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'carriedCheckpointResumeExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedResumePolicy=carried-checkpoint-resumed-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-resumed-terminal-eof'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'carriedCheckpointResumeExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedResumePolicy=carried-checkpoint-resumed-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-resumed-terminal-eof'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-resumed-denial'
            Groups = @('carryover-chain', 'checkpoint-core', 'checkpoint-extended', 'eof-advance', 'full-expanded', 'resumed-denial')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks truthful denial after the carried target checkpoint has already resumed to exhaustion.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resumed_denial_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resumed_denial_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=no'
                        'denialReason=carried-checkpoint-resumed-exhausted'
                        'carriedCheckpointResumedDenied=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedDenialReason=carried-checkpoint-resumed-exhausted'
                        'parsedResumePolicy=carried-checkpoint-resumed-exhausted-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-resumed-exhausted-denied'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=no'
                        'denialReason=carried-checkpoint-resumed-exhausted'
                        'carriedCheckpointResumedDenied=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedDenialReason=carried-checkpoint-resumed-exhausted'
                        'parsedResumePolicy=carried-checkpoint-resumed-exhausted-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-resumed-exhausted-denied'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-advance'
            Groups = @('carryover-chain', 'checkpoint-extended', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks direct advance delivery from the carried target checkpoint without entering the resume branch.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_advance_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_advance_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'carriedCheckpointAdvancedRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedBridgePolicy=carried-checkpoint-non-exhausted-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                    )
                }
                gate = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'carriedCheckpointAdvancedRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedBridgePolicy=carried-checkpoint-non-exhausted-resume-allowed'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-eof'
            Groups = @('carryover-chain', 'checkpoint-extended', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks direct EOF reporting once the carried target checkpoint advance is exhausted.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_eof_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_eof_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'carriedCheckpointExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedResumePolicy=carried-checkpoint-terminal-eof-no-resume'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-terminal-eof-no-resume'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'carriedCheckpointExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedResumePolicy=carried-checkpoint-terminal-eof-no-resume'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-terminal-eof-no-resume'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-resume-denial'
            Groups = @('carryover-chain', 'checkpoint-extended', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks truthful denial once the carried target checkpoint itself is exhausted.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_denial_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_resume_denial_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=no'
                        'denialReason=carried-checkpoint-exhausted'
                        'carriedCheckpointResumeDenied=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedDenialReason=carried-checkpoint-exhausted'
                        'parsedResumePolicy=carried-checkpoint-exhausted-resume-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-exhausted-resume-denied'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=no'
                        'denialReason=carried-checkpoint-exhausted'
                        'carriedCheckpointResumeDenied=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedDenialReason=carried-checkpoint-exhausted'
                        'parsedResumePolicy=carried-checkpoint-exhausted-resume-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=carried-checkpoint-exhausted-resume-denied'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-range'
            Groups = @('checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks range delivery once the carried target checkpoint has been claimed.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_range_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_range_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'claimedCheckpointRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedTargetActivated=1'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                    )
                }
                gate = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=2'
                        'lastFetchedMessageIndices=3'
                        'claimedCheckpointRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=3'
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-range-delivered'
                        'parsedFetchedMessageIndices=3'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-range-delivered'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-eof'
            Groups = @('checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks EOF reporting for the claimed carried target checkpoint.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_eof_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_eof_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'claimedCheckpointExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedResumePolicy=claimed-checkpoint-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimed-checkpoint-terminal-eof'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=2'
                        'eof=yes'
                        'exhausted=yes'
                        'claimedCheckpointExhausted=1'
                    )
                    Probe = @(
                        'attempts=3'
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-eof'
                        'parsedResumePolicy=claimed-checkpoint-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-eof'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-resume-denial'
            Groups = @('checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks truthful denial once the claimed carried target checkpoint has exhausted.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_denial_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_denial_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=no'
                        'denialReason=claimed-checkpoint-exhausted'
                        'claimedCheckpointResumeDenied=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedDenialReason=claimed-checkpoint-exhausted'
                        'parsedResumePolicy=claimed-checkpoint-exhausted-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimed-checkpoint-exhausted-denied'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=2'
                        'resumeAllowed=no'
                        'denialReason=claimed-checkpoint-exhausted'
                        'claimedCheckpointResumeDenied=1'
                    )
                    Probe = @(
                        'attempts=3'
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-resume-denied'
                        'parsedResumePolicy=claimed-checkpoint-exhausted-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-resume-denied'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-resume-range'
            Groups = @('checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded', 'claimed-checkpoint-resume-range')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks resumed range delivery from the claimed carried target checkpoint.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_range_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_range_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'claimedCheckpointResumeRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedResumePolicy=claimed-checkpoint-positive-resume-range-complete'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimed-checkpoint-positive-resume-range-complete'
                    )
                }
                gate = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=1'
                        'lastFetchedMessageIndices=3'
                        'claimedCheckpointResumeRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=2'
                        'accepted=1'
                        'rejected=1'
                        'lastRejectReason=invalid-claimed-resume-range-request'
                        'parsedResumePolicy=claimed-checkpoint-positive-resume-range-complete'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=1'
                        'lastRejectReason=invalid-claimed-resume-range-request'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-resume-eof'
            Groups = @('checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded', 'claimed-checkpoint-resume-eof')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks resumed EOF reporting from the claimed carried target checkpoint.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_eof_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_resume_eof_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'claimedCheckpointResumeExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedResumePolicy=claimed-resumed-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimed-resumed-terminal-eof'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=2'
                        'eof=yes'
                        'exhausted=yes'
                        'claimedCheckpointResumeExhausted=1'
                    )
                    Probe = @(
                        'attempts=3'
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-resume-eof'
                        'parsedResumePolicy=claimed-resumed-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=2'
                        'lastRejectReason=already-claimed-resume-eof'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-checkpoint-advance'
            Groups = @('checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded', 'claimed-checkpoint-bridge')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks advance delivery after the claimed cursor has been bridged into the claimant checkpoint.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_advance_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=0'
                        'lastFetchedMessageIndices=3'
                        'claimedCheckpointAdvancedRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedFetchedMessageIndices=3'
                        'parsedBridgePolicy=claimed-cursor-materialized-into-claimant-checkpoint'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedBridgePolicy=claimed-cursor-materialized-into-claimant-checkpoint'
                    )
                }
                gate = @{
                    Surface = @(
                        'requestedMessageCount=1'
                        'accepted=1'
                        'rejected=3'
                        'lastFetchedMessageIndices=3'
                        'claimedCheckpointAdvancedRangeDelivered=1'
                    )
                    Probe = @(
                        'attempts=4'
                        'accepted=1'
                        'rejected=3'
                        'lastRejectReason=already-claimed-checkpoint-advanced'
                        'parsedBridgePolicy=claimed-cursor-materialized-into-claimant-checkpoint'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=3'
                        'lastRejectReason=already-claimed-checkpoint-advanced'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-checkpoint-eof'
            Groups = @('checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded', 'claimed-checkpoint-bridge')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks EOF reporting once the bridged claimant checkpoint advance is exhausted.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_eof_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'eof=yes'
                        'exhausted=yes'
                        'claimedCheckpointBridgeExhausted=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedEof=yes'
                        'parsedResumePolicy=claimant-checkpoint-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimant-checkpoint-terminal-eof'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=3'
                        'eof=yes'
                        'exhausted=yes'
                        'claimedCheckpointBridgeExhausted=1'
                    )
                    Probe = @(
                        'attempts=4'
                        'accepted=1'
                        'rejected=3'
                        'lastRejectReason=already-claimed-checkpoint-eof'
                        'parsedResumePolicy=claimant-checkpoint-terminal-eof'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=3'
                        'lastRejectReason=already-claimed-checkpoint-eof'
                    )
                }
            }
        }
        @{
            Name = 'signon-message-cursor-carried-checkpoint-claimed-checkpoint-resume-denial'
            Groups = @('checkpoint-extended', 'claimed-checkpoint', 'eof-advance', 'full-expanded', 'claimed-checkpoint-bridge')
            EnabledByDefault = $false
            Mode = 'happy-gate'
            Notes = 'Checks truthful denial once the bridged claimant checkpoint itself is exhausted.'
            SurfaceNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denial_surface:'
            ProbeNeedle = 'dedicated_signon_message_cursor_carried_checkpoint_claimed_checkpoint_resume_denial_probe:'
            Runs = @{
                happy = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=0'
                        'resumeAllowed=no'
                        'denialReason=claimant-checkpoint-exhausted'
                        'claimedCheckpointBridgeResumeDenied=1'
                    )
                    Probe = @(
                        'attempts=1'
                        'accepted=1'
                        'rejected=0'
                        'parsedDenialReason=claimant-checkpoint-exhausted'
                        'parsedResumePolicy=claimant-checkpoint-exhausted-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=0'
                        'parsedResumePolicy=claimant-checkpoint-exhausted-denied'
                    )
                }
                gate = @{
                    Surface = @(
                        'accepted=1'
                        'rejected=4'
                        'resumeAllowed=no'
                        'denialReason=claimant-checkpoint-exhausted'
                        'claimedCheckpointBridgeResumeDenied=1'
                    )
                    Probe = @(
                        'attempts=5'
                        'accepted=1'
                        'rejected=4'
                        'lastRejectReason=already-claimed-checkpoint-resume-denied'
                        'parsedResumePolicy=claimant-checkpoint-exhausted-denied'
                    )
                    Key = @(
                        'accepted=1'
                        'rejected=4'
                        'lastRejectReason=already-claimed-checkpoint-resume-denied'
                    )
                }
            }
        }
    )
}
