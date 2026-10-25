#include "Engine/Source/Retail/HelmFit.h"
#include <cmath>
#include <cstring>
#include <set>
#include "Engine/Source/Retail/RetailModelData.h"
#include "Engine/Source/Retail/RetailPath.h"

namespace Retail {

// By race (Hu, Or, Dw, Ni, Sc, Ta, Gn, Tr, Go, Be, Dr), male then female.
static const HelmFit HELM_FITS[] = {
    { 1.0f, 0.0537f, 0.1985f }, { 1.0f, 0.0851f, 0.1852f },
    { 1.0f, 0.1442f, 0.2059f }, { 1.0f, 0.0786f, 0.1783f },
    { 1.0f, 0.0084f, 0.1769f }, { 1.0f, -0.0090f, 0.2084f },
    { 1.0f, 0.0974f, 0.1883f }, { 1.0f, 0.0642f, 0.2093f },
    { 1.0f, 0.1259f, 0.1310f }, { 1.0f, 0.0090f, 0.1611f },
    { 1.35f, 0.2558f, 0.1407f }, { 1.25f, 0.2242f, 0.1320f },
    { 1.15f, -0.0007f, 0.2797f }, { 1.15f, 0.0106f, 0.3120f },
    { 1.0f, 0.1238f, 0.1681f }, { 1.0f, 0.0878f, 0.0888f },
    { 1.0f, 0.0555f, 0.1697f }, { 1.0f, 0.0550f, 0.1694f },
    { 1.0f, 0.0799f, 0.1678f }, { 1.0f, -0.0002f, 0.2048f },
    { 1.0f, 0.0747f, 0.2591f }, { 1.0f, 0.0693f, 0.2057f },
};
constexpr uint32_t FIT_RACES = sizeof(HELM_FITS) / sizeof(HELM_FITS[0]) / 2;
constexpr const char* HELM_FOLDER = "item/objectcomponents/head/";

static const char* const BODY_RACES[FIT_RACES] = {
    "Human", "Orc", "Dwarf", "NightElf", "Scourge", "Tauren", "Gnome", "Troll", "Goblin", "BloodElf", "Draenei",
};

// character/<race>/<sex>/<race><sex>.m2 in retail, which holds the HD body under the old name.
static const uint32_t RETAIL_BODIES[FIT_RACES * 2] = {
    119940, 119563, 121287, 121087, 118355, 118135, 120791, 120590, 121768, 121608, 122055,
    121961, 119159, 119063, 122560, 122414, 119376, 119369, 117170, 116921, 117721, 117437,
};

constexpr uint32_t HELM_ATTACHMENT = 11;
constexpr int32_t HEAD_KEY_BONE = 6;
constexpr size_t ATTACHMENT_SIZE = 0x28;
constexpr size_t ATTACHMENT_BONE = 0x04;
constexpr size_t ATTACHMENT_POSITION = 0x08;
constexpr size_t BONE_PARENT = 0x08;
// Pivots this close are the same bone: the HD ports copy retail's to the last digit.
constexpr float SAME_PIVOT = 0.001f;

constexpr size_t VEC3_SIZE = 12;
constexpr size_t BOUNDS = 0xA0;
constexpr size_t COLLISION_BOUNDS = 0xBC;
constexpr size_t COLLISION_POSITIONS = 0xE0;
constexpr size_t SEQUENCE_BOUNDS = 0x20;
constexpr size_t BONE_TRANSLATION = 0x10;
constexpr size_t BONE_PIVOT = 0x4C;
constexpr size_t TRACK_KEYS = 0x0C;
constexpr size_t SPLINE_KEY_SIZE = 3 * VEC3_SIZE;
constexpr size_t PARTICLE_SPLINE = 0x1C0;
constexpr size_t CAMERA_POSITIONS = 0x10;
constexpr size_t CAMERA_POSITION_BASE = 0x24;
constexpr size_t CAMERA_TARGETS = 0x30;
constexpr size_t CAMERA_TARGET_BASE = 0x44;

const HelmFit* HelmFitFor(uint32_t race, uint32_t gender) {
    return race >= 1 && race <= FIT_RACES && gender <= 1 ? &HELM_FITS[(race - 1) * 2 + gender] : nullptr;
}

bool HelmRaceForPath(const std::string& normalized, uint32_t& race, uint32_t& gender) {
    RaceFileRequest request;
    size_t folder = normalized.rfind('/') + 1;
    if (normalized.compare(0, folder, HELM_FOLDER) != 0 || !ParseRaceFileName(normalized, request)) {
        return false;
    }
    race = request.race;
    gender = request.gender;
    return true;
}

std::string StockBodyPath(uint32_t race, uint32_t gender) {
    if (!HelmFitFor(race, gender)) {
        return std::string();
    }
    std::string name = BODY_RACES[race - 1];
    std::string sex = gender ? "Female" : "Male";
    return "Character\\" + name + "\\" + sex + "\\" + name + sex + ".m2";
}

uint32_t RetailBodyFileDataId(uint32_t race, uint32_t gender) {
    return HelmFitFor(race, gender) ? RETAIL_BODIES[(race - 1) * 2 + gender] : 0;
}

static std::array<float, 3> GetVec3(const std::vector<uint8_t>& m2, size_t at) {
    return { Get<float>(m2, at), Get<float>(m2, at + 4), Get<float>(m2, at + 8) };
}

// The pivots from the first head bone at or above bone to the root.
static bool ReadHeadChain(const std::vector<uint8_t>& m2, const Span& bones, uint32_t bone, HeadFrame& frame, std::string& error) {
    frame.chain.clear();
    bool head = false;
    for (uint32_t steps = 0; bone < bones.count; steps++) {
        if (steps > bones.count) {
            return Fail(error, "the body's bones loop");
        }
        size_t at = bones.At(bone, BONE_SIZE);
        head = head || Get<int32_t>(m2, at) == HEAD_KEY_BONE;
        if (head) {
            frame.chain.push_back(GetVec3(m2, at + BONE_PIVOT));
        }
        int16_t parent = Get<int16_t>(m2, at + BONE_PARENT);
        bone = parent < 0 ? bones.count : static_cast<uint32_t>(parent);
    }
    return head || Fail(error, "the helm attachment is not under a head bone");
}

bool ReadHeadFrame(const std::vector<uint8_t>& model, HeadFrame& frame, std::string& error) {
    std::vector<uint8_t> chunk;
    const std::vector<uint8_t>* m2 = &model;
    if (model.size() >= 8 && std::memcmp(model.data(), "MD21", 4) == 0) {
        uint32_t size = Get<uint32_t>(model, 4);
        if (uint64_t(size) + 8 > model.size()) {
            return Fail(error, "the MD21 chunk runs past the end of the file");
        }
        chunk.assign(model.begin() + 8, model.begin() + 8 + size);
        m2 = &chunk;
    }
    if (m2->size() < M2At::HEADER || std::memcmp(m2->data(), "MD20", 4) != 0) {
        return Fail(error, "a head frame needs an M2 body");
    }

    Span bones;
    Span attachments;
    if (!ReadSpan(*m2, M2At::BONES, BONE_SIZE, "bones", bones, error) || !ReadSpan(*m2, M2At::ATTACHMENTS, ATTACHMENT_SIZE, "attachments", attachments, error)) {
        return false;
    }
    for (uint32_t i = 0; i < attachments.count; i++) {
        size_t at = attachments.At(i, ATTACHMENT_SIZE);
        if (Get<uint32_t>(*m2, at) == HELM_ATTACHMENT) {
            frame.attachment = GetVec3(*m2, at + ATTACHMENT_POSITION);
            return ReadHeadChain(*m2, bones, Get<uint16_t>(*m2, at + ATTACHMENT_BONE), frame, error);
        }
    }
    return Fail(error, "the body has no helm attachment");
}

bool DeriveHelmFit(const HeadFrame& stock, const HeadFrame& retail, HelmFit& fit) {
    if (stock.chain.size() != retail.chain.size()) {
        return false;
    }
    for (size_t i = 0; i < stock.chain.size(); i++) {
        for (size_t axis = 0; axis < 3; axis++) {
            if (std::fabs(stock.chain[i][axis] - retail.chain[i][axis]) > SAME_PIVOT) {
                return false;
            }
        }
    }
    fit = { 1.0f, stock.attachment[0] - retail.attachment[0], stock.attachment[2] - retail.attachment[2] };
    return true;
}

// Fits one model's points and offsets in place, each offset array once however many tracks share it.
class HelmFitter {
    public:
    HelmFitter(std::vector<uint8_t>& m2, const HelmFit& fit) : m_m2(m2), m_fit(fit) {}

