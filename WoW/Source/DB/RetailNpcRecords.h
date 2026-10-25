#ifndef WOW_SOURCE_DB_RETAILNPCRECORDS_H
#define WOW_SOURCE_DB_RETAILNPCRECORDS_H

#include <cstdint>

// Not in the original. A served retail creature display with an ExtendedDisplayInfoID is drawn on the installed body
// of its race and gender (RetailNpcDisplay.h): this gives the body's stock model id and the served Extra row's id, or
// false (with one log line) when the display keeps its retail model.
bool ServeRetailNpcDisplay(uint32_t displayId, uint32_t retailExtraId, int32_t& modelId, int32_t& extraId);

#endif
