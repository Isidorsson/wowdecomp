#ifndef WOW_SOURCE_DB_RETAILNPCRECORDS_H
#define WOW_SOURCE_DB_RETAILNPCRECORDS_H

#include <cstddef>
#include <cstdint>

// Not in the original. A served retail creature display with an ExtendedDisplayInfoID is drawn on the installed body
// of its race and gender, or for Goblin and races 22 and up on the display's retail model (RetailNpcDisplay.h): this
// gives the body's model id and the served Extra row's id, or false (with one log line) when the display keeps its
// retail model.
bool ServeRetailNpcDisplay(uint32_t displayId, uint32_t retailExtraId, uint32_t retailModelId, int32_t& modelId, int32_t& extraId);

// A texture a retail body replaces by type (hair, eyes, jewelry, fur) with a file of the NPC's choices.
struct RetailBodyTextureView {
    uint32_t type;
    const char* path;
};

// What a retail body draws (RetailBodyLook.h): one mesh per geoset group, and its chosen textures.
struct RetailBodyLookView {
    const uint32_t* geosets = nullptr;
    size_t count = 0;
    const RetailBodyTextureView* textures = nullptr;
    size_t textureCount = 0;
};

// The look of the served Extra row whose bake the character component draws (its npc skin texture); false for any
// other skin, a stock NPC's or an installed body's.
bool FindRetailBodyLook(const char* npcSkinTexture, RetailBodyLookView& look);

#endif