    void Point(size_t at) const {
        const float shift[3] = { this->m_fit.shiftX, 0.0f, this->m_fit.shiftZ };
        for (size_t axis = 0; axis < 3; axis++) {
            size_t field = at + axis * 4;
            Put<float>(this->m_m2, field, (Get<float>(this->m_m2, field) - shift[axis]) / this->m_fit.scale);
        }
    }

    void Bounds(size_t at) const {
        float box[6];
        std::memcpy(box, this->m_m2.data() + at, sizeof(box));
        float radius = Get<float>(this->m_m2, at + sizeof(box));
        if (std::memcmp(box, box + 3, VEC3_SIZE) == 0 && radius == 0.0f) {
            return;
        }
        this->Point(at);
        this->Point(at + VEC3_SIZE);
        float shift = std::sqrt(this->m_fit.shiftX * this->m_fit.shiftX + this->m_fit.shiftZ * this->m_fit.shiftZ);
        Put<float>(this->m_m2, at + sizeof(box), (radius + shift) / this->m_fit.scale);
    }

    // Each record's point at field, for the records of the array at header.
    bool Points(size_t header, size_t recordSize, size_t field, std::string& error) const {
        Span span;
        if (!ReadSpan(this->m_m2, header, recordSize, "fitted records", span, error)) {
            return false;
        }
        for (uint32_t i = 0; i < span.count; i++) {
            this->Point(span.At(i, recordSize) + field);
        }
        return true;
    }

