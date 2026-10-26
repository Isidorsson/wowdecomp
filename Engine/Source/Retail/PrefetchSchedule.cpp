#include "Engine/Source/Retail/PrefetchSchedule.h"

namespace Retail {

uint64_t PrefetchSchedule::Request(uint32_t key, uint64_t nowMs) {
    if (this->m_waiting && this->m_key == key) {
        return this->m_generation;
    }
    this->m_waiting = true;
    this->m_key = key;
    this->m_requestedAt = nowMs;
    return ++this->m_generation;
}

PrefetchSchedule::Due PrefetchSchedule::Next(uint64_t nowMs) {
    Due due;
    if (!this->m_waiting) {
        return due;
    }
    uint64_t rested = nowMs > this->m_requestedAt ? nowMs - this->m_requestedAt : 0;
    if (rested < this->m_restMs) {
        due.kind = Due::Kind::Wait;
        due.waitMs = this->m_restMs - rested;
        return due;
    }
    this->m_waiting = false;
    due.kind = Due::Kind::Run;
    due.key = this->m_key;
    due.generation = this->m_generation;
    return due;
}

bool PrefetchSchedule::IsCurrent(uint64_t generation) const {
    return generation == this->m_generation;
}

void PrefetchSchedule::Cancel() {
    this->m_waiting = false;
    this->m_generation++;
}

}
