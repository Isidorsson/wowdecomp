#include <vector>
#include "Engine/Source/Retail/MemoryCache.h"
#include "Tools/Tests/Check.h"

using Retail::MemoryCache;

static std::vector<uint8_t> Filled(size_t size, uint8_t value) {
    return std::vector<uint8_t>(size, value);
}

TEST(MemoryEntriesComeBackAsTheyWerePut) {
    MemoryCache cache(100, 50);
    std::vector<uint8_t> bytes;

    CHECK(!cache.Find(1, bytes) && !cache.Has(1));
    cache.Put(1, { 1, 2, 3 });
    CHECK(cache.Has(1));
    CHECK(cache.Find(1, bytes) && bytes == std::vector<uint8_t>({ 1, 2, 3 }));
    CHECK(cache.Bytes() == 3);
}

TEST(MemoryEntriesPastTheCapDropTheLeastRecentlyUsed) {
    MemoryCache cache(100, 50);
    std::vector<uint8_t> bytes;

    cache.Put(1, Filled(40, 1));
    cache.Put(2, Filled(40, 2));
    CHECK(cache.Find(1, bytes));   // 2 is now the least recently used.
    cache.Put(3, Filled(40, 3));

    CHECK(!cache.Find(2, bytes));
    CHECK(cache.Find(1, bytes) && bytes == Filled(40, 1));
    CHECK(cache.Find(3, bytes) && bytes == Filled(40, 3));
    CHECK(cache.Bytes() == 80);
}

TEST(MemoryEntriesOverTheEntryLimitAreNotKept) {
    MemoryCache cache(100, 50);
    std::vector<uint8_t> bytes;

    cache.Put(1, Filled(10, 1));
    cache.Put(2, Filled(51, 2));
    CHECK(!cache.Find(2, bytes));
    CHECK(cache.Find(1, bytes));
    CHECK(cache.Bytes() == 10);
}

TEST(MemoryEntriesPutAgainReplaceTheOldBytes) {
    MemoryCache cache(100, 50);
    std::vector<uint8_t> bytes;

    cache.Put(1, Filled(30, 1));
    cache.Put(1, Filled(20, 2));
    CHECK(cache.Find(1, bytes) && bytes == Filled(20, 2));
    CHECK(cache.Bytes() == 20);
}
