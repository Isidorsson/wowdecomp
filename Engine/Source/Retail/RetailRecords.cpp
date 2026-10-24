#include "Engine/Source/Retail/RetailAnimation.h"
#include "Engine/Source/Retail/RetailModelData.h"

namespace Retail {

// From version 271 the constant FoV is gone and the record ends in an FoV track. The track's unit is
// unverified, so the stock 3.3.5 values are used, as wow-toolbox-editor's downporter does.
constexpr size_t RETAIL_CAMERA_SIZE = 116;
constexpr size_t CAMERA_SIZE = 100;
constexpr size_t RETAIL_SHARED_FIELDS = 0x04;       // farClip through roll: the same bytes 4 later in 3.3.5.
constexpr size_t RETAIL_FOV_TRACK = 0x60;
constexpr uint32_t PORTRAIT_CAMERA = 0;
constexpr float PORTRAIT_FOV = 0.7f;
constexpr float DEFAULT_FOV = 0.97f;

bool ConvertCameras(std::vector<uint8_t>& m2, std::string& error) {
    Span cameras;
    if (!ReadSpan(m2, M2At::CAMERAS, RETAIL_CAMERA_SIZE, "cameras", cameras, error)) {
        return false;
    }

    if (!cameras.count) {
        return true;
    }

    std::vector<uint8_t> converted(cameras.count * CAMERA_SIZE, 0);

    for (uint32_t i = 0; i < cameras.count; i++) {
        size_t from = cameras.At(i, RETAIL_CAMERA_SIZE);
        size_t to = i * CAMERA_SIZE;
        uint32_t type = Get<uint32_t>(m2, from);

        Put<uint32_t>(converted, to, type);
        Put<float>(converted, to + 4, type == PORTRAIT_CAMERA ? PORTRAIT_FOV : DEFAULT_FOV);
        std::memcpy(converted.data() + to + 8, m2.data() + from + RETAIL_SHARED_FIELDS, RETAIL_FOV_TRACK - RETAIL_SHARED_FIELDS);
    }

    Put<uint32_t>(m2, M2At::CAMERAS + 4, AppendAligned(m2, converted));
    return true;
}

}
