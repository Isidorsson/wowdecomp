#include "Engine/Source/Retail/PrefetchSchedule.h"
#include "Tools/Tests/Check.h"

using Retail::PrefetchSchedule;
using Kind = Retail::PrefetchSchedule::Due::Kind;

TEST(PrefetchesWaitUntilTheSelectionHasRested) {
    PrefetchSchedule schedule(200);
    CHECK(schedule.Next(0).kind == Kind::Idle);

    uint64_t generation = schedule.Request(145747, 1000);
    PrefetchSchedule::Due due = schedule.Next(1050);
    CHECK(due.kind == Kind::Wait && due.waitMs == 150);

    due = schedule.Next(1200);
    CHECK(due.kind == Kind::Run && due.key == 145747 && due.generation == generation);
    CHECK(schedule.IsCurrent(generation));
    CHECK(schedule.Next(5000).kind == Kind::Idle);   // Taken: it runs once.
}

TEST(RapidSelectionsPrefetchOnlyTheLastRow) {
    PrefetchSchedule schedule(200);
    uint64_t first = schedule.Request(1, 0);
    uint64_t second = schedule.Request(2, 100);
    uint64_t third = schedule.Request(3, 250);
    CHECK(first != second && second != third);

    PrefetchSchedule::Due due = schedule.Next(300);
    CHECK(due.kind == Kind::Wait && due.waitMs == 150);   // Rests from the last selection.
    due = schedule.Next(450);
    CHECK(due.kind == Kind::Run && due.key == 3 && due.generation == third);
    CHECK(!schedule.IsCurrent(first) && !schedule.IsCurrent(second));
}

TEST(ANewSelectionMakesTheRunningPrefetchStale) {
    PrefetchSchedule schedule(200);
    schedule.Request(1, 0);
    PrefetchSchedule::Due running = schedule.Next(200);
    CHECK(running.kind == Kind::Run && schedule.IsCurrent(running.generation));

    uint64_t next = schedule.Request(2, 300);
    CHECK(!schedule.IsCurrent(running.generation));
    CHECK(schedule.IsCurrent(next));
    CHECK(schedule.Next(500).key == 2);
}

TEST(ReselectingTheWaitingRowKeepsItsRest) {
    PrefetchSchedule schedule(200);
    uint64_t generation = schedule.Request(7, 0);
    CHECK(schedule.Request(7, 150) == generation);
    PrefetchSchedule::Due due = schedule.Next(200);
    CHECK(due.kind == Kind::Run && due.key == 7);

    // Once taken, selecting it again is a new request.
    CHECK(schedule.Request(7, 300) != generation);
    CHECK(schedule.Next(400).kind == Kind::Wait);
}

TEST(CancellingDropsTheWaitingRequestAndStopsTheRunningOne) {
    PrefetchSchedule schedule(200);
    schedule.Request(1, 0);
    PrefetchSchedule::Due running = schedule.Next(200);
    uint64_t waiting = schedule.Request(2, 250);
    schedule.Cancel();
    CHECK(!schedule.IsCurrent(running.generation) && !schedule.IsCurrent(waiting));
    CHECK(schedule.Next(1000).kind == Kind::Idle);
}
