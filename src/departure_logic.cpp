#include "departure_logic.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ArduinoJson.h>

ParsedTime parseIso8601Time(const char* timeString) {
    ParsedTime result = {0, 0, 0, 0, 0, 0, false};

    if (timeString == NULL || strlen(timeString) < 19) {
        return result;
    }

    int matched = sscanf(timeString, "%d-%d-%dT%d:%d:%d",
                         &result.year, &result.month, &result.day,
                         &result.hour, &result.minute, &result.second);

    if (matched == 6) {
        result.valid = true;
    }

    return result;
}

bool matchesDirectionFilter(const char* direction, const char* filter) {
    // Empty or NULL filter matches everything
    if (filter == NULL || strlen(filter) == 0) {
        return true;
    }

    if (direction == NULL) {
        return false;
    }

    // Make a copy of filter since we'll modify it
    size_t filterLen = strlen(filter);
    char* filterCopy = (char*)malloc(filterLen + 1);
    if (filterCopy == NULL) {
        return false;
    }
    strcpy(filterCopy, filter);

    bool matches = false;
    char* start = filterCopy;

    while (*start != '\0') {
        // Find the next comma or end of string
        char* comma = strchr(start, ',');
        if (comma != NULL) {
            *comma = '\0';
        }

        // Trim leading whitespace
        while (*start == ' ' || *start == '\t') {
            start++;
        }

        // Trim trailing whitespace
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        // Check if direction contains this keyword
        if (strlen(start) > 0 && strstr(direction, start) != NULL) {
            matches = true;
            break;
        }

        // Move to next keyword
        if (comma != NULL) {
            start = comma + 1;
        } else {
            break;
        }
    }

    free(filterCopy);
    return matches;
}

PlannedTime calculatePlannedTime(int hour, int minute, int second, int delaySec) {
    PlannedTime result;

    result.second = second - delaySec;
    result.minute = minute;
    result.hour = hour;

    // Handle underflow for seconds
    while (result.second < 0) {
        result.second += 60;
        result.minute--;
    }

    // Handle underflow for minutes
    while (result.minute < 0) {
        result.minute += 60;
        result.hour--;
    }

    // Handle underflow for hours (wrap around midnight)
    while (result.hour < 0) {
        result.hour += 24;
    }

    return result;
}

int calculateMinutesUntil(ParsedTime parsed, time_t now) {
    if (!parsed.valid) {
        return -1;
    }

    struct tm depTm = {0};
    depTm.tm_year = parsed.year - 1900;
    depTm.tm_mon = parsed.month - 1;
    depTm.tm_mday = parsed.day;
    depTm.tm_hour = parsed.hour;
    depTm.tm_min = parsed.minute;
    depTm.tm_sec = parsed.second;
    depTm.tm_isdst = -1;

    time_t depTime = mktime(&depTm);
    double diffSeconds = difftime(depTime, now);

    return (int)(diffSeconds / 60);
}

int delaySecondsToMinutes(int delaySec) {
    return delaySec / 60;
}

DeparturesResult parseDeparturesJson(const char* json, int maxResults) {
    DeparturesResult result = {{}, 0, false};

    if (json == NULL) {
        return result;
    }

    if (maxResults > MAX_DEPARTURES) {
        maxResults = MAX_DEPARTURES;
    }

    StaticJsonDocument<200> filter;
    filter["departures"][0]["direction"] = true;
    filter["departures"][0]["when"] = true;
    filter["departures"][0]["delay"] = true;

    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));

    if (error) {
        return result;
    }

    JsonArray departures = doc["departures"];
    if (departures.isNull()) {
        return result;
    }

    result.success = true;

    for (JsonObject dep : departures) {
        if (result.count >= maxResults) break;

        Departure* d = &result.departures[result.count];

        // Parse direction
        const char* direction = dep["direction"] | "";
        strncpy(d->direction, direction, MAX_DIRECTION_LEN - 1);
        d->direction[MAX_DIRECTION_LEN - 1] = '\0';

        // Parse when
        const char* whenStr = dep["when"] | "";
        d->when = parseIso8601Time(whenStr);

        // Parse delay (can be null, default to 0)
        d->delaySec = dep["delay"] | 0;

        // Mark as valid if we have a valid time
        d->valid = d->when.valid;

        result.count++;
    }

    return result;
}