    // Every float of the array at header, of elements size bytes, divided by the scale.
    bool Offsets(size_t header, size_t size, std::string& error) {
        Span span;
        if (!ReadSpan(this->m_m2, header, size, "fitted offsets", span, error)) {
            return false;
        }
        if (!span.count || !this->m_scaled.insert(span.offset).second) {
            return true;
        }
        for (size_t at = span.offset; at < span.At(span.count, size); at += 4) {
            Put<float>(this->m_m2, at, Get<float>(this->m_m2, at) / this->m_fit.scale);
        }
        return true;
    }

    // The per-sequence keys of the M2Track at track, of keySize bytes each.
    bool TrackKeys(size_t track, size_t keySize, std::string& error) {
        Span sequences;
        if (!ReadSpan(this->m_m2, track + TRACK_KEYS, 8, "track keys", sequences, error)) {
            return false;
        }
        for (uint32_t s = 0; s < sequences.count; s++) {
            if (!this->Offsets(sequences.At(s, 8), keySize, error)) {
                return false;
            }
        }
        return true;
    }

    // The keys of each record's M2Track at track, for the records of the array at header.
    bool RecordTracks(size_t header, size_t recordSize, size_t track, size_t keySize, std::string& error) {
        Span span;
        if (!ReadSpan(this->m_m2, header, recordSize, "fitted records", span, error)) {
            return false;
        }
        for (uint32_t i = 0; i < span.count; i++) {
            if (!this->TrackKeys(span.At(i, recordSize) + track, keySize, error)) {
                return false;
            }
        }
        return true;
    }

    // The elements of each record's M2Array at field, for the records of the array at header.
    bool RecordArrays(size_t header, size_t recordSize, size_t field, size_t size, std::string& error) {
        Span span;
        if (!ReadSpan(this->m_m2, header, recordSize, "fitted records", span, error)) {
            return false;
        }
        for (uint32_t i = 0; i < span.count; i++) {
            if (!this->Offsets(span.At(i, recordSize) + field, size, error)) {
                return false;
            }
        }
        return true;
    }

    private:
    std::vector<uint8_t>& m_m2;
    const HelmFit& m_fit;
    std::set<uint32_t> m_scaled;
};

static bool FitRecordPoints(const HelmFitter& fitter, std::string& error) {
    return fitter.Points(M2At::VERTICES, VERTEX_SIZE, 0, error)
        && fitter.Points(M2At::BONES, BONE_SIZE, BONE_PIVOT, error)
        && fitter.Points(M2At::ATTACHMENTS, 0x28, 0x08, error)
        && fitter.Points(M2At::EVENTS, 0x24, 0x0C, error)
        && fitter.Points(M2At::LIGHTS, 0x9C, 0x04, error)
        && fitter.Points(M2At::RIBBONS, 0xB0, 0x08, error)
        && fitter.Points(M2At::PARTICLES, 0x1DC, 0x08, error)
        && fitter.Points(M2At::CAMERAS, 0x64, CAMERA_POSITION_BASE, error)
        && fitter.Points(M2At::CAMERAS, 0x64, CAMERA_TARGET_BASE, error)
        && fitter.Points(COLLISION_POSITIONS, VEC3_SIZE, 0, error);
}

static bool FitBounds(const HelmFitter& fitter, const std::vector<uint8_t>& m2, std::string& error) {
    Span sequences;
    if (!ReadSpan(m2, M2At::SEQUENCES, SEQUENCE_SIZE, "sequences", sequences, error)) {
        return false;
    }
    fitter.Bounds(BOUNDS);
    fitter.Bounds(COLLISION_BOUNDS);
    for (uint32_t s = 0; s < sequences.count; s++) {
        fitter.Bounds(sequences.At(s, SEQUENCE_SIZE) + SEQUENCE_BOUNDS);
    }
    return true;
}

bool FitHelmToStockHead(std::vector<uint8_t>& m2, const HelmFit& fit, std::string& error) {
    if (m2.size() < M2At::HEADER || std::memcmp(m2.data(), "MD20", 4) != 0) {
        return Fail(error, "a helm fit needs a converted MD20 model");
    }

    HelmFitter fitter(m2, fit);
    return FitRecordPoints(fitter, error) && FitBounds(fitter, m2, error)
        && fitter.RecordTracks(M2At::BONES, BONE_SIZE, BONE_TRANSLATION, VEC3_SIZE, error)
        && fitter.RecordTracks(M2At::CAMERAS, 0x64, CAMERA_POSITIONS, SPLINE_KEY_SIZE, error)
        && fitter.RecordTracks(M2At::CAMERAS, 0x64, CAMERA_TARGETS, SPLINE_KEY_SIZE, error)
        && fitter.RecordArrays(M2At::PARTICLES, 0x1DC, PARTICLE_SPLINE, VEC3_SIZE, error);
}

}
