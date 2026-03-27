#include "spawn_trace.h"

#include <algorithm>

namespace hl::game_api::detail
{
SpawnTrace::SpawnTrace()
    : SpawnTrace(64)
{
}

SpawnTrace::SpawnTrace(std::size_t max_entries)
    : max_entries_(std::max<std::size_t>(max_entries, 1))
{
}

void SpawnTrace::Clear()
{
    next_sequence_ = 1;
    events_.clear();
}

void SpawnTrace::Record(
    std::string_view callback_name,
    std::string_view detail,
    int edict_index,
    std::string_view classname,
    std::string_view map_name)
{
    if (events_.size() >= max_entries_)
    {
        events_.pop_front();
    }

    events_.push_back({
        next_sequence_++,
        std::string(callback_name),
        std::string(detail),
        edict_index,
        std::string(classname),
        std::string(map_name),
    });
}

std::vector<SpawnTraceEvent> SpawnTrace::Snapshot() const
{
    return std::vector<SpawnTraceEvent>(events_.begin(), events_.end());
}

const SpawnTraceEvent* SpawnTrace::LastEvent() const noexcept
{
    return events_.empty() ? nullptr : &events_.back();
}

std::size_t SpawnTrace::Size() const noexcept
{
    return events_.size();
}

ActivationTrace::ActivationTrace()
    : ActivationTrace(256)
{
}

ActivationTrace::ActivationTrace(std::size_t max_entries)
    : trace_(max_entries)
{
}

void ActivationTrace::Clear()
{
    event_count_ = 0;
    trace_.Clear();
}

void ActivationTrace::Record(
    std::string_view callback_name,
    std::string_view detail,
    int edict_index,
    std::string_view classname,
    std::string_view map_name)
{
    ++event_count_;
    trace_.Record(callback_name, detail, edict_index, classname, map_name);
}

std::vector<SpawnTraceEvent> ActivationTrace::Snapshot() const
{
    return trace_.Snapshot();
}

const SpawnTraceEvent* ActivationTrace::LastEvent() const noexcept
{
    return trace_.LastEvent();
}

std::size_t ActivationTrace::Size() const noexcept
{
    return trace_.Size();
}

std::size_t ActivationTrace::EventCount() const noexcept
{
    return event_count_;
}

WorldspawnSpawnDiagnostics::WorldspawnSpawnDiagnostics()
    : WorldspawnSpawnDiagnostics(64)
{
}

WorldspawnSpawnDiagnostics::WorldspawnSpawnDiagnostics(std::size_t trace_capacity)
    : trace_(trace_capacity)
    , distinct_trace_(16)
    , precache_trace_(32)
{
}

void WorldspawnSpawnDiagnostics::Reset()
{
    active_ = false;
    attempted_ = false;
    succeeded_ = false;
    saw_seh_exception_ = false;
    exception_code_ = 0;
    edict_index_ = -1;
    classname_.clear();
    map_name_.clear();
    trace_.Clear();
    distinct_trace_.Clear();
    precache_trace_.Clear();
    sound_precache_count_ = 0;
    duplicate_sound_precache_count_ = 0;
    missing_sound_file_count_ = 0;
    random_long_baseline_ = 0;
    random_float_baseline_ = 0;
}

void WorldspawnSpawnDiagnostics::BeginAttempt(
    int edict_index,
    std::string_view classname,
    std::string_view map_name,
    std::uint64_t random_long_baseline,
    std::uint64_t random_float_baseline)
{
    Reset();
    active_ = true;
    attempted_ = true;
    edict_index_ = edict_index;
    classname_ = std::string(classname);
    map_name_ = std::string(map_name);
    random_long_baseline_ = random_long_baseline;
    random_float_baseline_ = random_float_baseline;
}

void WorldspawnSpawnDiagnostics::RecordCallback(
    std::string_view callback_name,
    std::string_view detail)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(callback_name, detail, edict_index_, classname_, map_name_);
    if (const SpawnTraceEvent* last_distinct = distinct_trace_.LastEvent();
        last_distinct == nullptr || last_distinct->callback_name != callback_name)
    {
        distinct_trace_.Record(callback_name, detail, edict_index_, classname_, map_name_);
    }
}

void WorldspawnSpawnDiagnostics::RecordPrecacheOperation(
    std::string_view callback_name,
    std::string_view detail)
{
    if (!active_)
    {
        return;
    }

    precache_trace_.Record(callback_name, detail, edict_index_, classname_, map_name_);
}

