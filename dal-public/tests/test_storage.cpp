#include <gtest/gtest.h>

#include <iomanip>
#include <sstream>
#include <string>

#include <dal/indice/fixings.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal-public/src/storage.hpp>

using namespace Dal;

namespace {
    std::string LegacyFixingsPayload(int count, int badIndex = -1, const char* bad = "") {
        std::ostringstream payload;
        payload << R"({"~type":"Fixings","name":"legacy","fixing_times":[)";
        for (int i = 0; i < count; ++i) {
            payload << (i ? ",\"" : "\"");
            if (i == badIndex)
                payload << bad;
            else
                payload << "2026-02-14 11:" << std::setfill('0') << std::setw(2) << 22 + i << ":00";
            payload << '"';
        }
        payload << R"(],"fixings":[)";
        for (int i = 0; i < count; ++i)
            payload << (i ? "," : "") << i + 1;
        payload << "]}";
        return payload.str();
    }

    void AssertLegacyFixings(const Handle_<Fixings_>& restored, int count) {
        ASSERT_TRUE(restored);
        ASSERT_EQ(restored->Type(), "Fixings");
        ASSERT_EQ(restored->Name(), "legacy");
        ASSERT_EQ(restored->vals_.size(), static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            const DateTime_ time(Date_(2026, 2, 14), 11, 22 + i, 0);
            ASSERT_EQ(restored->vals_.at(time), i + 1);
            ASSERT_EQ(LookupFixing(restored->vals_, time), i + 1);
        }
    }
} // namespace

TEST(PublicStorageTest, TestReadObjectJsonLegacyFixingsDiagnosticsAndRetry) {
    const auto parseFixings = [](const std::string& payload) { return handle_cast<Fixings_>(ReadObjectJson(payload.data(), payload.size())); };
    for (const int count : {1, 11}) {
        const auto valid = LegacyFixingsPayload(count);
        const auto original = parseFixings(valid);
        ASSERT_NO_FATAL_FAILURE(AssertLegacyFixings(original, count));
        for (const char* bad : {"2026-02-14 11:xx:00", "2026-02-14 11:2147483648:00", "2026-02-14 25:00:00"}) {
            for (const int index : count == 1 ? Vector_<int>{0} : Vector_<int>{10, 0}) {
                SCOPED_TRACE(bad);
                SCOPED_TRACE(index);
                try {
                    parseFixings(LegacyFixingsPayload(count, index, bad));
                    FAIL() << "Malformed legacy archive accepted";
                } catch (const Exception_& error) {
                    const std::string message(error.what());
                    const std::string code = "ARCHIVE_DATETIME_INVALID:";
                    const std::string expected = code + " field=/fixing_times/" + std::to_string(index) + "; expected=";
                    const auto start = message.find(code);
                    ASSERT_NE(start, std::string::npos);
                    ASSERT_EQ(message.compare(start, expected.size(), expected), 0);
                }
                ASSERT_NO_FATAL_FAILURE(AssertLegacyFixings(original, count));
                ASSERT_NO_FATAL_FAILURE(AssertLegacyFixings(parseFixings(valid), count));
            }
        }
    }
}
