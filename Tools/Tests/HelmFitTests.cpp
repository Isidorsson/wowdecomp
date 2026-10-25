#include <array>
#include <cmath>
#include "Engine/Source/Retail/HelmFit.h"
#include "Tools/Tests/Check.h"
#include "Tools/Tests/M2Builder.h"

using Retail::FitHelmToStockHead;
using Retail::HelmFit;
using Retail::HelmFitFor;
using Retail::DeriveHelmFit;
using Retail::HeadFrame;
using Retail::HelmRaceForPath;
using Retail::ReadHeadFrame;
using Retail::RetailBodyFileDataId;
using Retail::StockBodyPath;

using Vec3 = std::array<float, 3>;

constexpr size_t ATTACHMENTS = 0xF0;
constexpr size_t BOUNDS = 0xA0;
constexpr size_t COLLISION_BOUNDS = 0xBC;

static void PutVec3(std::vector<uint8_t>& blob, size_t at, const Vec3& v) {
    std::memcpy(blob.data() + at, v.data(), 12);
}

static Vec3 GetVec3(const std::vector<uint8_t>& blob, size_t at) {
    Vec3 v;
    std::memcpy(v.data(), blob.data() + at, 12);
    return v;
}

static float GetFloat(const std::vector<uint8_t>& blob, size_t at) {
    float value;
    std::memcpy(&value, blob.data() + at, 4);
    return value;
}

static bool Near(const Vec3& a, const Vec3& b) {
    return std::fabs(a[0] - b[0]) < 1e-5f && std::fabs(a[1] - b[1]) < 1e-5f && std::fabs(a[2] - b[2]) < 1e-5f;
}

static Vec3 Fitted(const Vec3& p, const HelmFit& fit) {
    return { (p[0] - fit.shiftX) / fit.scale, p[1] / fit.scale, (p[2] - fit.shiftZ) / fit.scale };
}

static std::vector<uint8_t> VertexAt(const Vec3& position) {
    std::vector<uint8_t> record = Vertex(255, 0, 0, 0);
    PutVec3(record, 0, position);
    return record;
}

static std::vector<uint8_t> BoneAt(const Vec3& pivot) {
    std::vector<uint8_t> record = Bone(0);
    PutVec3(record, 0x4C, pivot);
    return record;
}

static std::vector<uint8_t> AttachmentAt(const Vec3& position) {
    std::vector<uint8_t> record(0x28, 0);
    PutU32(record, 0x00, 11);
    PutVec3(record, 0x08, position);
    return record;
}

static std::vector<uint8_t> Floats(std::vector<float> values) {
    std::vector<uint8_t> bytes(values.size() * 4);
    std::memcpy(bytes.data(), values.data(), bytes.size());
    return bytes;
}

// A converted helm: one vertex, two bones whose translation tracks share one key, and one attachment.
static std::vector<uint8_t> Helm(const Vec3& vertex, const Vec3& pivot, const Vec3& key) {
    std::vector<uint8_t> blob = NewMd20(264);
    AddArray(blob, M2At::SEQUENCES, 1, Sequence(0, 0x20));
    AddArray(blob, M2At::BONES, 2, Concat({ BoneAt(pivot), BoneAt(pivot) }));
    AddArray(blob, M2At::VERTICES, 1, VertexAt(vertex));
    AddArray(blob, ATTACHMENTS, 1, AttachmentAt(pivot));
    blob.resize((blob.size() + 15) & ~size_t(15), 0);
    size_t keys = blob.size();
    std::vector<uint8_t> keyBytes = Floats({ key[0], key[1], key[2] });
    blob.insert(blob.end(), keyBytes.begin(), keyBytes.end());
    for (uint32_t bone = 0; bone < 2; bone++) {
        SetTrack(blob, RecordAt(blob, M2At::BONES, bone, 0x58) + 0x10, { { { 1, 0 } }, { { 1, static_cast<uint32_t>(keys) } } });
    }
    return blob;
}

static bool RaceOf(const char* path, uint32_t race, uint32_t gender) {
    uint32_t parsedRace = 0;
    uint32_t parsedGender = 0;
    return HelmRaceForPath(path, parsedRace, parsedGender) && parsedRace == race && parsedGender == gender;
}

static bool NoRace(const char* path) {
    uint32_t race;
    uint32_t gender;
    return !HelmRaceForPath(path, race, gender);
}

TEST(OnlyRaceNamedHeadReadsAreFitted) {
    CHECK(RaceOf("item/objectcomponents/head/retail-141045_huf.m2", 1, 1));
    CHECK(RaceOf("item/objectcomponents/head/retail-141045_tam.m2", 6, 0));
    CHECK(HelmFitFor(6, 0)->scale == 1.35f && HelmFitFor(6, 1)->scale == 1.25f && HelmFitFor(7, 1)->scale == 1.15f);
    CHECK(NoRace("item/objectcomponents/head/retail-141045.m2"));
    CHECK(NoRace("item/objectcomponents/weapon/retail-141045_huf.m2"));
    CHECK(NoRace("item/objectcomponents/head/sub/retail-141045_huf.m2"));
    CHECK(NoRace("retail/141045.m2"));
    CHECK(!HelmFitFor(0, 0) && !HelmFitFor(12, 0) && !HelmFitFor(1, 2));
}