void WorldspawnSpawnDiagnostics::RecordSoundPrecache(bool duplicate, bool file_exists)
{
    if (!active_)
    {
        return;
    }

    ++sound_precache_count_;
    if (duplicate)
    {
        ++duplicate_sound_precache_count_;
    }

    if (!file_exists)
    {
        ++missing_sound_file_count_;
    }
}

void WorldspawnSpawnDiagnostics::MarkSucceeded()
{
    active_ = false;
    succeeded_ = true;
}

void WorldspawnSpawnDiagnostics::MarkSehException(unsigned int exception_code)
{
    active_ = false;
    saw_seh_exception_ = true;
    exception_code_ = exception_code;
}

void WorldspawnSpawnDiagnostics::SetSucceeded(bool succeeded) noexcept
{
    succeeded_ = succeeded;
}

bool WorldspawnSpawnDiagnostics::IsActive() const noexcept
{
    return active_;
}

bool WorldspawnSpawnDiagnostics::Attempted() const noexcept
{
    return attempted_;
}

bool WorldspawnSpawnDiagnostics::Succeeded() const noexcept
{
    return succeeded_;
}

bool WorldspawnSpawnDiagnostics::SawSehException() const noexcept
{
    return saw_seh_exception_;
}

unsigned int WorldspawnSpawnDiagnostics::ExceptionCode() const noexcept
{
    return exception_code_;
}

int WorldspawnSpawnDiagnostics::EdictIndex() const noexcept
{
    return edict_index_;
}

const std::string& WorldspawnSpawnDiagnostics::Classname() const noexcept
{
    return classname_;
}

const std::string& WorldspawnSpawnDiagnostics::MapName() const noexcept
{
    return map_name_;
}

const SpawnTraceEvent* WorldspawnSpawnDiagnostics::LastEvent() const noexcept
{
    return trace_.LastEvent();
}

const SpawnTraceEvent* WorldspawnSpawnDiagnostics::LastDistinctEvent() const noexcept
{
    return distinct_trace_.LastEvent();
}

std::vector<SpawnTraceEvent> WorldspawnSpawnDiagnostics::TraceSnapshot() const
{
    return trace_.Snapshot();
}

std::vector<SpawnTraceEvent> WorldspawnSpawnDiagnostics::DistinctCallbackSnapshot() const
{
    return distinct_trace_.Snapshot();
}

std::vector<SpawnTraceEvent> WorldspawnSpawnDiagnostics::PrecacheSnapshot() const
{
    return precache_trace_.Snapshot();
}

std::size_t WorldspawnSpawnDiagnostics::SoundPrecacheCount() const noexcept
{
    return sound_precache_count_;
}

std::size_t WorldspawnSpawnDiagnostics::DuplicateSoundPrecacheCount() const noexcept
{
    return duplicate_sound_precache_count_;
}

std::size_t WorldspawnSpawnDiagnostics::MissingSoundFileCount() const noexcept
{
    return missing_sound_file_count_;
}

std::uint64_t WorldspawnSpawnDiagnostics::RandomLongBaseline() const noexcept
{
    return random_long_baseline_;
}

std::uint64_t WorldspawnSpawnDiagnostics::RandomFloatBaseline() const noexcept
{
    return random_float_baseline_;
}

ServerActivationDiagnostics::ServerActivationDiagnostics()
    : ServerActivationDiagnostics(256)
{
}

ServerActivationDiagnostics::ServerActivationDiagnostics(std::size_t trace_capacity)
    : trace_(trace_capacity)
    , distinct_trace_(64)
{
}

void ServerActivationDiagnostics::Reset()
{
    active_ = false;
    attempted_ = false;
    succeeded_ = false;
    saw_seh_exception_ = false;
    exception_code_ = 0;
    edict_count_ = 0;
    client_max_ = 0;
    map_name_.clear();
    current_context_known_ = false;
    current_edict_index_ = -1;
    current_classname_.clear();
    trace_.Clear();
    distinct_trace_.Clear();
}

void ServerActivationDiagnostics::BeginAttempt(
    std::string_view map_name,
    int edict_count,
    int client_max)
{
    Reset();
    active_ = true;
    attempted_ = true;
    map_name_ = std::string(map_name);
    edict_count_ = edict_count;
    client_max_ = client_max;
}

void ServerActivationDiagnostics::RecordCallback(
    std::string_view callback_name,
    std::string_view detail)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(
        callback_name,
        detail,
        current_context_known_ ? current_edict_index_ : -1,
        current_context_known_ ? std::string_view(current_classname_) : std::string_view(),
        map_name_);
    if (const SpawnTraceEvent* last_distinct = distinct_trace_.LastEvent();
        last_distinct == nullptr || last_distinct->callback_name != callback_name)
    {
        distinct_trace_.Record(
            callback_name,
            detail,
            current_context_known_ ? current_edict_index_ : -1,
            current_context_known_ ? std::string_view(current_classname_) : std::string_view(),
            map_name_);
    }
}

