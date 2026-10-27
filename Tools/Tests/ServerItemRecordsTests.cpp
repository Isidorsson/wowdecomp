#include "WoW/Source/DB/ServerItemRecords.h"
#include "Tools/Tests/Check.h"

// Item.dbc rows for items only the server defines (ServerItemRecords.h): built from the item query reply the client
// cached, so an item without a stock row still equips, sheathes and shows. Fakes stand in for ItemRec and
// ItemStats_C, which need Storm to link; the members are the ones the real types have.

namespace {

struct FakeRec {
    int32_t m_id = 0;
    int32_t m_classID = 0;
    int32_t m_subclassID = 0;
    int32_t m_soundOverrideSubclassID = 0;
    int32_t m_material = 0;
    int32_t m_displayInfoID = 0;
    int32_t m_inventoryType = 0;
    int32_t m_sheatheType = 0;
};

struct FakeStats {
    uint32_t m_class = 4;
    uint32_t m_subClass = 1;
    uint32_t m_soundOverrideSubclass = 0xFFFFFFFF;
    uint32_t m_displayId = 117667;
    uint32_t m_inventoryType = 1;
    uint32_t m_material = 7;
    uint32_t m_sheatheType = 0;
};

}

TEST(AServerItemTakesEveryItemDbcColumnFromItsQueryReply) {
    FakeStats stats;
    stats.m_sheatheType = 3;
    FakeRec rec;
    ServerItemRecords::FillItemRec(100015, stats, rec);
    CHECK(rec.m_id == 100015);
    CHECK(rec.m_classID == 4);
    CHECK(rec.m_subclassID == 1);
    CHECK(rec.m_soundOverrideSubclassID == -1);
    CHECK(rec.m_material == 7);
    CHECK(rec.m_displayInfoID == 117667);
    CHECK(rec.m_inventoryType == 1);
    CHECK(rec.m_sheatheType == 3);
}

TEST(AStockRowWinsOverTheQueryReply) {
    ServerItemRecords::Served<FakeRec> served;
    FakeRec stock;
    stock.m_inventoryType = 5;
    FakeStats stats;
    CHECK(served.Resolve(45435, &stock, &stats) == &stock);
    CHECK(stock.m_inventoryType == 5);
}

TEST(AnItemTheClientHasNotCachedHasNoRow) {
    ServerItemRecords::Served<FakeRec> served;
    CHECK(served.Resolve(100015, nullptr, static_cast<const FakeStats*>(nullptr)) == nullptr);
}

TEST(AServedRowKeepsItsAddressAcrossLookups) {
    ServerItemRecords::Served<FakeRec> served;
    FakeStats stats;
    FakeRec* first = served.Resolve(100015, nullptr, &stats);
    for (int32_t id = 100016; id < 100100; id++) {
        served.Resolve(id, nullptr, &stats);
    }
    CHECK(first != nullptr);
    CHECK(served.Resolve(100015, nullptr, &stats) == first);
    CHECK(first->m_id == 100015);
}

TEST(AServedRowFollowsANewerQueryReply) {
    ServerItemRecords::Served<FakeRec> served;
    FakeStats stats;
    FakeRec* rec = served.Resolve(100015, nullptr, &stats);
    stats.m_inventoryType = 20;
    stats.m_displayId = 110474;
    served.Resolve(100015, nullptr, &stats);
    CHECK(rec->m_inventoryType == 20);
    CHECK(rec->m_displayInfoID == 110474);
}

TEST(EachServerItemGetsItsOwnRow) {
    ServerItemRecords::Served<FakeRec> served;
    FakeStats hood;
    FakeStats robe;
    robe.m_inventoryType = 20;
    FakeRec* a = served.Resolve(100015, nullptr, &hood);
    FakeRec* b = served.Resolve(100017, nullptr, &robe);
    CHECK(a != b);
    CHECK(a->m_inventoryType == 1);
    CHECK(b->m_inventoryType == 20);
}