TEST(BodiesAreNamedForEachStockRace) {
    CHECK(StockBodyPath(1, 1) == "Character\\Human\\Female\\HumanFemale.m2");
    CHECK(StockBodyPath(6, 0) == "Character\\Tauren\\Male\\TaurenMale.m2");
    CHECK(StockBodyPath(11, 1) == "Character\\Draenei\\Female\\DraeneiFemale.m2");
    CHECK(RetailBodyFileDataId(1, 1) == 119563 && RetailBodyFileDataId(6, 0) == 122055 && RetailBodyFileDataId(11, 1) == 117437);
    CHECK(StockBodyPath(12, 0).empty() && !RetailBodyFileDataId(0, 0) && !RetailBodyFileDataId(1, 2));
}

static std::vector<uint8_t> BoneUnder(const Vec3& pivot, uint16_t parent, int32_t keyBone = -1) {
    std::vector<uint8_t> record = BoneAt(pivot);
    PutU32(record, 0x00, static_cast<uint32_t>(keyBone));
    PutU16(record, 0x08, parent);
    return record;
}

// A body of root, spine and head (key bone 6), with the helm attachment on a bone of its own under the head, or
// with no head bone above it.
static std::vector<uint8_t> Body(const Vec3& head, const Vec3& attachment, bool headBone = true) {
    std::vector<uint8_t> blob = NewMd20(264);
    AddArray(blob, M2At::BONES, 4, Concat({ BoneUnder({ 0, 0, 0 }, 0xFFFF), BoneUnder({ 0, 0, 1.2f }, 0), BoneUnder(head, 1, headBone ? 6 : -1), BoneUnder(attachment, 2) }));
    std::vector<uint8_t> other = AttachmentAt({ 0, 0, 0 });
    PutU32(other, 0x00, 1);
    std::vector<uint8_t> helm = AttachmentAt(attachment);
    PutU16(helm, 0x04, 3);
    AddArray(blob, ATTACHMENTS, 2, Concat({ other, helm }));
    return blob;
}

TEST(HeadFrameReadsTheHelmAttachmentAndTheChainAboveTheHead) {
    HeadFrame frame;
    std::string error;

    CHECK(ReadHeadFrame(Body({ -0.0331f, 0, 1.7119f }, { 0.052f, 0, 1.897f }), frame, error));
    CHECK(Near(frame.attachment, { 0.052f, 0, 1.897f }));
    CHECK(frame.chain.size() == 3 && Near(frame.chain[0], { -0.0331f, 0, 1.7119f }) && Near(frame.chain[2], { 0, 0, 0 }));

    CHECK(ReadHeadFrame(Md21File(Body({ 0, 0, 1.5f }, { 0, 0, 1.5f })), frame, error));
    CHECK(Near(frame.attachment, { 0, 0, 1.5f }) && frame.chain.size() == 3);
}

TEST(BodiesWithoutAHeadFrameAreRefused) {
    HeadFrame frame;
    std::string error;
    std::vector<uint8_t> noHelm = Body({ 0, 0, 1.5f }, { 0, 0, 1.6f });
    PutU32(noHelm, RecordAt(noHelm, ATTACHMENTS, 1, 0x28), 2);

    CHECK(!ReadHeadFrame(noHelm, frame, error) && error.find("helm attachment") != std::string::npos);
    CHECK(!ReadHeadFrame(Body({ 0, 0, 1.5f }, { 0, 0, 1.6f }, false), frame, error) && error.find("head bone") != std::string::npos);
    CHECK(!ReadHeadFrame(std::vector<uint8_t>(16, 0), frame, error));
}

TEST(AnHdPortWearsRetailHelmsMovedByItsAttachment) {
    HeadFrame stock = { { 0.052f, 0, 1.897f }, { { -0.0331f, 0, 1.7119f }, { 0, 0, 1.2f }, { 0, 0, 0 } } };
    HeadFrame retail = { { -0.0331f, 0, 1.7119f }, stock.chain };
    HelmFit fit;

    CHECK(DeriveHelmFit(stock, retail, fit));
    CHECK(fit.scale == 1.0f && std::fabs(fit.shiftX - 0.0851f) < 1e-5f && std::fabs(fit.shiftZ - 0.1851f) < 1e-5f);

    CHECK(DeriveHelmFit(retail, retail, fit) && fit.shiftX == 0.0f && fit.shiftZ == 0.0f);
}

