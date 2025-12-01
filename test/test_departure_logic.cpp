#include <unity.h>
#include "../src/departure_logic.h"
#include <time.h>
#include <string.h>

// ============================================================================
// Tests for parseIso8601Time
// ============================================================================

void test_parseIso8601Time_valid_timestamp(void) {
    ParsedTime result = parseIso8601Time("2024-12-01T17:26:45");

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(2024, result.year);
    TEST_ASSERT_EQUAL_INT(12, result.month);
    TEST_ASSERT_EQUAL_INT(1, result.day);
    TEST_ASSERT_EQUAL_INT(17, result.hour);
    TEST_ASSERT_EQUAL_INT(26, result.minute);
    TEST_ASSERT_EQUAL_INT(45, result.second);
}

void test_parseIso8601Time_midnight(void) {
    ParsedTime result = parseIso8601Time("2024-01-15T00:00:00");

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(0, result.hour);
    TEST_ASSERT_EQUAL_INT(0, result.minute);
    TEST_ASSERT_EQUAL_INT(0, result.second);
}

void test_parseIso8601Time_end_of_day(void) {
    ParsedTime result = parseIso8601Time("2024-12-31T23:59:59");

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(23, result.hour);
    TEST_ASSERT_EQUAL_INT(59, result.minute);
    TEST_ASSERT_EQUAL_INT(59, result.second);
}

void test_parseIso8601Time_null_input(void) {
    ParsedTime result = parseIso8601Time(NULL);
    TEST_ASSERT_FALSE(result.valid);
}

void test_parseIso8601Time_empty_string(void) {
    ParsedTime result = parseIso8601Time("");
    TEST_ASSERT_FALSE(result.valid);
}

void test_parseIso8601Time_too_short(void) {
    ParsedTime result = parseIso8601Time("2024-12-01");
    TEST_ASSERT_FALSE(result.valid);
}

void test_parseIso8601Time_with_timezone(void) {
    // Should still parse the datetime part correctly
    ParsedTime result = parseIso8601Time("2024-12-01T17:26:45+01:00");

    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(17, result.hour);
    TEST_ASSERT_EQUAL_INT(26, result.minute);
}

void test_parseIso8601Time_malformed_values(void) {
    // Invalid month/day/hour values - sscanf will parse them but they're nonsense
    // The parser doesn't validate ranges, just format
    ParsedTime result = parseIso8601Time("2024-13-45T99:99:99");

    // sscanf succeeds in parsing 6 integers, so valid=true
    // This documents current behavior - ranges aren't validated
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(13, result.month);
    TEST_ASSERT_EQUAL_INT(99, result.hour);
}

// ============================================================================
// Tests for matchesDirectionFilter
// ============================================================================

void test_matchesDirectionFilter_empty_filter_matches_all(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Hauptbahnhof", ""));
    TEST_ASSERT_TRUE(matchesDirectionFilter("Any Direction", ""));
}

void test_matchesDirectionFilter_null_filter_matches_all(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Hauptbahnhof", NULL));
}

void test_matchesDirectionFilter_exact_match(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Hauptbahnhof", "Hauptbahnhof"));
}

void test_matchesDirectionFilter_partial_match(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Freiburg Hauptbahnhof", "Hauptbahnhof"));
    TEST_ASSERT_TRUE(matchesDirectionFilter("Hauptbahnhof Süd", "Hauptbahnhof"));
}

void test_matchesDirectionFilter_no_match(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter("Rieselfeld", "Hauptbahnhof"));
}

void test_matchesDirectionFilter_multiple_keywords_first_matches(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Hauptbahnhof", "Hauptbahnhof,Marktplatz"));
}

void test_matchesDirectionFilter_multiple_keywords_second_matches(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Marktplatz", "Hauptbahnhof,Marktplatz"));
}

void test_matchesDirectionFilter_multiple_keywords_none_match(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter("Rieselfeld", "Hauptbahnhof,Marktplatz"));
}