void ServerActivationDiagnostics::RecordCallback(
    std::string_view callback_name,
    std::string_view detail,
    int edict_index,
    std::string_view classname)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(callback_name, detail, edict_index, classname, map_name_);
    if (const SpawnTraceEvent* last_distinct = distinct_trace_.LastEvent();
        last_distinct == nullptr || last_distinct->callback_name != callback_name)
    {
        distinct_trace_.Record(callback_name, detail, edict_index, classname, map_name_);
    }
}

void ServerActivationDiagnostics::RecordInternalEvent(
    std::string_view event_name,
    std::string_view detail,
    int edict_index,
    std::string_view classname)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(event_name, detail, edict_index, classname, map_name_);
}

void ServerActivationDiagnostics::MarkSucceeded()
{
    active_ = false;
    succeeded_ = true;
    current_context_known_ = false;
    current_edict_index_ = -1;
    current_classname_.clear();
}

void ServerActivationDiagnostics::MarkSehException(unsigned int exception_code)
{
    active_ = false;
    saw_seh_exception_ = true;
    exception_code_ = exception_code;
    current_context_known_ = false;
    current_edict_index_ = -1;
    current_classname_.clear();
}

void ServerActivationDiagnostics::SetCurrentEntityContext(
    int edict_index,
    std::string_view classname)
{
    current_context_known_ = edict_index >= 0 || !classname.empty();
    current_edict_index_ = edict_index;
    current_classname_ = std::string(classname);
}

void ServerActivationDiagnostics::ClearCurrentEntityContext()
{
    current_context_known_ = false;
    current_edict_index_ = -1;
    current_classname_.clear();
}

bool ServerActivationDiagnostics::IsActive() const noexcept
{
    return active_;
}

bool ServerActivationDiagnostics::Attempted() const noexcept
{
    return attempted_;
}

bool ServerActivationDiagnostics::Succeeded() const noexcept
{
    return succeeded_;
}

bool ServerActivationDiagnostics::SawSehException() const noexcept
{
    return saw_seh_exception_;
}

unsigned int ServerActivationDiagnostics::ExceptionCode() const noexcept
{
    return exception_code_;
}

int ServerActivationDiagnostics::EdictCount() const noexcept
{
    return edict_count_;
}

int ServerActivationDiagnostics::ClientMax() const noexcept
{
    return client_max_;
}

const std::string& ServerActivationDiagnostics::MapName() const noexcept
{
    return map_name_;
}

bool ServerActivationDiagnostics::HasCurrentEntityContext() const noexcept
{
    return current_context_known_;
}

int ServerActivationDiagnostics::CurrentEdictIndex() const noexcept
{
    return current_edict_index_;
}

const std::string& ServerActivationDiagnostics::CurrentClassname() const noexcept
{
    return current_classname_;
}

const SpawnTraceEvent* ServerActivationDiagnostics::LastEvent() const noexcept
{
    return trace_.LastEvent();
}

const SpawnTraceEvent* ServerActivationDiagnostics::LastDistinctEvent() const noexcept
{
    return distinct_trace_.LastEvent();
}

std::vector<SpawnTraceEvent> ServerActivationDiagnostics::TraceSnapshot() const
{
    return trace_.Snapshot();
}

std::vector<SpawnTraceEvent> ServerActivationDiagnostics::DistinctCallbackSnapshot() const
{
    return distinct_trace_.Snapshot();
}

std::size_t ServerActivationDiagnostics::EventCount() const noexcept
{
    return trace_.EventCount();
}

ServerFrameDiagnostics::ServerFrameDiagnostics()
    : ServerFrameDiagnostics(256)
{
}

ServerFrameDiagnostics::ServerFrameDiagnostics(std::size_t trace_capacity)
    : trace_(trace_capacity)
    , distinct_trace_(64)
{
}

void ServerFrameDiagnostics::Reset()
{
    active_ = false;
    attempted_ = false;
    succeeded_ = false;
    saw_seh_exception_ = false;
    exception_code_ = 0;
    frame_number_ = 0;
    host_frame_index_ = 0;
    server_frame_index_ = 0;
    time_ = 0.0f;
    frametime_ = 0.0f;
    map_name_.clear();
    trace_.Clear();
    distinct_trace_.Clear();
}

