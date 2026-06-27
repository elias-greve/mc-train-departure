#include <unity.h>
#include "../src/departure_logic.h"
#include <string.h>
#include <string>

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
    TEST_ASSERT_TRUE(matchesDirectionFilter("Alpha", "Alpha"));
}

void test_matchesDirectionFilter_partial_match(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Test City Alpha", "Alpha"));
    TEST_ASSERT_TRUE(matchesDirectionFilter("Alpha Street", "Alpha"));
}

void test_matchesDirectionFilter_no_match(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter("Beta", "Alpha"));
}

void test_matchesDirectionFilter_multiple_keywords_first_matches(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Alpha", "Alpha,Gamma"));
}

void test_matchesDirectionFilter_multiple_keywords_second_matches(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Gamma", "Alpha,Gamma"));
}

void test_matchesDirectionFilter_multiple_keywords_none_match(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter("Beta", "Alpha,Gamma"));
}

void test_matchesDirectionFilter_keywords_with_spaces(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Alpha", " Alpha , Gamma "));
}

void test_matchesDirectionFilter_null_direction(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter(NULL, "Alpha"));
}

void test_matchesDirectionFilter_case_sensitive(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter("alpha", "Alpha"));
}

void test_matchesDirectionFilter_trailing_comma(void) {
    TEST_ASSERT_TRUE(matchesDirectionFilter("Alpha", "Alpha,"));
    TEST_ASSERT_TRUE(matchesDirectionFilter("Gamma", "Alpha,Gamma,"));
}

void test_matchesDirectionFilter_only_commas(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter("Alpha", ","));
    TEST_ASSERT_FALSE(matchesDirectionFilter("Alpha", ",,"));
}

void test_matchesDirectionFilter_whitespace_only(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter("Alpha", "   "));
    TEST_ASSERT_FALSE(matchesDirectionFilter("Alpha", " , "));
}

void test_matchesDirectionFilter_exclude_blocks_match(void) {
    // "!Alpha" shows everything except directions containing "Alpha"
    TEST_ASSERT_FALSE(matchesDirectionFilter("Alpha", "!Alpha"));
    TEST_ASSERT_TRUE(matchesDirectionFilter("Beta", "!Alpha"));
}

void test_matchesDirectionFilter_exclude_substring(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter("Test City Alpha Station", "!Alpha"));
}

void test_matchesDirectionFilter_multiple_excludes(void) {
    TEST_ASSERT_FALSE(matchesDirectionFilter("Alpha", "!Alpha,!Gamma"));
    TEST_ASSERT_FALSE(matchesDirectionFilter("Gamma", "!Alpha,!Gamma"));
    TEST_ASSERT_TRUE(matchesDirectionFilter("Beta", "!Alpha,!Gamma"));
}

void test_matchesDirectionFilter_exclude_wins_over_include(void) {
    // An exclusion always rejects, even if an include would otherwise match.
    TEST_ASSERT_FALSE(matchesDirectionFilter("Alpha Beta", "Beta,!Alpha"));
    // Include without the excluded term still matches.
    TEST_ASSERT_TRUE(matchesDirectionFilter("Beta", "Beta,!Alpha"));
}

// ============================================================================
// Tests for parseDeparturesJson (VAG EFA shape)
// ============================================================================