void test_matchesDirectionFilter_keywords_with_spaces(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Hauptbahnhof", " Hauptbahnhof , Marktplatz "));
}

void test_matchesDirectionFilter_null_direction(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter(NULL, "Hauptbahnhof"));
}

void test_matchesDirectionFilter_case_sensitive(void) {
    // The filter is case-sensitive
    TEST_ASSERT_FALSE(matchesDirectionFilter("hauptbahnhof", "Hauptbahnhof"));
}

void test_matchesDirectionFilter_trailing_comma(void) {
    // Trailing comma should be handled gracefully
    TEST_ASSERT_TRUE(matchesDirectionFilter("Hauptbahnhof", "Hauptbahnhof,"));
    TEST_ASSERT_TRUE(matchesDirectionFilter("Marktplatz", "Hauptbahnhof,Marktplatz,"));
}

void test_matchesDirectionFilter_only_commas(void) {
    // Only commas or empty keywords should not match
    TEST_ASSERT_FALSE(matchesDirectionFilter("Hauptbahnhof", ","));
    TEST_ASSERT_FALSE(matchesDirectionFilter("Hauptbahnhof", ",,"));
}

void test_matchesDirectionFilter_whitespace_only(void) {
    // Whitespace-only filter should not match anything
    TEST_ASSERT_FALSE(matchesDirectionFilter("Hauptbahnhof", "   "));
    TEST_ASSERT_FALSE(matchesDirectionFilter("Hauptbahnhof", " , "));
}

// ============================================================================
// Tests for calculatePlannedTime
// ============================================================================

void test_calculatePlannedTime_no_delay(void) {
    PlannedTime result = calculatePlannedTime(17, 30, 0, 0);

    TEST_ASSERT_EQUAL_INT(17, result.hour);
    TEST_ASSERT_EQUAL_INT(30, result.minute);
    TEST_ASSERT_EQUAL_INT(0, result.second);
}

void test_calculatePlannedTime_small_delay(void) {
    // 3 minutes delay
    PlannedTime result = calculatePlannedTime(17, 33, 0, 180);

    TEST_ASSERT_EQUAL_INT(17, result.hour);
    TEST_ASSERT_EQUAL_INT(30, result.minute);
    TEST_ASSERT_EQUAL_INT(0, result.second);
}

void test_calculatePlannedTime_delay_crosses_hour(void) {
    // 10 minutes delay, actual time 17:05, planned should be 16:55
    PlannedTime result = calculatePlannedTime(17, 5, 0, 600);

    TEST_ASSERT_EQUAL_INT(16, result.hour);
    TEST_ASSERT_EQUAL_INT(55, result.minute);
    TEST_ASSERT_EQUAL_INT(0, result.second);
}

void test_calculatePlannedTime_delay_crosses_midnight(void) {
    // 30 minutes delay, actual time 00:15, planned should be 23:45
    PlannedTime result = calculatePlannedTime(0, 15, 0, 1800);

    TEST_ASSERT_EQUAL_INT(23, result.hour);
    TEST_ASSERT_EQUAL_INT(45, result.minute);
}

void test_calculatePlannedTime_seconds_underflow(void) {
    // 90 seconds delay, actual time 17:30:30, planned should be 17:29:00
    PlannedTime result = calculatePlannedTime(17, 30, 30, 90);

    TEST_ASSERT_EQUAL_INT(17, result.hour);
    TEST_ASSERT_EQUAL_INT(29, result.minute);
    TEST_ASSERT_EQUAL_INT(0, result.second);
}

// ============================================================================
// Tests for delaySecondsToMinutes
// ============================================================================

void test_delaySecondsToMinutes_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, delaySecondsToMinutes(0));
}

void test_delaySecondsToMinutes_exact_minutes(void) {
    TEST_ASSERT_EQUAL_INT(5, delaySecondsToMinutes(300));
}

