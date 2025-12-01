#ifndef DEPARTURE_LOGIC_H
#define DEPARTURE_LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_DEPARTURES 10
#define MAX_DIRECTION_LEN 64

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parsed time structure from ISO 8601 string
 */
typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    bool valid;
} ParsedTime;

/**
 * Planned time with delay removed
 */
typedef struct {
    int hour;
    int minute;
    int second;
} PlannedTime;

/**
 * A single departure entry
 */
typedef struct {
    char direction[MAX_DIRECTION_LEN];
    ParsedTime when;
    int delaySec;
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
 * Parse an ISO 8601 timestamp string (e.g., "2024-12-01T17:26:45")
 *
 * @param timeString The ISO 8601 formatted string
 * @return ParsedTime struct with parsed values and valid flag
 */
ParsedTime parseIso8601Time(const char* timeString);

/**
 * Check if a direction string matches any keyword in a comma-separated filter
 *
 * @param direction The direction string to check (e.g., "Hauptbahnhof")
 * @param filter Comma-separated filter keywords (e.g., "Hbf,Marktplatz")
 * @return true if direction contains any filter keyword, or if filter is empty/NULL
 */
bool matchesDirectionFilter(const char* direction, const char* filter);

/**
 * Calculate planned time by subtracting delay from actual time
 *
 * @param hour Actual hour
 * @param minute Actual minute
 * @param second Actual second
 * @param delaySec Delay in seconds to subtract
 * @return PlannedTime with delay removed
 */
PlannedTime calculatePlannedTime(int hour, int minute, int second, int delaySec);

/**
 * Calculate minutes until departure
 *
 * @param parsed The parsed departure time
 * @param now Current time as time_t
 * @return Minutes until departure (can be negative if in past)
 */
int calculateMinutesUntil(ParsedTime parsed, time_t now);

/**
 * Convert delay from seconds to minutes (rounded)
 *
 * @param delaySec Delay in seconds
 * @return Delay in minutes
 */
int delaySecondsToMinutes(int delaySec);

/**
 * Parse departures from JSON response
 *
 * @param json The JSON string from the API
 * @param maxResults Maximum number of departures to parse
 * @return DeparturesResult with parsed departures
 */
DeparturesResult parseDeparturesJson(const char* json, int maxResults);

#ifdef __cplusplus
}
#endif

#endif // DEPARTURE_LOGIC_H
