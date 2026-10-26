#ifndef ENGINE_SOURCE_RETAIL_PREFETCHSCHEDULE_H
#define ENGINE_SOURCE_RETAIL_PREFETCHSCHEDULE_H

#include <cstdint>

namespace Retail {

// When the retail browser's prefetch worker prepares a display: the latest selection only, once it has rested
// selected for a while (arrowing through rows prepares none of the rows passed). Each request is a generation; a job
// that is no longer the current one stops at its next step. Not thread-safe: the caller locks.

constexpr uint64_t PREFETCH_REST_MS = 200;

class PrefetchSchedule {
    public:
    explicit PrefetchSchedule(uint64_t restMs = PREFETCH_REST_MS) : m_restMs(restMs) {}

    // Replaces any waiting request; the generation it runs as. Requesting the key already waiting keeps its time.
    uint64_t Request(uint32_t key, uint64_t nowMs);

    struct Due {
        enum class Kind {
            Idle,   // Nothing waits.
            Wait,   // The request rests for waitMs more.
            Run,    // Run key as generation: taken, so the next call is Idle until another request.
        };
        Kind kind = Kind::Idle;
        uint64_t waitMs = 0;
        uint32_t key = 0;
        uint64_t generation = 0;
    };
    Due Next(uint64_t nowMs);

    // Whether a job of generation is still the latest request.
    bool IsCurrent(uint64_t generation) const;

    // Drops the waiting request and makes every job stale.
    void Cancel();

    private:
    uint64_t m_restMs;
    uint64_t m_generation = 0;
    bool m_waiting = false;
    uint32_t m_key = 0;
    uint64_t m_requestedAt = 0;
};

}

#endif