TEST(AnotherSkeletonKeepsTheTable) {
    HeadFrame retail = { { 0.3827f, 0, 1.8888f }, { { 0.3827f, 0, 1.8888f }, { 0.2877f, 0, 1.8855f }, { 0, 0, 0 } } };
    HeadFrame moved = retail;
    moved.chain[0][2] += 0.005f;
    HeadFrame shorter = retail;
    shorter.chain.pop_back();
    HelmFit fit = { 2.0f, 3.0f, 4.0f };

    CHECK(!DeriveHelmFit(moved, retail, fit) && !DeriveHelmFit(shorter, retail, fit));
    CHECK(fit.scale == 2.0f);
}

TEST(HumanFemaleHelmMovesDownAndBack) {
    std::vector<uint8_t> helm = Helm({ 0.1851f, 0.2f, 0.3852f }, { 0, 0, 0 }, { 0, 0, 0 });
    std::string error;

    CHECK(FitHelmToStockHead(helm, *HelmFitFor(1, 1), error));
    CHECK(Near(GetVec3(helm, RecordAt(helm, M2At::VERTICES, 0, 0x30)), { 0.1f, 0.2f, 0.2f }));
}

TEST(ScaledRaceShrinksPointsAndKeysOnce) {
    const HelmFit& fit = *HelmFitFor(6, 0);
    Vec3 vertex = { 0.7f, 0.1f, 0.9f };
    Vec3 pivot = { 0.5f, 0.0f, 0.5f };
    Vec3 key = { 0.27f, 0.0f, 0.0f };
    std::vector<uint8_t> helm = Helm(vertex, pivot, key);
    std::string error;

    CHECK(FitHelmToStockHead(helm, fit, error));
    Vec3 fittedPivot = GetVec3(helm, RecordAt(helm, M2At::BONES, 0, 0x58) + 0x4C);
    CHECK(Near(fittedPivot, Fitted(pivot, fit)));
    CHECK(Near(GetVec3(helm, RecordAt(helm, ATTACHMENTS, 0, 0x28) + 0x08), fittedPivot));
    size_t keyAt = TrackEntry(helm, RecordAt(helm, M2At::BONES, 1, 0x58) + 0x10, 0, true).second;
    Vec3 fittedKey = GetVec3(helm, keyAt);
    CHECK(Near(fittedKey, { 0.2f, 0.0f, 0.0f }));

    // A translated vertex lands where the fitted vertex translated by the fitted key does.
    Vec3 fittedVertex = GetVec3(helm, RecordAt(helm, M2At::VERTICES, 0, 0x30));
    Vec3 moved = Fitted({ vertex[0] + key[0], vertex[1] + key[1], vertex[2] + key[2] }, fit);
    CHECK(Near({ fittedVertex[0] + fittedKey[0], fittedVertex[1] + fittedKey[1], fittedVertex[2] + fittedKey[2] }, moved));
}

TEST(BoundsMoveWithTheHelmButEmptyOnesStay) {
    const HelmFit& fit = *HelmFitFor(10, 0);
    std::vector<uint8_t> helm = Helm({ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 });
    PutVec3(helm, BOUNDS, { 0.0f, -0.1f, 0.1f });
    PutVec3(helm, BOUNDS + 12, { 0.2f, 0.1f, 0.4f });
    std::memcpy(helm.data() + BOUNDS + 24, Floats({ 0.45f }).data(), 4);
    std::string error;

    CHECK(FitHelmToStockHead(helm, fit, error));
    CHECK(Near(GetVec3(helm, BOUNDS), { 0.0f - 0.0799f, -0.1f, 0.1f - 0.1678f }));
    CHECK(Near(GetVec3(helm, BOUNDS + 12), { 0.2f - 0.0799f, 0.1f, 0.4f - 0.1678f }));
    CHECK(GetFloat(helm, BOUNDS + 24) > 0.45f);
    CHECK(Near(GetVec3(helm, COLLISION_BOUNDS), { 0, 0, 0 }) && GetFloat(helm, COLLISION_BOUNDS + 24) == 0.0f);
}

TEST(OnlyConvertedModelsAreFitted) {
    std::vector<uint8_t> file = Md21File(MinimalMd20());
    std::vector<uint8_t> broken = Helm({ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 });
    PutU32(broken, M2At::VERTICES, 1000);
    std::string error;

    CHECK(!FitHelmToStockHead(file, *HelmFitFor(1, 0), error) && error.find("MD20") != std::string::npos);
    CHECK(!FitHelmToStockHead(broken, *HelmFitFor(1, 0), error));
}

// B6: a served race draws on its own retail body, which the retail helms were made for: they hang unmoved.
TEST(RetailBodyRacesWearRetailHelmsUnmoved) {
    CHECK(Retail::HelmFitsUnmoved(22, 0) && Retail::HelmFitsUnmoved(32, 1) && Retail::HelmFitsUnmoved(84, 0));
    CHECK(Retail::HelmFitsUnmoved(0, 0));
    // Goblins wear retail helms only on their retail body (the installed one is no port of it).
    CHECK(Retail::HelmFitsUnmoved(9, 0) && Retail::HelmFitsUnmoved(9, 1));
    CHECK(!Retail::HelmFitsUnmoved(1, 0) && !Retail::HelmFitsUnmoved(11, 1));
}
