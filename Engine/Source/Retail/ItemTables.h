#ifndef ENGINE_SOURCE_RETAIL_ITEMTABLES_H
#define ENGINE_SOURCE_RETAIL_ITEMTABLES_H

#include <cstdint>

namespace Retail {

// DBFilesClient item tables and the WoWDBDefs layouts read from them (wow-toolbox-editor casc/skins.rs, casc/gear.rs).
constexpr uint32_t MODEL_FILE_DATA_FDID = 1337833;
constexpr uint32_t MODEL_FILE_DATA_LAYOUT = 0x2AE4E788;
constexpr uint32_t COMPONENT_MODEL_FILE_DATA_FDID = 1349053;
constexpr uint32_t COMPONENT_MODEL_FILE_DATA_LAYOUT = 0xAD90D87A;
constexpr uint32_t ITEM_DISPLAY_INFO_FDID = 1266429;
constexpr uint32_t ITEM_DISPLAY_INFO_LAYOUT = 0x9F3AB8A9;
constexpr uint32_t ITEM_DISPLAY_INFO_MODEL_MAT_RES_FDID = 4050937;
constexpr uint32_t ITEM_DISPLAY_INFO_MODEL_MAT_RES_LAYOUT = 0x52510D63;
constexpr uint32_t TEXTURE_FILE_DATA_FDID = 982459;
constexpr uint32_t TEXTURE_FILE_DATA_LAYOUT = 0xBD7C74C2;
constexpr uint32_t ITEM_APPEARANCE_FDID = 982462;
constexpr uint32_t ITEM_APPEARANCE_LAYOUT = 0x481C4281;

}

#endif
