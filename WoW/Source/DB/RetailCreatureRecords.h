#ifndef WOW_SOURCE_DB_RETAILCREATURERECORDS_H
#define WOW_SOURCE_DB_RETAILCREATURERECORDS_H

class CreatureDisplayInfoRec;

// Not in the original. A served retail display's fourth texture variation, which fills model texture type 5 and has no
// field in the client's three-variation row: its name in the model's folder, as the row's variations are named. Empty
// for a stock row and for a display without one.
const char* RetailFourthVariation(const CreatureDisplayInfoRec* displayRec);

#endif