void ServerFrameDiagnostics::BeginFrame(
    std::string_view map_name,
    int frame_number,
    std::uint64_t host_frame_index,
    std::uint64_t server_frame_index,
    float time,
    float frametime)
{
    Reset();
    active_ = true;
    attempted_ = true;
    map_name_ = std::string(map_name);
    frame_number_ = frame_number;
    host_frame_index_ = host_frame_index;
    server_frame_index_ = server_frame_index;
    time_ = time;
    frametime_ = frametime;
}

void ServerFrameDiagnostics::RecordCallback(
    std::string_view callback_name,
    std::string_view detail)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(callback_name, detail, -1, {}, map_name_);
    if (const SpawnTraceEvent* last_distinct = distinct_trace_.LastEvent();
        last_distinct == nullptr || last_distinct->callback_name != callback_name)
    {
        distinct_trace_.Record(callback_name, detail, -1, {}, map_name_);
    }
}

void ServerFrameDiagnostics::RecordCallback(
    std::string_view callback_name,
    std::string_view detail,
    int edict_index,
    std::string_view classname)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(callback_name, detail, edict_index, classname, map_name_);
    if (const SpawnTraceEvent* last_distinct = distinct_trace_.LastEvent();
        last_distinct == nullptr || last_distinct->callback_name != callback_name)
    {
        distinct_trace_.Record(callback_name, detail, edict_index, classname, map_name_);
    }
}

void ServerFrameDiagnostics::RecordInternalEvent(
    std::string_view event_name,
    std::string_view detail,
    int edict_index,
    std::string_view classname)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(event_name, detail, edict_index, classname, map_name_);
}

void ServerFrameDiagnostics::MarkSucceeded()
{
    active_ = false;
    succeeded_ = true;
}

void ServerFrameDiagnostics::MarkSehException(unsigned int exception_code)
{
    active_ = false;
    saw_seh_exception_ = true;
    exception_code_ = exception_code;
}

bool ServerFrameDiagnostics::IsActive() const noexcept
{
    return active_;
}

bool ServerFrameDiagnostics::Attempted() const noexcept
{
    return attempted_;
}

bool ServerFrameDiagnostics::Succeeded() const noexcept
{
    return succeeded_;
}

bool ServerFrameDiagnostics::SawSehException() const noexcept
{
    return saw_seh_exception_;
}

unsigned int ServerFrameDiagnostics::ExceptionCode() const noexcept
{
    return exception_code_;
}

int ServerFrameDiagnostics::FrameNumber() const noexcept
{
    return frame_number_;
}

std::uint64_t ServerFrameDiagnostics::HostFrameIndex() const noexcept
{
    return host_frame_index_;
}

std::uint64_t ServerFrameDiagnostics::ServerFrameIndex() const noexcept
{
    return server_frame_index_;
}

float ServerFrameDiagnostics::Time() const noexcept
{
    return time_;
}

float ServerFrameDiagnostics::Frametime() const noexcept
{
    return frametime_;
}

const std::string& ServerFrameDiagnostics::MapName() const noexcept
{
    return map_name_;
}

const SpawnTraceEvent* ServerFrameDiagnostics::LastEvent() const noexcept
{
    return trace_.LastEvent();
}

const SpawnTraceEvent* ServerFrameDiagnostics::LastDistinctEvent() const noexcept
{
    return distinct_trace_.LastEvent();
}

std::vector<SpawnTraceEvent> ServerFrameDiagnostics::TraceSnapshot() const
{
    return trace_.Snapshot();
}

std::vector<SpawnTraceEvent> ServerFrameDiagnostics::DistinctCallbackSnapshot() const
{
    return distinct_trace_.Snapshot();
}

std::size_t ServerFrameDiagnostics::EventCount() const noexcept
{
    return trace_.EventCount();
}

EntityThinkDiagnostics::EntityThinkDiagnostics()
    : EntityThinkDiagnostics(256)
{
}

EntityThinkDiagnostics::EntityThinkDiagnostics(std::size_t trace_capacity)
    : trace_(trace_capacity)
    , distinct_trace_(64)
{
}

void EntityThinkDiagnostics::Reset()
{
    active_ = false;
    attempted_ = false;
    completed_ = false;
    saw_entity_seh_ = false;
    exception_code_ = 0;
    frame_number_ = 0;
    host_frame_index_ = 0;
    server_frame_index_ = 0;
    time_ = 0.0f;
    frametime_ = 0.0f;
    map_name_.clear();
    current_context_known_ = false;
    current_edict_index_ = -1;
    current_classname_.clear();
    trace_.Clear();
    distinct_trace_.Clear();
}