void test_delaySecondsToMinutes_rounds_down(void) {
    TEST_ASSERT_EQUAL_INT(2, delaySecondsToMinutes(179));  // 2:59 -> 2
}

void test_delaySecondsToMinutes_negative(void) {
    // Negative delay means early
    TEST_ASSERT_EQUAL_INT(-2, delaySecondsToMinutes(-120));
}

// ============================================================================
// Tests for calculateMinutesUntil
// ============================================================================

void test_calculateMinutesUntil_future_departure(void) {
    ParsedTime parsed = {2024, 12, 1, 17, 30, 0, true};

    // Create a "now" time at 17:20
    struct tm nowTm = {0};
    nowTm.tm_year = 2024 - 1900;
    nowTm.tm_mon = 12 - 1;
    nowTm.tm_mday = 1;
    nowTm.tm_hour = 17;
    nowTm.tm_min = 20;
    nowTm.tm_sec = 0;
    nowTm.tm_isdst = -1;
    time_t now = mktime(&nowTm);

    int minutes = calculateMinutesUntil(parsed, now);
    TEST_ASSERT_EQUAL_INT(10, minutes);
}

void test_calculateMinutesUntil_past_departure(void) {
    ParsedTime parsed = {2024, 12, 1, 17, 20, 0, true};

    // Create a "now" time at 17:30
    struct tm nowTm = {0};
    nowTm.tm_year = 2024 - 1900;
    nowTm.tm_mon = 12 - 1;
    nowTm.tm_mday = 1;
    nowTm.tm_hour = 17;
    nowTm.tm_min = 30;
    nowTm.tm_sec = 0;
    nowTm.tm_isdst = -1;
    time_t now = mktime(&nowTm);

    int minutes = calculateMinutesUntil(parsed, now);
    TEST_ASSERT_EQUAL_INT(-10, minutes);
}

void test_calculateMinutesUntil_invalid_parsed_time(void) {
    ParsedTime parsed = {0, 0, 0, 0, 0, 0, false};
    time_t now = time(NULL);

    int minutes = calculateMinutesUntil(parsed, now);
    TEST_ASSERT_EQUAL_INT(-1, minutes);
}

// ============================================================================
// Tests for parseDeparturesJson
// ============================================================================

