#ifndef WOW_SOURCE_DB_RETAILCREATURERECORDS_H
#define WOW_SOURCE_DB_RETAILCREATURERECORDS_H

#include <cstdint>

class CreatureDisplayInfoRec;

// Not in the original. A served retail display's fourth texture variation, which fills model texture type 5 and has no
// field in the client's three-variation row: its name in the model's folder, as the row's variations are named. Empty
// for a stock row and for a display without one.
const char* RetailFourthVariation(const CreatureDisplayInfoRec* displayRec);

// Whether the client draws its own CreatureDisplayInfo row for a display id: it has one, and RetailCasc.ini's
// creature_displays does not hand that id to retail.
bool DrawsInstalledCreatureDisplay(uint32_t id);

#endif
