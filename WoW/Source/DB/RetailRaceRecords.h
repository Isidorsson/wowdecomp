#ifndef WOW_SOURCE_DB_RETAILRACERECORDS_H
#define WOW_SOURCE_DB_RETAILRACERECORDS_H

#include <cstdint>

// Not in the original. Retail races the installed ChrRaces lacks are served from the retail tables (RetailRaces.h),
// hidden from character creation, so their NPCs draw through the character component; the race-sized component
// tables are sized for them.

// The highest race id the race-sized tables must hold: the installed ChrRaces' highest id, or the highest served one.
uint32_t ChrRaceCapacity();

// Whether the character component needs the race's CharSections rows (RetailRaces NeedsCharSections): a served race
// with a bake does not, every installed race does.
bool ChrRaceNeedsCharSections(uint32_t race, bool baked);

// One line in Logs/RetailAssets.log, once per race: a race the race-sized tables cannot hold is drawn without its
// character component.
void LogChrRaceRefused(uint32_t race, uint32_t sex);

#endif
