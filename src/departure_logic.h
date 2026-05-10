#ifndef DEPARTURE_LOGIC_H
#define DEPARTURE_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_DEPARTURES 10
#define MAX_DIRECTION_LEN 64

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A single departure entry parsed from VAG EFA response.
 */
typedef struct {
  char direction[MAX_DIRECTION_LEN];
  int schedHour;
  int schedMinute;
  int realHour;
  int realMinute;
  int delayMin;
  int countdown;
  bool valid;
} Departure;

/**
 * Result of parsing departures JSON
 */
typedef struct {
  Departure departures[MAX_DEPARTURES];
  int count;
  bool success;
} DeparturesResult;

/**
 * Check if a direction string matches any keyword in a comma-separated filter
 *
 * @param direction The direction string to check (e.g., "Direction A")
 * @param filter Comma-separated filter keywords (e.g., "Direction A,Direction B")
 * @return true if direction contains any filter keyword, or if filter is empty/NULL
 */
bool matchesDirectionFilter(const char* direction, const char* filter);

/**
 * Parse departures from VAG EFA JSON response.
 *
 * Expected shape:
 *   {
 *     "departureList": [
 *       {
 *         "countdown": "4",
 *         "dateTime":     { "hour": "17", "minute": "26", ... },
 *         "realDateTime": { "hour": "17", "minute": "29", ... },  // optional
 *         "servingLine": { "direction": "...", "number": "3", "realtime": "1" }
 *       }
 *     ]
 *   }
 *
 * @param json The JSON string from the API
 * @param maxResults Maximum number of departures to parse
 * @return DeparturesResult with parsed departures
 */
DeparturesResult parseDeparturesJson(const char* json, int maxResults);

#ifdef __cplusplus
}
#endif

#endif  // DEPARTURE_LOGIC_H