void test_parseDeparturesJson_valid_response(void) {
    const char* json = R"({
        "departures": [
            {
                "direction": "Hauptbahnhof",
                "when": "2024-12-01T17:26:45+01:00",
                "delay": 180
            },
            {
                "direction": "Marktplatz",
                "when": "2024-12-01T17:34:00+01:00",
                "delay": 0
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(2, result.count);

    TEST_ASSERT_EQUAL_STRING("Hauptbahnhof", result.departures[0].direction);
    TEST_ASSERT_TRUE(result.departures[0].valid);
    TEST_ASSERT_EQUAL_INT(17, result.departures[0].when.hour);
    TEST_ASSERT_EQUAL_INT(26, result.departures[0].when.minute);
    TEST_ASSERT_EQUAL_INT(180, result.departures[0].delaySec);

    TEST_ASSERT_EQUAL_STRING("Marktplatz", result.departures[1].direction);
    TEST_ASSERT_EQUAL_INT(0, result.departures[1].delaySec);
}

void test_parseDeparturesJson_null_delay(void) {
    const char* json = R"({
        "departures": [
            {
                "direction": "Hauptbahnhof",
                "when": "2024-12-01T17:26:45+01:00",
                "delay": null
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(1, result.count);
    TEST_ASSERT_EQUAL_INT(0, result.departures[0].delaySec);
}

void test_parseDeparturesJson_missing_when(void) {
    const char* json = R"({
        "departures": [
            {
                "direction": "Hauptbahnhof",
                "delay": 180
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(1, result.count);
    TEST_ASSERT_FALSE(result.departures[0].valid);
}

void test_parseDeparturesJson_empty_departures(void) {
    const char* json = R"({
        "departures": []
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(0, result.count);
}

void test_parseDeparturesJson_no_departures_key(void) {
    const char* json = R"({
        "error": "not found"
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_FALSE(result.success);
}

void test_parseDeparturesJson_invalid_json(void) {
    const char* json = "not valid json at all";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_FALSE(result.success);
}

void test_parseDeparturesJson_null_input(void) {
    DeparturesResult result = parseDeparturesJson(NULL, 10);

    TEST_ASSERT_FALSE(result.success);
}

void test_parseDeparturesJson_max_results_limit(void) {
    const char* json = R"({
        "departures": [
            {"direction": "A", "when": "2024-12-01T17:00:00+01:00", "delay": 0},
            {"direction": "B", "when": "2024-12-01T17:10:00+01:00", "delay": 0},
            {"direction": "C", "when": "2024-12-01T17:20:00+01:00", "delay": 0},
            {"direction": "D", "when": "2024-12-01T17:30:00+01:00", "delay": 0},
            {"direction": "E", "when": "2024-12-01T17:40:00+01:00", "delay": 0}
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 3);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(3, result.count);
    TEST_ASSERT_EQUAL_STRING("A", result.departures[0].direction);
    TEST_ASSERT_EQUAL_STRING("C", result.departures[2].direction);
}

void test_parseDeparturesJson_long_direction_truncated(void) {
    const char* json = R"({
        "departures": [
            {
                "direction": "This is a very long direction name that exceeds the maximum allowed length for direction strings",
                "when": "2024-12-01T17:26:45+01:00",
                "delay": 0
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(1, result.count);
    // Should be truncated to MAX_DIRECTION_LEN - 1 characters
    TEST_ASSERT_EQUAL_INT(MAX_DIRECTION_LEN - 1, strlen(result.departures[0].direction));
}

void test_parseDeparturesJson_negative_delay(void) {
    // Negative delay means train is early
    const char* json = R"({
        "departures": [
            {
                "direction": "Hauptbahnhof",
                "when": "2024-12-01T17:26:45+01:00",
                "delay": -60
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(-60, result.departures[0].delaySec);
}

void test_parseDeparturesJson_realistic_api_response(void) {
    // Realistic response based on DB transport.rest API structure
    const char* json = R"({
        "departures": [
            {
                "tripId": "1|1234|0|80|1122024",
                "stop": {
                    "type": "stop",
                    "id": "8014418",
                    "name": "Freiburg(Breisgau) Hbf",
                    "location": {"type": "location", "latitude": 47.997, "longitude": 7.841}
                },
                "when": "2024-12-01T17:26:00+01:00",
                "plannedWhen": "2024-12-01T17:23:00+01:00",
                "delay": 180,
                "platform": "1",
                "plannedPlatform": "1",
                "direction": "Freiburg Messe/Universität",
                "provenance": null,
                "line": {
                    "type": "line",
                    "id": "4-vag-4",
                    "name": "STR 4",
                    "mode": "train",
                    "product": "tram"
                },
                "remarks": [],
                "currentTripPosition": {"type": "location", "latitude": 47.99, "longitude": 7.84}
            },
            {
                "tripId": "1|5678|0|80|1122024",
                "when": "2024-12-01T17:34:00+01:00",
                "delay": null,
                "direction": "Zähringen",
                "line": {"name": "STR 2"}
            },
            {
                "tripId": "1|9999|0|80|1122024",
                "when": null,
                "delay": 600,
                "direction": "Cancelled Train"
            }
        ],
        "realtimeDataUpdatedAt": 1701445560
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(3, result.count);

    // First departure: normal with delay
    TEST_ASSERT_EQUAL_STRING("Freiburg Messe/Universität", result.departures[0].direction);
    TEST_ASSERT_TRUE(result.departures[0].valid);
    TEST_ASSERT_EQUAL_INT(17, result.departures[0].when.hour);
    TEST_ASSERT_EQUAL_INT(26, result.departures[0].when.minute);
    TEST_ASSERT_EQUAL_INT(180, result.departures[0].delaySec);

    // Second departure: null delay (on time)
    TEST_ASSERT_EQUAL_STRING("Zähringen", result.departures[1].direction);
    TEST_ASSERT_TRUE(result.departures[1].valid);
    TEST_ASSERT_EQUAL_INT(0, result.departures[1].delaySec);

    // Third departure: null when (cancelled/unknown)
    TEST_ASSERT_EQUAL_STRING("Cancelled Train", result.departures[2].direction);
    TEST_ASSERT_FALSE(result.departures[2].valid);
}

void test_parseDeparturesJson_unicode_direction(void) {
    // German umlauts and special characters in direction
    const char* json = R"({
        "departures": [
            {
                "direction": "Günterstal über Lorettostraße",
                "when": "2024-12-01T17:26:45+01:00",
                "delay": 0
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("Günterstal über Lorettostraße", result.departures[0].direction);
}

void test_parseDeparturesJson_empty_direction(void) {
    const char* json = R"({
        "departures": [
            {
                "direction": "",
                "when": "2024-12-01T17:26:45+01:00",
                "delay": 0
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("", result.departures[0].direction);
    TEST_ASSERT_TRUE(result.departures[0].valid);
}

void test_parseDeparturesJson_missing_direction(void) {
    const char* json = R"({
        "departures": [
            {
                "when": "2024-12-01T17:26:45+01:00",
                "delay": 0
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(1, result.count);
    TEST_ASSERT_EQUAL_STRING("", result.departures[0].direction);
}

void test_parseDeparturesJson_large_delay(void) {
    // 2 hour delay (7200 seconds)
    const char* json = R"({
        "departures": [
            {
                "direction": "Hauptbahnhof",
                "when": "2024-12-01T19:30:00+01:00",
                "delay": 7200
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(7200, result.departures[0].delaySec);
}

void test_parseDeparturesJson_api_error_response(void) {
    // API returns error object instead of departures
    const char* json = R"({
        "message": "Internal Server Error",
        "url": "https://app.services-bahn.de/mob/bahnhofstafel/abfahrt"
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(0, result.count);
}

void test_parseDeparturesJson_departures_not_array(void) {
    // departures is an object instead of array
    const char* json = R"({
        "departures": {
            "error": "malformed"
        }
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_FALSE(result.success);
}

void test_parseDeparturesJson_extra_fields_ignored(void) {
    // Response with many extra fields that should be ignored
    const char* json = R"({
        "departures": [
            {
                "tripId": "1|12345|0|80|1122024",
                "stop": {"id": "123", "name": "Test"},
                "when": "2024-12-01T17:26:45+01:00",
                "plannedWhen": "2024-12-01T17:20:00+01:00",
                "delay": 405,
                "platform": "3a",
                "plannedPlatform": "3",
                "direction": "Endstation",
                "line": {"name": "S1", "mode": "train"},
                "remarks": [{"type": "warning", "text": "Delay"}],
                "loadFactor": "high",
                "cancelled": false
            }
        ],
        "realtimeDataUpdatedAt": 1701445560,
        "systemName": "DB Navigator"
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(1, result.count);
    TEST_ASSERT_EQUAL_STRING("Endstation", result.departures[0].direction);
    TEST_ASSERT_EQUAL_INT(405, result.departures[0].delaySec);
}

void test_parseDeparturesJson_explicit_null_when(void) {
    // when is explicitly null (different from missing)
    const char* json = R"({
        "departures": [
            {
                "direction": "Cancelled Service",
                "when": null,
                "delay": 0
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(1, result.count);
    TEST_ASSERT_EQUAL_STRING("Cancelled Service", result.departures[0].direction);
    TEST_ASSERT_FALSE(result.departures[0].valid);  // null when = invalid
}

// ============================================================================
// Test runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // parseIso8601Time tests
    RUN_TEST(test_parseIso8601Time_valid_timestamp);
    RUN_TEST(test_parseIso8601Time_midnight);
    RUN_TEST(test_parseIso8601Time_end_of_day);
    RUN_TEST(test_parseIso8601Time_null_input);
    RUN_TEST(test_parseIso8601Time_empty_string);
    RUN_TEST(test_parseIso8601Time_too_short);
    RUN_TEST(test_parseIso8601Time_with_timezone);
    RUN_TEST(test_parseIso8601Time_malformed_values);

    // matchesDirectionFilter tests
    RUN_TEST(test_matchesDirectionFilter_empty_filter_matches_all);
    RUN_TEST(test_matchesDirectionFilter_null_filter_matches_all);
    RUN_TEST(test_matchesDirectionFilter_exact_match);
    RUN_TEST(test_matchesDirectionFilter_partial_match);
    RUN_TEST(test_matchesDirectionFilter_no_match);
    RUN_TEST(test_matchesDirectionFilter_multiple_keywords_first_matches);
    RUN_TEST(test_matchesDirectionFilter_multiple_keywords_second_matches);
    RUN_TEST(test_matchesDirectionFilter_multiple_keywords_none_match);
    RUN_TEST(test_matchesDirectionFilter_keywords_with_spaces);
    RUN_TEST(test_matchesDirectionFilter_null_direction);
    RUN_TEST(test_matchesDirectionFilter_case_sensitive);
    RUN_TEST(test_matchesDirectionFilter_trailing_comma);
    RUN_TEST(test_matchesDirectionFilter_only_commas);
    RUN_TEST(test_matchesDirectionFilter_whitespace_only);

    // calculatePlannedTime tests
    RUN_TEST(test_calculatePlannedTime_no_delay);
    RUN_TEST(test_calculatePlannedTime_small_delay);
    RUN_TEST(test_calculatePlannedTime_delay_crosses_hour);
    RUN_TEST(test_calculatePlannedTime_delay_crosses_midnight);
    RUN_TEST(test_calculatePlannedTime_seconds_underflow);

    // delaySecondsToMinutes tests
    RUN_TEST(test_delaySecondsToMinutes_zero);
    RUN_TEST(test_delaySecondsToMinutes_exact_minutes);
    RUN_TEST(test_delaySecondsToMinutes_rounds_down);
    RUN_TEST(test_delaySecondsToMinutes_negative);

    // calculateMinutesUntil tests
    RUN_TEST(test_calculateMinutesUntil_future_departure);
    RUN_TEST(test_calculateMinutesUntil_past_departure);
    RUN_TEST(test_calculateMinutesUntil_invalid_parsed_time);

    // parseDeparturesJson tests
    RUN_TEST(test_parseDeparturesJson_valid_response);
    RUN_TEST(test_parseDeparturesJson_null_delay);
    RUN_TEST(test_parseDeparturesJson_missing_when);
    RUN_TEST(test_parseDeparturesJson_empty_departures);
    RUN_TEST(test_parseDeparturesJson_no_departures_key);
    RUN_TEST(test_parseDeparturesJson_invalid_json);
    RUN_TEST(test_parseDeparturesJson_null_input);
    RUN_TEST(test_parseDeparturesJson_max_results_limit);
    RUN_TEST(test_parseDeparturesJson_long_direction_truncated);
    RUN_TEST(test_parseDeparturesJson_negative_delay);
    RUN_TEST(test_parseDeparturesJson_realistic_api_response);
    RUN_TEST(test_parseDeparturesJson_unicode_direction);
    RUN_TEST(test_parseDeparturesJson_empty_direction);
    RUN_TEST(test_parseDeparturesJson_missing_direction);
    RUN_TEST(test_parseDeparturesJson_large_delay);
    RUN_TEST(test_parseDeparturesJson_api_error_response);
    RUN_TEST(test_parseDeparturesJson_departures_not_array);
    RUN_TEST(test_parseDeparturesJson_extra_fields_ignored);
    RUN_TEST(test_parseDeparturesJson_explicit_null_when);

    return UNITY_END();
}
