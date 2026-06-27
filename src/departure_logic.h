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
 * Why a parse attempt failed. PARSE_OK means success.
 *
 * PARSE_ERR_NO_MEMORY is distinct from PARSE_ERR_INVALID_JSON because it is not
 * recoverable by retrying: the response was well-formed but too large for the
 * document pool, so every retry of the same payload fails identically. The
 * firmware uses this to skip retries and surface a different message.
 */
typedef enum {
  PARSE_OK = 0,
  PARSE_ERR_NULL_INPUT,    // json pointer was NULL
  PARSE_ERR_NO_MEMORY,     // valid JSON, but exceeded the document pool
  PARSE_ERR_INVALID_JSON,  // malformed / truncated JSON
  PARSE_ERR_NO_LIST,       // parsed OK but "departureList" was missing
} ParseError;

/**
 * Result of parsing departures JSON
 */
typedef struct {
  Departure departures[MAX_DEPARTURES];
  int count;
  bool success;
  ParseError error;  // PARSE_OK on success; reason otherwise
} DeparturesResult;

/**
 * Check if a direction string matches any keyword in a comma-separated filter
 *
 * Keywords are comma-separated and matched as substrings. A keyword prefixed
 * with '!' is an exclusion: if the direction contains it, the entry never
 * matches (exclusion always wins over inclusion). A filter of only exclusions
 * (e.g. "!Alpha") shows everything not excluded. An empty/NULL filter matches all.
 *
 * @param direction The direction string to check (e.g., "Direction A")
 * @param filter Comma-separated keywords; '!' prefix excludes (e.g. "Alpha,!Depot")
 * @return true if the direction passes the filter
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

#ifdef ARDUINO
#include <Stream.h>
/**
 * Parse departures by streaming directly from an Arduino Stream (e.g. the HTTP
 * client's network socket) instead of buffering the whole response in RAM.
 *
 * The EFA response is ~167 KB but we only need departureList; buffering it all
 * via http.getString() exhausts the heap once the WiFi stack is up, yielding a
 * truncated body that fails as PARSE_ERR_INVALID_JSON. Streaming with the same
 * field filter keeps peak memory at the filtered ~10 KB regardless of body size.
 *
 * @param stream Source stream positioned at the start of the JSON body
 * @param maxResults Maximum number of departures to parse
 * @return DeparturesResult with parsed departures
 */
DeparturesResult parseDeparturesJsonStream(Stream& stream, int maxResults);
#endif  // ARDUINO
#endif  // __cplusplus

#endif  // DEPARTURE_LOGIC_H