void EntityThinkDiagnostics::BeginFrame(
    std::string_view map_name,
    int frame_number,
    std::uint64_t host_frame_index,
    std::uint64_t server_frame_index,
    float time,
    float frametime)
{
    Reset();
    active_ = true;
    attempted_ = true;
    map_name_ = std::string(map_name);
    frame_number_ = frame_number;
    host_frame_index_ = host_frame_index;
    server_frame_index_ = server_frame_index;
    time_ = time;
    frametime_ = frametime;
}

void EntityThinkDiagnostics::RecordCallback(
    std::string_view callback_name,
    std::string_view detail)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(
        callback_name,
        detail,
        current_context_known_ ? current_edict_index_ : -1,
        current_context_known_ ? std::string_view(current_classname_) : std::string_view(),
        map_name_);
    if (const SpawnTraceEvent* last_distinct = distinct_trace_.LastEvent();
        last_distinct == nullptr || last_distinct->callback_name != callback_name)
    {
        distinct_trace_.Record(
            callback_name,
            detail,
            current_context_known_ ? current_edict_index_ : -1,
            current_context_known_ ? std::string_view(current_classname_) : std::string_view(),
            map_name_);
    }
}

void EntityThinkDiagnostics::RecordCallback(
    std::string_view callback_name,
    std::string_view detail,
    int edict_index,
    std::string_view classname)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(callback_name, detail, edict_index, classname, map_name_);
    if (const SpawnTraceEvent* last_distinct = distinct_trace_.LastEvent();
        last_distinct == nullptr || last_distinct->callback_name != callback_name)
    {
        distinct_trace_.Record(callback_name, detail, edict_index, classname, map_name_);
    }
}

void EntityThinkDiagnostics::RecordInternalEvent(
    std::string_view event_name,
    std::string_view detail,
    int edict_index,
    std::string_view classname)
{
    if (!active_)
    {
        return;
    }

    trace_.Record(event_name, detail, edict_index, classname, map_name_);
}

void EntityThinkDiagnostics::SetCurrentEntityContext(
    int edict_index,
    std::string_view classname)
{
    current_context_known_ = edict_index >= 0 || !classname.empty();
    current_edict_index_ = edict_index;
    current_classname_ = std::string(classname);
}

void EntityThinkDiagnostics::ClearCurrentEntityContext()
{
    current_context_known_ = false;
    current_edict_index_ = -1;
    current_classname_.clear();
}

void EntityThinkDiagnostics::MarkCompleted()
{
    active_ = false;
    completed_ = true;
    current_context_known_ = false;
    current_edict_index_ = -1;
    current_classname_.clear();
}

void EntityThinkDiagnostics::MarkEntitySeh(unsigned int exception_code)
{
    saw_entity_seh_ = true;
    exception_code_ = exception_code;
}

bool EntityThinkDiagnostics::IsActive() const noexcept
{
    return active_;
}

bool EntityThinkDiagnostics::Attempted() const noexcept
{
    return attempted_;
}

bool EntityThinkDiagnostics::Completed() const noexcept
{
    return completed_;
}

bool EntityThinkDiagnostics::SawEntitySeh() const noexcept
{
    return saw_entity_seh_;
}

unsigned int EntityThinkDiagnostics::ExceptionCode() const noexcept
{
    return exception_code_;
}

int EntityThinkDiagnostics::FrameNumber() const noexcept
{
    return frame_number_;
}

std::uint64_t EntityThinkDiagnostics::HostFrameIndex() const noexcept
{
    return host_frame_index_;
}

std::uint64_t EntityThinkDiagnostics::ServerFrameIndex() const noexcept
{
    return server_frame_index_;
}

float EntityThinkDiagnostics::Time() const noexcept
{
    return time_;
}

float EntityThinkDiagnostics::Frametime() const noexcept
{
    return frametime_;
}

const std::string& EntityThinkDiagnostics::MapName() const noexcept
{
    return map_name_;
}

const SpawnTraceEvent* EntityThinkDiagnostics::LastEvent() const noexcept
{
    return trace_.LastEvent();
}

const SpawnTraceEvent* EntityThinkDiagnostics::LastDistinctEvent() const noexcept
{
    return distinct_trace_.LastEvent();
}

std::vector<SpawnTraceEvent> EntityThinkDiagnostics::TraceSnapshot() const
{
    return trace_.Snapshot();
}

std::vector<SpawnTraceEvent> EntityThinkDiagnostics::DistinctCallbackSnapshot() const
{
    return distinct_trace_.Snapshot();
}

std::size_t EntityThinkDiagnostics::EventCount() const noexcept
{
    return trace_.EventCount();
}
} // namespace hl::game_api::detail