void test_parseDeparturesJson_valid_response(void) {
    const char* json = R"({
        "departureList": [
            {
                "countdown": "4",
                "dateTime": { "year": "2026", "month": "4", "day": "15", "hour": "17", "minute": "26" },
                "realDateTime": { "year": "2026", "month": "4", "day": "15", "hour": "17", "minute": "29" },
                "servingLine": { "number": "99", "direction": "Alpha", "realtime": "1", "name": "Tram" }
            },
            {
                "countdown": "12",
                "dateTime": { "year": "2026", "month": "4", "day": "15", "hour": "17", "minute": "34" },
                "servingLine": { "number": "99", "direction": "Beta", "realtime": "0", "name": "Tram" }
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(2, result.count);

    TEST_ASSERT_EQUAL_STRING("Alpha", result.departures[0].direction);
    TEST_ASSERT_TRUE(result.departures[0].valid);
    TEST_ASSERT_EQUAL_INT(17, result.departures[0].schedHour);
    TEST_ASSERT_EQUAL_INT(26, result.departures[0].schedMinute);
    TEST_ASSERT_EQUAL_INT(17, result.departures[0].realHour);
    TEST_ASSERT_EQUAL_INT(29, result.departures[0].realMinute);
    TEST_ASSERT_EQUAL_INT(3, result.departures[0].delayMin);
    TEST_ASSERT_EQUAL_INT(4, result.departures[0].countdown);

    // Second: no realDateTime -> real falls back to scheduled, delay 0
    TEST_ASSERT_EQUAL_STRING("Beta", result.departures[1].direction);
    TEST_ASSERT_EQUAL_INT(17, result.departures[1].schedHour);
    TEST_ASSERT_EQUAL_INT(34, result.departures[1].schedMinute);
    TEST_ASSERT_EQUAL_INT(17, result.departures[1].realHour);
    TEST_ASSERT_EQUAL_INT(34, result.departures[1].realMinute);
    TEST_ASSERT_EQUAL_INT(0, result.departures[1].delayMin);
    TEST_ASSERT_EQUAL_INT(12, result.departures[1].countdown);
}

void test_parseDeparturesJson_midnight_wrap_delay(void) {
    // Scheduled 23:58, real 00:03 next day -> +5 min
    const char* json = R"({
        "departureList": [
            {
                "countdown": "5",
                "dateTime": { "hour": "23", "minute": "58" },
                "realDateTime": { "hour": "0", "minute": "3" },
                "servingLine": { "direction": "Alpha", "number": "99", "realtime": "1" }
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(1, result.count);
    TEST_ASSERT_EQUAL_INT(5, result.departures[0].delayMin);
}

void test_parseDeparturesJson_empty_list(void) {
    const char* json = R"({ "departureList": [] })";
    DeparturesResult result = parseDeparturesJson(json, 10);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(0, result.count);
}

void test_parseDeparturesJson_no_departure_list_key(void) {
    const char* json = R"({ "error": "not found" })";
    DeparturesResult result = parseDeparturesJson(json, 10);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(PARSE_ERR_NO_LIST, result.error);
}

void test_parseDeparturesJson_invalid_json(void) {
    DeparturesResult result = parseDeparturesJson("not valid json at all", 10);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(PARSE_ERR_INVALID_JSON, result.error);
}

void test_parseDeparturesJson_null_input(void) {
    DeparturesResult result = parseDeparturesJson(NULL, 10);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(PARSE_ERR_NULL_INPUT, result.error);
}

void test_parseDeparturesJson_success_sets_ok(void) {
    const char* json = R"({ "departureList": [] })";
    DeparturesResult result = parseDeparturesJson(json, 10);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(PARSE_OK, result.error);
}

void test_parseDeparturesJson_no_memory(void) {
    // Build a well-formed response large enough to overflow the document pool.
    // Each entry carries a long direction string; ~400 entries far exceeds the
    // 16 KB pool, so deserialization must report NoMemory, not malformed JSON.
    std::string json = "{ \"departureList\": [";
    for (int i = 0; i < 400; i++) {
        if (i > 0) json += ",";
        json +=
            "{ \"countdown\": \"5\","
            "\"dateTime\": { \"hour\": \"17\", \"minute\": \"26\" },"
            "\"realDateTime\": { \"hour\": \"17\", \"minute\": \"29\" },"
            "\"servingLine\": { \"direction\": "
            "\"Very long direction name used to inflate the payload well past the pool size\","
            "\"number\": \"3\", \"realtime\": \"1\" } }";
    }
    json += "] }";

    DeparturesResult result = parseDeparturesJson(json.c_str(), 10);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(PARSE_ERR_NO_MEMORY, result.error);
}

void test_parseDeparturesJson_max_results_limit(void) {
    const char* json = R"({
        "departureList": [
            {"countdown":"2","dateTime":{"hour":"17","minute":"00"},"servingLine":{"direction":"A","number":"99","realtime":"0"}},
            {"countdown":"4","dateTime":{"hour":"17","minute":"10"},"servingLine":{"direction":"B","number":"99","realtime":"0"}},
            {"countdown":"6","dateTime":{"hour":"17","minute":"20"},"servingLine":{"direction":"C","number":"99","realtime":"0"}},
            {"countdown":"8","dateTime":{"hour":"17","minute":"30"},"servingLine":{"direction":"D","number":"99","realtime":"0"}},
            {"countdown":"10","dateTime":{"hour":"17","minute":"40"},"servingLine":{"direction":"E","number":"99","realtime":"0"}}
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 3);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(3, result.count);
    TEST_ASSERT_EQUAL_STRING("A", result.departures[0].direction);
    TEST_ASSERT_EQUAL_STRING("C", result.departures[2].direction);
}

void test_parseDeparturesJson_unicode_direction(void) {
    const char* json = R"({
        "departureList": [
            {
                "countdown": "3",
                "dateTime": { "hour": "17", "minute": "26" },
                "servingLine": { "direction": "Alpha via Delta", "number": "99", "realtime": "0" }
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("Alpha via Delta", result.departures[0].direction);
}

void test_parseDeparturesJson_long_direction_truncated(void) {
    const char* json = R"({
        "departureList": [
            {
                "countdown": "3",
                "dateTime": { "hour": "17", "minute": "26" },
                "servingLine": {
                    "direction": "This is a very long direction name that exceeds the maximum allowed length for direction strings",
                    "number": "99",
                    "realtime": "0"
                }
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(MAX_DIRECTION_LEN - 1, (int)strlen(result.departures[0].direction));
}

void test_parseDeparturesJson_missing_direction(void) {
    const char* json = R"({
        "departureList": [
            {
                "countdown": "3",
                "dateTime": { "hour": "17", "minute": "26" },
                "servingLine": { "number": "99", "realtime": "0" }
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(1, result.count);
    TEST_ASSERT_EQUAL_STRING("", result.departures[0].direction);
}

void test_parseDeparturesJson_realtime_equals_scheduled_no_delay(void) {
    // realDateTime == dateTime -> delay 0
    const char* json = R"({
        "departureList": [
            {
                "countdown": "7",
                "dateTime": { "hour": "17", "minute": "26" },
                "realDateTime": { "hour": "17", "minute": "26" },
                "servingLine": { "direction": "Alpha", "number": "99", "realtime": "1" }
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(0, result.departures[0].delayMin);
    TEST_ASSERT_EQUAL_INT(7, result.departures[0].countdown);
}

void test_parseDeparturesJson_realistic_efa_response(void) {
    // Response shape mirroring real VAG EFA output, including extra fields
    // that should be filtered out.
    const char* json = R"({
        "parameters": [{"name":"language","value":"de"}],
        "dm": { "itdDateTime": { "itdDate": {"year":"2026","month":"4","day":"15"} } },
        "departureList": [
            {
                "stopName": "Test City, Test Stop",
                "stopID": "1234567",
                "countdown": "4",
                "dateTime": { "year":"2026","month":"4","day":"15","hour":"17","minute":"26" },
                "realDateTime": { "year":"2026","month":"4","day":"15","hour":"17","minute":"29" },
                "servingLine": {
                    "key":"99","code":"99","number":"99","symbol":"99",
                    "direction":"Test City, Alpha Street",
                    "realtime":"1","name":"Tram"
                }
            },
            {
                "stopName": "Test City, Test Stop",
                "countdown": "1",
                "dateTime": { "hour":"17","minute":"30" },
                "servingLine": { "number":"99","direction":"Test City, Beta","realtime":"0","name":"Tram" }
            }
        ]
    })";

    DeparturesResult result = parseDeparturesJson(json, 10);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_INT(2, result.count);

    TEST_ASSERT_EQUAL_INT(4, result.departures[0].countdown);
    TEST_ASSERT_EQUAL_INT(3, result.departures[0].delayMin);
    TEST_ASSERT_TRUE(matchesDirectionFilter(result.departures[0].direction, "Alpha"));
    TEST_ASSERT_FALSE(matchesDirectionFilter(result.departures[1].direction, "Alpha"));
}

// ============================================================================
// Test runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

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
    RUN_TEST(test_matchesDirectionFilter_exclude_blocks_match);
    RUN_TEST(test_matchesDirectionFilter_exclude_substring);
    RUN_TEST(test_matchesDirectionFilter_multiple_excludes);
    RUN_TEST(test_matchesDirectionFilter_exclude_wins_over_include);

    // parseDeparturesJson (EFA) tests
    RUN_TEST(test_parseDeparturesJson_valid_response);
    RUN_TEST(test_parseDeparturesJson_midnight_wrap_delay);
    RUN_TEST(test_parseDeparturesJson_empty_list);
    RUN_TEST(test_parseDeparturesJson_no_departure_list_key);
    RUN_TEST(test_parseDeparturesJson_invalid_json);
    RUN_TEST(test_parseDeparturesJson_null_input);
    RUN_TEST(test_parseDeparturesJson_success_sets_ok);
    RUN_TEST(test_parseDeparturesJson_no_memory);
    RUN_TEST(test_parseDeparturesJson_max_results_limit);
    RUN_TEST(test_parseDeparturesJson_unicode_direction);
    RUN_TEST(test_parseDeparturesJson_long_direction_truncated);
    RUN_TEST(test_parseDeparturesJson_missing_direction);
    RUN_TEST(test_parseDeparturesJson_realtime_equals_scheduled_no_delay);
    RUN_TEST(test_parseDeparturesJson_realistic_efa_response);

    return UNITY_END();
}
