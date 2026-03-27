#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace hl::game_api::detail
{
struct SpawnTraceEvent
{
    std::size_t sequence = 0;
    std::string callback_name;
    std::string detail;
    int edict_index = -1;
    std::string classname;
    std::string map_name;
};

class SpawnTrace
{
public:
    SpawnTrace();
    explicit SpawnTrace(std::size_t max_entries);

    void Clear();
    void Record(
        std::string_view callback_name,
        std::string_view detail,
        int edict_index,
        std::string_view classname,
        std::string_view map_name);

    std::vector<SpawnTraceEvent> Snapshot() const;
    const SpawnTraceEvent* LastEvent() const noexcept;
    std::size_t Size() const noexcept;

private:
    std::size_t max_entries_ = 64;
    std::size_t next_sequence_ = 1;
    std::deque<SpawnTraceEvent> events_;
};

class ActivationTrace
{
public:
    ActivationTrace();
    explicit ActivationTrace(std::size_t max_entries);

    void Clear();
    void Record(
        std::string_view callback_name,
        std::string_view detail,
        int edict_index,
        std::string_view classname,
        std::string_view map_name);

    std::vector<SpawnTraceEvent> Snapshot() const;
    const SpawnTraceEvent* LastEvent() const noexcept;
    std::size_t Size() const noexcept;
    std::size_t EventCount() const noexcept;

private:
    SpawnTrace trace_;
    std::size_t event_count_ = 0;
};

class WorldspawnSpawnDiagnostics
{
public:
    WorldspawnSpawnDiagnostics();
    explicit WorldspawnSpawnDiagnostics(std::size_t trace_capacity);

    void Reset();
    void BeginAttempt(
        int edict_index,
        std::string_view classname,
        std::string_view map_name,
        std::uint64_t random_long_baseline,
        std::uint64_t random_float_baseline);
    void RecordCallback(std::string_view callback_name, std::string_view detail);
    void RecordPrecacheOperation(std::string_view callback_name, std::string_view detail);
    void RecordSoundPrecache(bool duplicate, bool file_exists);
    void MarkSucceeded();
    void MarkSehException(unsigned int exception_code);
    void SetSucceeded(bool succeeded) noexcept;

    bool IsActive() const noexcept;
    bool Attempted() const noexcept;
    bool Succeeded() const noexcept;
    bool SawSehException() const noexcept;
    unsigned int ExceptionCode() const noexcept;
    int EdictIndex() const noexcept;
    const std::string& Classname() const noexcept;
    const std::string& MapName() const noexcept;
    const SpawnTraceEvent* LastEvent() const noexcept;
    const SpawnTraceEvent* LastDistinctEvent() const noexcept;
    std::vector<SpawnTraceEvent> TraceSnapshot() const;
    std::vector<SpawnTraceEvent> DistinctCallbackSnapshot() const;
    std::vector<SpawnTraceEvent> PrecacheSnapshot() const;
    std::size_t SoundPrecacheCount() const noexcept;
    std::size_t DuplicateSoundPrecacheCount() const noexcept;
    std::size_t MissingSoundFileCount() const noexcept;
    std::uint64_t RandomLongBaseline() const noexcept;
    std::uint64_t RandomFloatBaseline() const noexcept;

private:
    bool active_ = false;
    bool attempted_ = false;
    bool succeeded_ = false;
    bool saw_seh_exception_ = false;
    unsigned int exception_code_ = 0;
    int edict_index_ = -1;
    std::string classname_;
    std::string map_name_;
    SpawnTrace trace_;
    SpawnTrace distinct_trace_;
    SpawnTrace precache_trace_;
    std::size_t sound_precache_count_ = 0;
    std::size_t duplicate_sound_precache_count_ = 0;
    std::size_t missing_sound_file_count_ = 0;
    std::uint64_t random_long_baseline_ = 0;
    std::uint64_t random_float_baseline_ = 0;
};

class ServerActivationDiagnostics
{
public:
    ServerActivationDiagnostics();
    explicit ServerActivationDiagnostics(std::size_t trace_capacity);

    void Reset();
    void BeginAttempt(std::string_view map_name, int edict_count, int client_max);
    void RecordCallback(std::string_view callback_name, std::string_view detail);
    void RecordCallback(
        std::string_view callback_name,
        std::string_view detail,
        int edict_index,
        std::string_view classname);
    void RecordInternalEvent(
        std::string_view event_name,
        std::string_view detail,
        int edict_index,
        std::string_view classname);
    void MarkSucceeded();
    void MarkSehException(unsigned int exception_code);
    void SetCurrentEntityContext(int edict_index, std::string_view classname);
    void ClearCurrentEntityContext();

    bool IsActive() const noexcept;
    bool Attempted() const noexcept;
    bool Succeeded() const noexcept;
    bool SawSehException() const noexcept;
    unsigned int ExceptionCode() const noexcept;
    int EdictCount() const noexcept;
    int ClientMax() const noexcept;
    const std::string& MapName() const noexcept;
    bool HasCurrentEntityContext() const noexcept;
    int CurrentEdictIndex() const noexcept;
    const std::string& CurrentClassname() const noexcept;
    const SpawnTraceEvent* LastEvent() const noexcept;
    const SpawnTraceEvent* LastDistinctEvent() const noexcept;
    std::vector<SpawnTraceEvent> TraceSnapshot() const;
    std::vector<SpawnTraceEvent> DistinctCallbackSnapshot() const;
    std::size_t EventCount() const noexcept;

private:
    bool active_ = false;
    bool attempted_ = false;
    bool succeeded_ = false;
    bool saw_seh_exception_ = false;
    unsigned int exception_code_ = 0;
    int edict_count_ = 0;
    int client_max_ = 0;
    std::string map_name_;
    bool current_context_known_ = false;
    int current_edict_index_ = -1;
    std::string current_classname_;
    ActivationTrace trace_;
    SpawnTrace distinct_trace_;
};

class ServerFrameDiagnostics
{
public:
    ServerFrameDiagnostics();
    explicit ServerFrameDiagnostics(std::size_t trace_capacity);

    void Reset();
    void BeginFrame(
        std::string_view map_name,
        int frame_number,
        std::uint64_t host_frame_index,
        std::uint64_t server_frame_index,
        float time,
        float frametime);
    void RecordCallback(std::string_view callback_name, std::string_view detail);
    void RecordCallback(
        std::string_view callback_name,
        std::string_view detail,
        int edict_index,
        std::string_view classname);
    void RecordInternalEvent(
        std::string_view event_name,
        std::string_view detail,
        int edict_index,
        std::string_view classname);
    void MarkSucceeded();
    void MarkSehException(unsigned int exception_code);

    bool IsActive() const noexcept;
    bool Attempted() const noexcept;
    bool Succeeded() const noexcept;
    bool SawSehException() const noexcept;
    unsigned int ExceptionCode() const noexcept;
    int FrameNumber() const noexcept;
    std::uint64_t HostFrameIndex() const noexcept;
    std::uint64_t ServerFrameIndex() const noexcept;
    float Time() const noexcept;
    float Frametime() const noexcept;
    const std::string& MapName() const noexcept;
    const SpawnTraceEvent* LastEvent() const noexcept;
    const SpawnTraceEvent* LastDistinctEvent() const noexcept;
    std::vector<SpawnTraceEvent> TraceSnapshot() const;
    std::vector<SpawnTraceEvent> DistinctCallbackSnapshot() const;
    std::size_t EventCount() const noexcept;

private:
    bool active_ = false;
    bool attempted_ = false;
    bool succeeded_ = false;
    bool saw_seh_exception_ = false;
    unsigned int exception_code_ = 0;
    int frame_number_ = 0;
    std::uint64_t host_frame_index_ = 0;
    std::uint64_t server_frame_index_ = 0;
    float time_ = 0.0f;
    float frametime_ = 0.0f;
    std::string map_name_;
    ActivationTrace trace_;
    SpawnTrace distinct_trace_;
};

class EntityThinkDiagnostics
{
public:
    EntityThinkDiagnostics();
    explicit EntityThinkDiagnostics(std::size_t trace_capacity);

    void Reset();
    void BeginFrame(
        std::string_view map_name,
        int frame_number,
        std::uint64_t host_frame_index,
        std::uint64_t server_frame_index,
        float time,
        float frametime);
    void RecordCallback(std::string_view callback_name, std::string_view detail);
    void RecordCallback(
        std::string_view callback_name,
        std::string_view detail,
        int edict_index,
        std::string_view classname);
    void RecordInternalEvent(
        std::string_view event_name,
        std::string_view detail,
        int edict_index,
        std::string_view classname);
    void SetCurrentEntityContext(int edict_index, std::string_view classname);
    void ClearCurrentEntityContext();
    void MarkCompleted();
    void MarkEntitySeh(unsigned int exception_code);

    bool IsActive() const noexcept;
    bool Attempted() const noexcept;
    bool Completed() const noexcept;
    bool SawEntitySeh() const noexcept;
    unsigned int ExceptionCode() const noexcept;
    int FrameNumber() const noexcept;
    std::uint64_t HostFrameIndex() const noexcept;
    std::uint64_t ServerFrameIndex() const noexcept;
    float Time() const noexcept;
    float Frametime() const noexcept;
    const std::string& MapName() const noexcept;
    const SpawnTraceEvent* LastEvent() const noexcept;
    const SpawnTraceEvent* LastDistinctEvent() const noexcept;
    std::vector<SpawnTraceEvent> TraceSnapshot() const;
    std::vector<SpawnTraceEvent> DistinctCallbackSnapshot() const;
    std::size_t EventCount() const noexcept;

private:
    bool active_ = false;
    bool attempted_ = false;
    bool completed_ = false;
    bool saw_entity_seh_ = false;
    unsigned int exception_code_ = 0;
    int frame_number_ = 0;
    std::uint64_t host_frame_index_ = 0;
    std::uint64_t server_frame_index_ = 0;
    float time_ = 0.0f;
    float frametime_ = 0.0f;
    std::string map_name_;
    bool current_context_known_ = false;
    int current_edict_index_ = -1;
    std::string current_classname_;
    ActivationTrace trace_;
    SpawnTrace distinct_trace_;
};
} // namespace hl::game_api::detail
