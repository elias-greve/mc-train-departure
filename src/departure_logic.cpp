#include "departure_logic.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  // Two keyword kinds, distinguished by a leading '!':
  //   "Alpha"   include: direction must contain this to match
  //   "!Alpha"  exclude: if direction contains this, it never matches
  // An exclude always wins. If the filter is made up only of excludes, the
  // default is to show everything that isn't excluded (e.g. "!Alpha").
  bool hasInclude = false;   // at least one positive keyword present
  bool hasExclude = false;   // at least one negative keyword present
  bool includeHit = false;   // direction matched some positive keyword
  bool excludeHit = false;   // direction matched some negative keyword
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

    // A leading '!' marks an exclusion keyword; the rest is the term to match.
    bool negate = (*start == '!');
    char* term = negate ? start + 1 : start;

    if (strlen(term) > 0) {
      bool contains = (strstr(direction, term) != NULL);
      if (negate) {
        hasExclude = true;
        if (contains) excludeHit = true;
      } else {
        hasInclude = true;
        if (contains) includeHit = true;
      }
    }

    // Move to next keyword
    if (comma != NULL) {
      start = comma + 1;
    } else {
      break;
    }
  }

  free(filterCopy);

  // Any exclusion match rejects outright.
  if (excludeHit) {
    return false;
  }
  // With positive keywords, require one to have matched. With only exclusions,
  // not being excluded is enough to show. A non-empty filter that yielded no
  // usable keyword at all (e.g. "," or "   ") matches nothing.
  if (hasInclude) {
    return includeHit;
  }
  return hasExclude;
}

// Helper: read an int from a JSON value that may be a string ("5") or a number (5).
static int readIntField(JsonVariantConst v) {
  if (v.isNull()) return 0;
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    return s ? atoi(s) : 0;
  }
  if (v.is<int>()) return v.as<int>();
  return 0;
}

// Build the field filter shared by every parse entry point. Only departureList
// fields we actually use are kept; everything else (servingLines metadata, the
// bulk of the ~167 KB response) is discarded as the JSON is read.
static void buildDepartureFilter(JsonDocument& filter) {
  filter["departureList"][0]["countdown"] = true;
  filter["departureList"][0]["dateTime"] = true;
  filter["departureList"][0]["realDateTime"] = true;
  filter["departureList"][0]["servingLine"]["direction"] = true;
  filter["departureList"][0]["servingLine"]["number"] = true;
  filter["departureList"][0]["servingLine"]["realtime"] = true;
}

// Map an ArduinoJson deserialization error onto our error codes. NoMemory is
// kept distinct from malformed JSON because it is not recoverable by retrying.
static ParseError mapDeserError(DeserializationError error) {
  return (error == DeserializationError::NoMemory) ? PARSE_ERR_NO_MEMORY : PARSE_ERR_INVALID_JSON;
}

// Extract departures from an already-deserialized document into result.
static void extractDepartures(const DynamicJsonDocument& doc, DeparturesResult& result, int maxResults) {
  JsonArrayConst departures = doc["departureList"].as<JsonArrayConst>();
  if (departures.isNull()) {
    result.error = PARSE_ERR_NO_LIST;
    return;
  }

  result.success = true;
  result.error = PARSE_OK;

  for (JsonObjectConst dep : departures) {
    if (result.count >= maxResults) break;

    Departure* d = &result.departures[result.count];

    // Direction
    const char* direction = dep["servingLine"]["direction"] | "";
    strncpy(d->direction, direction, MAX_DIRECTION_LEN - 1);
    d->direction[MAX_DIRECTION_LEN - 1] = '\0';

    // Scheduled time
    JsonVariantConst sched = dep["dateTime"];
    d->schedHour = readIntField(sched["hour"]);
    d->schedMinute = readIntField(sched["minute"]);

    // Real time (falls back to scheduled when absent).
    // Use an inner-field presence check because the JSON filter can leave an
    // empty object when EFA omits realDateTime entirely.
    d->realHour = d->schedHour;
    d->realMinute = d->schedMinute;
    if (dep["realDateTime"]["hour"].is<const char*>()) {
      JsonVariantConst real = dep["realDateTime"];
      d->realHour = readIntField(real["hour"]);
      d->realMinute = readIntField(real["minute"]);
    }

    // Delay in minutes with midnight wrap clamp
    int delayMin = (d->realHour * 60 + d->realMinute) - (d->schedHour * 60 + d->schedMinute);
    if (delayMin < -720) delayMin += 1440;
    if (delayMin > 720) delayMin -= 1440;
    d->delayMin = delayMin;

    // Countdown
    d->countdown = readIntField(dep["countdown"]);

    // Mark valid if we at least have a servingLine present (dateTime may
    // legitimately be all zeros for malformed entries; treat a missing
    // dateTime as invalid).
    d->valid = !sched.isNull();

    result.count++;
  }
}

// A full 15-entry filtered response uses ~9.9 KB of pool (each entry carries
// full dateTime + realDateTime objects). 16 KB leaves headroom for busy stops.
static const size_t kDocCapacity = 16384;

DeparturesResult parseDeparturesJson(const char* json, int maxResults) {
  DeparturesResult result = {{}, 0, false, PARSE_OK};

  if (json == NULL) {
    result.error = PARSE_ERR_NULL_INPUT;
    return result;
  }

  if (maxResults > MAX_DEPARTURES) {
    maxResults = MAX_DEPARTURES;
  }

  StaticJsonDocument<512> filter;
  buildDepartureFilter(filter);

  DynamicJsonDocument doc(kDocCapacity);
  DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  if (error) {
    result.error = mapDeserError(error);
    return result;
  }

  extractDepartures(doc, result, maxResults);
  return result;
}

#ifdef ARDUINO
DeparturesResult parseDeparturesJsonStream(Stream& stream, int maxResults) {
  DeparturesResult result = {{}, 0, false, PARSE_OK};

  if (maxResults > MAX_DEPARTURES) {
    maxResults = MAX_DEPARTURES;
  }

  StaticJsonDocument<512> filter;
  buildDepartureFilter(filter);

  // Deserialize straight from the socket: the filter drops everything outside
  // departureList as bytes arrive, so peak RAM is the filtered ~10 KB rather
  // than the full 167 KB body that http.getString() would have buffered.
  DynamicJsonDocument doc(kDocCapacity);
  DeserializationError error = deserializeJson(doc, stream, DeserializationOption::Filter(filter));
  if (error) {
    result.error = mapDeserError(error);
    return result;
  }

  extractDepartures(doc, result, maxResults);
  return result;
}
#endif  // ARDUINO
