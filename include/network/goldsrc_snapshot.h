#pragma once

#include "network/goldsrc_world_baseline.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace hl::network
{
inline constexpr std::uint8_t kGoldSrcServerTimeOpcode = 7u;
inline constexpr std::uint8_t kGoldSrcClientDataOpcode = 15u;
inline constexpr std::uint8_t kGoldSrcPacketEntitiesOpcode = 40u;
inline constexpr std::uint8_t kGoldSrcDeltaPacketEntitiesOpcode = 41u;
inline constexpr std::uint8_t kGoldSrcClientDeltaOpcode = 4u;
inline constexpr std::size_t kGoldSrcMaximumSnapshotBytes = 1200u;
inline constexpr std::size_t kGoldSrcMaximumSnapshotEntities = 2048u;
inline constexpr std::size_t kGoldSrcMaximumSnapshotWeapons = 64u;
inline constexpr std::size_t kGoldSrcClientFrameHistoryDepth = 64u;
inline constexpr std::uint32_t kGoldSrcServerFrameMask = 0x3FFFFFFFu;
inline constexpr double kGoldSrcDefaultSnapshotRateHz = 20.0;
inline constexpr double kGoldSrcMinimumSnapshotRateHz = 10.0;
inline constexpr double kGoldSrcMaximumSnapshotRateHz = 30.0;
inline constexpr std::size_t kGoldSrcSnapshotIntervalHistogramBuckets =
    1002u;

struct GoldSrcClientDataState final
{
    GoldSrcDecodedDeltaRecord state;
};

struct GoldSrcWeaponState final
{
    std::uint8_t weapon_index = 0u;
    GoldSrcDecodedDeltaRecord state;
};

struct GoldSrcSnapshotEntityState final
{
    std::uint16_t entity_index = 0u;
    GoldSrcBaselineKind kind = GoldSrcBaselineKind::kEntity;
    GoldSrcDecodedDeltaRecord state;
};

struct GoldSrcServerFrame final
{
    std::uint32_t frame_id = 0u;
    float server_time = 0.0f;
    GoldSrcClientDataState clientdata;
    std::vector<GoldSrcWeaponState> weapons;
    std::vector<GoldSrcSnapshotEntityState> entities;
};

struct GoldSrcPlayerSnapshotInput final
{
    GoldSrcSnapshotEntityState entity;
    bool include_local_entity = true;
    GoldSrcClientDataState clientdata;
    std::vector<GoldSrcWeaponState> weapons;
    std::vector<GoldSrcSnapshotEntityState> remote_entities;
};

enum class GoldSrcPlayerSnapshotApplyStatus
{
    kApplied,
    kNullFrame,
    kInvalidMaximumClients,
    kInvalidEntityIndex,
    kWrongEntityKind,
    kInvalidPlayerState,
    kInvalidClientData,
    kInvalidWeaponData,
    kDuplicateEntity,
    kEntityCountExceeded,
};

std::string_view ReasonFor(GoldSrcPlayerSnapshotApplyStatus status) noexcept;

GoldSrcPlayerSnapshotApplyStatus ApplyGoldSrcPlayerSnapshot(
    GoldSrcServerFrame* frame,
    const GoldSrcPlayerSnapshotInput& player,
    std::uint16_t maximum_clients) noexcept;

struct GoldSrcFirstSnapshotPayload final
{
    std::array<std::uint8_t, kGoldSrcMaximumSnapshotBytes> bytes{};
    std::size_t size = 0u;
};

struct GoldSrcFirstSnapshotBundle final
{
    GoldSrcServerFrame frame;
    GoldSrcFirstSnapshotPayload payload;
};

enum class GoldSrcSnapshotCodecStatus
{
    kOk,
    kNullInput,
    kEmptyPayload,
    kPayloadTooLarge,
    kInvalidFrame,
    kNonFiniteServerTime,
    kNonMonotonicServerTime,
    kMissingDeltaTable,
    kDuplicateDeltaTable,
    kInvalidDeltaState,
    kUnsupportedFieldEncoding,
    kValueOutOfRange,
    kInvalidWeaponIndex,
    kWeaponOrderInvalid,
    kEntityCountExceeded,
    kInvalidEntityIndex,
    kDuplicateEntity,
    kEntityOrderInvalid,
    kMissingEntityBaseline,
    kEntityKindMismatch,
    kWrongMessageOrder,
    kUnexpectedPreviousFrame,
    kInvalidClientData,
    kInvalidWeaponData,
    kInvalidPacketEntities,
    kUnexpectedDeltaPacket,
    kMissingEntityTerminator,
    kNonZeroPadding,
    kTrailingData,
    kOutputCapacityExceeded,
};

std::string_view ReasonFor(GoldSrcSnapshotCodecStatus status) noexcept;

struct GoldSrcSnapshotEncodeResult final
{
    GoldSrcSnapshotCodecStatus status =
        GoldSrcSnapshotCodecStatus::kInvalidFrame;
    GoldSrcFirstSnapshotPayload payload;
    std::string_view failure_table;
    std::string_view failure_field;

    bool ok() const noexcept
    {
        return status == GoldSrcSnapshotCodecStatus::kOk;
    }
};

struct GoldSrcSnapshotDecodeResult final
{
    GoldSrcSnapshotCodecStatus status =
        GoldSrcSnapshotCodecStatus::kEmptyPayload;
    GoldSrcServerFrame frame;
    std::size_t bytes_consumed = 0u;

    bool ok() const noexcept
    {
        return status == GoldSrcSnapshotCodecStatus::kOk;
    }
};

struct GoldSrcSnapshotBuildResult final
{
    GoldSrcSnapshotCodecStatus status =
        GoldSrcSnapshotCodecStatus::kInvalidFrame;
    GoldSrcFirstSnapshotBundle bundle;
    std::string_view failure_table;
    std::string_view failure_field;

    bool ok() const noexcept
    {
        return status == GoldSrcSnapshotCodecStatus::kOk;
    }
};

GoldSrcSnapshotEncodeResult EncodeGoldSrcFirstSnapshot(
    const GoldSrcServerFrame& frame,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity = kGoldSrcMaximumSnapshotBytes) noexcept;

GoldSrcSnapshotDecodeResult DecodeGoldSrcFirstSnapshot(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t frame_id,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry) noexcept;

GoldSrcSnapshotBuildResult BuildGoldSrcFirstSnapshot(
    std::uint32_t frame_id,
    float server_time,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity = kGoldSrcMaximumSnapshotBytes,
    const GoldSrcPlayerSnapshotInput* player = nullptr) noexcept;

enum class GoldSrcSnapshotKind
{
    kFull,
    kDelta,
};

std::string_view NameFor(GoldSrcSnapshotKind kind) noexcept;

enum class GoldSrcEntityDeltaOperationKind
{
    kAdd,
    kUpdate,
    kRemove,
    kUnchanged,
};

struct GoldSrcEntityDeltaOperation final
{
    GoldSrcEntityDeltaOperationKind kind =
        GoldSrcEntityDeltaOperationKind::kUnchanged;
    std::uint16_t entity_index = 0u;
    const GoldSrcSnapshotEntityState* previous = nullptr;
    const GoldSrcSnapshotEntityState* current = nullptr;
};

enum class GoldSrcEntityDiffStatus
{
    kOk,
    kInvalidBaseFrame,
    kInvalidCurrentFrame,
    kIncompatibleEntityKind,
    kOperationCountExceeded,
};

std::string_view ReasonFor(GoldSrcEntityDiffStatus status) noexcept;

struct GoldSrcEntityDiffResult final
{
    GoldSrcEntityDiffStatus status =
        GoldSrcEntityDiffStatus::kInvalidCurrentFrame;
    std::vector<GoldSrcEntityDeltaOperation> operations;
    std::size_t adds = 0u;
    std::size_t updates = 0u;
    std::size_t removes = 0u;
    std::size_t unchanged = 0u;

    bool ok() const noexcept
    {
        return status == GoldSrcEntityDiffStatus::kOk;
    }
};

GoldSrcEntityDiffResult CompareGoldSrcSnapshotEntities(
    const GoldSrcServerFrame& base,
    const GoldSrcServerFrame& current) noexcept;

GoldSrcSnapshotEncodeResult EncodeGoldSrcDeltaSnapshot(
    const GoldSrcServerFrame& frame,
    const GoldSrcServerFrame& base,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity = kGoldSrcMaximumSnapshotBytes) noexcept;

GoldSrcSnapshotDecodeResult DecodeGoldSrcDeltaSnapshot(
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t frame_id,
    const GoldSrcServerFrame& base,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry) noexcept;

struct GoldSrcContinuousSnapshotBundle final
{
    GoldSrcServerFrame frame;
    GoldSrcFirstSnapshotPayload payload;
    GoldSrcSnapshotKind kind = GoldSrcSnapshotKind::kFull;
    std::optional<std::uint32_t> base_frame_id;
    GoldSrcEntityDiffResult entity_diff;
};

struct GoldSrcContinuousSnapshotBuildResult final
{
    GoldSrcSnapshotCodecStatus status =
        GoldSrcSnapshotCodecStatus::kInvalidFrame;
    GoldSrcContinuousSnapshotBundle bundle;
    std::string_view failure_table;
    std::string_view failure_field;

    bool ok() const noexcept
    {
        return status == GoldSrcSnapshotCodecStatus::kOk;
    }
};

GoldSrcContinuousSnapshotBuildResult BuildGoldSrcContinuousSnapshot(
    std::uint32_t frame_id,
    float server_time,
    const GoldSrcServerFrame* acknowledged_base,
    const GoldSrcBaselineBundle& baselines,
    const GoldSrcDeltaRegistry& registry,
    std::size_t output_capacity = kGoldSrcMaximumSnapshotBytes,
    const GoldSrcPlayerSnapshotInput* player = nullptr) noexcept;

bool IsGoldSrcServerFrameNewer(
    std::uint32_t candidate,
    std::uint32_t baseline) noexcept;
std::uint32_t GoldSrcServerFrameDistance(
    std::uint32_t newer,
    std::uint32_t older) noexcept;

enum class GoldSrcFrameStoreResult
{
    kStored,
    kInvalidHistory,
    kInvalidFrame,
    kNonMonotonicFrame,
};

std::string_view ReasonFor(GoldSrcFrameStoreResult result) noexcept;

enum class GoldSrcFrameAcknowledgeResult
{
    kAcknowledged,
    kDuplicate,
    kUnknown,
    kFuture,
    kEvicted,
    kStale,
    kAmbiguous,
    kInvalidPhase,
};

std::string_view ReasonFor(GoldSrcFrameAcknowledgeResult result) noexcept;

class GoldSrcClientFrameHistory final
{
public:
    explicit GoldSrcClientFrameHistory(
        std::size_t capacity = kGoldSrcClientFrameHistoryDepth) noexcept;

    void Reset() noexcept;
    GoldSrcFrameStoreResult Store(
        const GoldSrcServerFrame& frame,
        GoldSrcSnapshotKind kind = GoldSrcSnapshotKind::kFull,
        std::optional<std::uint32_t> base_frame_id = std::nullopt);
    GoldSrcFrameAcknowledgeResult Acknowledge(
        std::uint8_t wire_frame_reference,
        std::optional<std::uint32_t> client_server_acknowledgement =
            std::nullopt) noexcept;

    bool valid() const noexcept;
    std::size_t capacity() const noexcept;
    std::size_t size() const noexcept;
    const GoldSrcServerFrame* Find(std::uint32_t frame_id) const noexcept;
    const GoldSrcServerFrame* oldest() const noexcept;
    const GoldSrcServerFrame* newest() const noexcept;
    const GoldSrcServerFrame* last_acknowledged() const noexcept;
    const std::optional<std::uint32_t>&
        last_acknowledged_frame() const noexcept;

private:
    struct Entry final
    {
        GoldSrcServerFrame frame;
        GoldSrcSnapshotKind kind = GoldSrcSnapshotKind::kFull;
        std::optional<std::uint32_t> base_frame_id;
    };

    friend class GoldSrcFrameReferenceResolver;
    std::vector<Entry> frames_;
    std::size_t capacity_ = 0u;
    std::optional<std::uint32_t> last_acknowledged_frame_;
};

struct GoldSrcFrameReferenceResolution final
{
    GoldSrcFrameAcknowledgeResult result =
        GoldSrcFrameAcknowledgeResult::kUnknown;
    std::optional<std::uint32_t> frame_id;
};

class GoldSrcFrameReferenceResolver final
{
public:
    static GoldSrcFrameReferenceResolution Resolve(
        const GoldSrcClientFrameHistory& history,
        std::uint8_t wire_frame_reference,
        std::optional<std::uint32_t> client_server_acknowledgement =
            std::nullopt) noexcept;
};

enum class GoldSrcSnapshotDueResult
{
    kDue,
    kDisabled,
    kNotDue,
    kInvalidServerTime,
    kNonMonotonicServerTime,
};

std::string_view ReasonFor(GoldSrcSnapshotDueResult result) noexcept;

struct GoldSrcSnapshotScheduleState final
{
    bool enabled = false;
    double snapshot_rate_hz = kGoldSrcDefaultSnapshotRateHz;
    double snapshot_interval_seconds =
        1.0 / kGoldSrcDefaultSnapshotRateHz;
    double next_due_server_time = 0.0;
    double last_observed_server_time = 0.0;
    double last_due_server_time = 0.0;
    double minimum_due_interval_seconds = 0.0;
    double maximum_due_interval_seconds = 0.0;
    std::uint32_t median_due_interval_msec = 0u;
    std::uint32_t p95_due_interval_msec = 0u;
    std::array<
        std::uint64_t,
        kGoldSrcSnapshotIntervalHistogramBuckets>
        due_interval_histogram{};
    std::uint64_t due_interval_samples = 0u;
    bool has_last_due_server_time = false;
    std::optional<std::uint32_t> last_generated_frame;
    std::optional<std::uint32_t> last_sent_frame;
    std::optional<std::uint32_t> last_acknowledged_frame;
    std::uint64_t consecutive_full_snapshots = 0u;
    std::uint64_t consecutive_delta_snapshots = 0u;
    std::uint64_t skipped_snapshots = 0u;
    std::uint64_t failed_builds = 0u;
    std::uint64_t failed_sends = 0u;
    std::uint64_t due_checks = 0u;
    std::uint64_t due_snapshots = 0u;
    std::uint64_t snapshot_burst_count = 0u;
};

class GoldSrcSnapshotScheduler final
{
public:
    bool Start(double server_time, double snapshot_rate_hz) noexcept;
    void Stop() noexcept;
    void Reset() noexcept;
    GoldSrcSnapshotDueResult CheckDue(double server_time) noexcept;
    void RecordGenerated(std::uint32_t frame_id) noexcept;
    void RecordSent(
        std::uint32_t frame_id,
        GoldSrcSnapshotKind kind) noexcept;
    void RecordAcknowledged(std::uint32_t frame_id) noexcept;
    void RecordBuildFailure() noexcept;
    void RecordSendFailure() noexcept;

    const GoldSrcSnapshotScheduleState& state() const noexcept;

private:
    GoldSrcSnapshotScheduleState state_;
};

enum class GoldSrcOutgoingActionKind
{
    kReliable,
    kSnapshot,
    kEmptyAcknowledgement,
};

struct GoldSrcOutgoingClientDemand final
{
    bool active = false;
    bool reliable_pending = false;
    bool snapshot_due = false;
    bool empty_acknowledgement_pending = false;
    std::uint32_t incoming_frontier = 0u;
};

struct GoldSrcOutgoingAction final
{
    GoldSrcOutgoingActionKind kind =
        GoldSrcOutgoingActionKind::kEmptyAcknowledgement;
    std::size_t client_index = 0u;
    std::uint32_t acknowledged_frontier = 0u;
};

struct GoldSrcOutgoingScheduleResult final
{
    std::vector<GoldSrcOutgoingAction> actions;
    std::size_t next_fair_client_index = 0u;
    std::size_t snapshots_deferred = 0u;
    std::size_t empty_acknowledgements_coalesced = 0u;
};

GoldSrcOutgoingScheduleResult BuildGoldSrcBoundedOutgoingSchedule(
    const std::vector<GoldSrcOutgoingClientDemand>& clients,
    std::size_t send_budget,
    std::size_t fair_client_index) noexcept;

inline constexpr std::uint32_t kGoldSrcEffectNoInterpolation = 32u;

struct GoldSrcRemoteInterpolationSample final
{
    double server_time = 0.0;
    std::array<float, 3u> origin{};
    std::array<float, 3u> velocity{};
    float animtime = 0.0f;
    std::uint32_t effects = 0u;
    bool entity_present = false;
};

enum class GoldSrcRemoteInterpolationStatus
{
    kEligible,
    kMissingEntity,
    kInvalidTime,
    kNonMonotonicTime,
    kNonMonotonicAnimtime,
    kPermanentNoInterpolation,
    kIncoherentMotion,
};

GoldSrcRemoteInterpolationStatus ValidateGoldSrcRemoteInterpolationSamples(
    const GoldSrcRemoteInterpolationSample& previous,
    const GoldSrcRemoteInterpolationSample& current,
    float position_tolerance = 1.0f) noexcept;

enum class GoldSrcFirstSnapshotPhase
{
    kNone,
    kAwaitingFirstSnapshot,
    kFirstSnapshotPrepared,
    kFirstSnapshotSentAwaitingClientReference,
    kFirstSnapshotAcknowledged,
    kContinuousSnapshotStreaming,
    kContinuousSnapshotStable,
};

std::string_view NameFor(GoldSrcFirstSnapshotPhase phase) noexcept;

enum class GoldSrcFirstSnapshotTransitionResult
{
    kAdvanced,
    kAlreadyApplied,
    kInvalidPhase,
    kInvalidFrame,
};

std::string_view ReasonFor(
    GoldSrcFirstSnapshotTransitionResult result) noexcept;

class GoldSrcFirstSnapshotSessionState final
{
public:
    void Reset() noexcept;
    GoldSrcFirstSnapshotTransitionResult EnterAwaiting() noexcept;
    GoldSrcFirstSnapshotTransitionResult Prepare(
        GoldSrcFirstSnapshotBundle bundle);
    GoldSrcFirstSnapshotTransitionResult MarkSent();
    GoldSrcFrameAcknowledgeResult Acknowledge(
        std::uint8_t wire_frame_reference,
        std::optional<std::uint32_t> client_server_acknowledgement =
            std::nullopt) noexcept;
    bool StartContinuous(
        double server_time,
        double snapshot_rate_hz =
            kGoldSrcDefaultSnapshotRateHz) noexcept;
    GoldSrcSnapshotDueResult CheckContinuousDue(
        double server_time) noexcept;
    GoldSrcFrameStoreResult MarkContinuousSent(
        const GoldSrcContinuousSnapshotBundle& bundle);

    GoldSrcFirstSnapshotPhase phase() const noexcept;
    const std::optional<GoldSrcFirstSnapshotBundle>& prepared() const noexcept;
    const GoldSrcClientFrameHistory& history() const noexcept;
    const GoldSrcSnapshotScheduler& scheduler() const noexcept;

private:
    GoldSrcFirstSnapshotPhase phase_ = GoldSrcFirstSnapshotPhase::kNone;
    std::optional<GoldSrcFirstSnapshotBundle> prepared_;
    GoldSrcClientFrameHistory history_;
    GoldSrcSnapshotScheduler scheduler_;
    std::size_t continuous_acknowledgements_ = 0u;
};
} // namespace hl::network
