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

DeparturesResult parseDeparturesJson(const char* json, int maxResults) {
  DeparturesResult result = {{}, 0, false};

  if (json == NULL) {
    return result;
  }

  if (maxResults > MAX_DEPARTURES) {
    maxResults = MAX_DEPARTURES;
  }

  StaticJsonDocument<512> filter;
  filter["departureList"][0]["countdown"] = true;
  filter["departureList"][0]["dateTime"] = true;
  filter["departureList"][0]["realDateTime"] = true;
  filter["departureList"][0]["servingLine"]["direction"] = true;
  filter["departureList"][0]["servingLine"]["number"] = true;
  filter["departureList"][0]["servingLine"]["realtime"] = true;

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));

  if (error) {
    return result;
  }

  JsonArrayConst departures = doc["departureList"].as<JsonArrayConst>();
  if (departures.isNull()) {
    return result;
  }

  result.success = true;

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

  return result;
}
