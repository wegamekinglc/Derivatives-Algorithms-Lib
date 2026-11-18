//
// Created by wegam on 2023/1/22.
//

#include <cmath>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/indice/fixings.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/box.hpp>
#include <dal/storage/json.hpp>
#include <dal/storage/legacyidentity_internal.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/file.hpp>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <string>

using namespace Dal;

namespace {
    String_ LegacyFixingsJson(int count, int invalidIndex = -1, bool emptyValues = false, bool duplicateLast = false) {
        std::ostringstream text;
        text << "{\"~type\":\"Fixings\",\"name\":\"perf-fixings\",\"fixing_times\":[";
        for (int i = 0; i < count; ++i) {
            const int day = duplicateLast && i == count - 1 ? 0 : i;
            text << (i ? "," : "") << '"'
                 << (i == invalidIndex ? String_("not-a-time") : Date::ToString(Date_(2025, 1, 1).AddDays(day)) + " 00:00:00") << '"';
        }
        text << "],\"fixings\":[";
        if (!emptyValues)
            for (int i = 0; i < count; ++i)
                text << (i ? "," : "") << 100. + i / 8.;
        text << "]}";
        return String_(text.str());
    }

    void AssertExactLegacyRetry(int count) {
        const auto restored = handle_cast<Fixings_>(JSON::ReadString(LegacyFixingsJson(count), false));
        ASSERT_EQ(restored->vals_.size(), static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            const DateTime_ time(Date_(2025, 1, 1).AddDays(i), 0.);
            ASSERT_EQ(restored->vals_.at(time), 100. + i / 8.);
            ASSERT_EQ(LookupFixing(restored->vals_, time), 100. + i / 8.);
        }
    }
} // namespace

TEST(StorageTest, TestJSONDateTimeLegacyIndexedErrorsAndExactRetry) {
    struct Failure_ {
        String_ input_, code_, field_;
    };
    for (int count : {1, 365}) {
        Vector_<Failure_> failures;
        for (int index : count == 1 ? Vector_<int>{0} : Vector_<int>{0, 9, 10, 364})
            failures.push_back({LegacyFixingsJson(count, index), "ARCHIVE_DATETIME_INVALID", "field=/fixing_times/" + String::FromInt(index)});
        failures.push_back(
            {LegacyFixingsJson(count, -1, true), "ARCHIVE_FIXINGS_LENGTH_MISMATCH", "fixing_times=" + String::FromInt(count) + "; fixings=0"});
        if (count == 365)
            failures.push_back({LegacyFixingsJson(count, -1, false, true), "ARCHIVE_FIXINGS_DUPLICATE_TIME", "field=/fixing_times/364"});
        for (const auto& failure : failures) {
            try {
                JSON::ReadString(failure.input_, false);
                FAIL() << "Malformed legacy archive accepted";
            } catch (const Exception_& error) {
                const String_ message(error.what());
                ASSERT_NE(message.find(failure.code_), String_::npos);
                ASSERT_NE(message.find(failure.field_), String_::npos);
            }
            ASSERT_NO_FATAL_FAILURE(AssertExactLegacyRetry(count));
        }
    }
}

TEST(StorageTest, TestJSONDateTimeKeepsFrozenLegacyIdentityBytes) {
    const DateTime_ time(Date_(2026, 2, 14), 0, 0, 11);
    const Fixings_ source("legacy", {{time, 1.}});
    ASSERT_EQ(LegacyIdentity::DateTimeString(time), "2026-02-14 00:00:10");
    ASSERT_EQ(LegacyIdentity::ObjectJson(source),
              "{\n\"~type\": \"Fixings\",\n\"name\": \"legacy\",\n\"fixings\": [1],\n\"fixing_times\": [\"2026-02-14 00:00:10\"]}");
    Matrix_<Cell_> cells(1, 1);
    cells(0, 0) = Cell_(time);
    ASSERT_EQ(LegacyIdentity::ObjectJson(Box_("legacy", cells)),
              "{\n\"~type\": \"Box\",\n\"name\": \"legacy\",\n\"contents\": { \"rows\": 1,\n\"cols\": 1,\n\"vals\": [\"2026-02-14 00:00:10\"]}}");
}

TEST(StorageTest, TestJSONDateTimePreservesExactFixingKeys) {
    const Date_ date(2026, 2, 14);
    Fixings_::vals_t values;
    double ordinal = 0.;
    for (double fraction : {0., std::numeric_limits<double>::denorm_min(), .1 / 86400., .2 / 86400.,
                            std::nextafter(.2 / 86400., 1.), std::nextafter(1., 0.)})
        values.emplace(DateTime_(date, fraction), ++ordinal);
    const Fixings_ source("subseconds", values);
    const auto encoded = JSON::WriteString(source);
    const auto restored = handle_cast<Fixings_>(JSON::ReadString(encoded, false));
    ASSERT_EQ(restored->vals_.size(), values.size());
    for (const auto& [time, value] : values) {
        ASSERT_EQ(restored->vals_.at(time), value);
        ASSERT_EQ(LookupFixing(restored->vals_, time), value);
    }
    ASSERT_EQ(JSON::WriteString(*restored), encoded);
    const Fixings_ negativeZero("subseconds", {{DateTime_(date, -0.), 1.}});
    ASSERT_EQ(JSON::WriteString(negativeZero), JSON::WriteString(Fixings_("subseconds", {{DateTime_(date, 0.), 1.}})));
    ASSERT_THROW(JSON::WriteString(Fixings_("invalid", {{DateTime_(), 1.}})), Exception_);
}

TEST(StorageTest, TestJSONDateTimeUsesTheSameProtocolForCellsAndMatrices) {
    const Date_ date(2026, 2, 14);
    const DateTime_ time(date, std::nextafter(.2 / 86400., 1.));
    const Vector_<Cell_> cells{Cell_(time), Cell_(date), Cell_(String_("ordinary")), Cell_(47123.5)};
    const Script::ScriptProductData_ product("cells", cells, {"PV=1", "PV=1", "PV=1", "PV=1"});
    const auto productCopy = handle_cast<Script::ScriptProductData_>(JSON::ReadString(JSON::WriteString(product), false));
    ASSERT_EQ(Cell::ToDateTime(productCopy->Dates()[0]), time);
    ASSERT_TRUE(Cell::IsDate(productCopy->Dates()[1]));
    ASSERT_TRUE(Cell::IsString(productCopy->Dates()[2]));
    ASSERT_TRUE(Cell::IsDouble(productCopy->Dates()[3]));
    Matrix_<Cell_> matrix(1, 4);
    for (int i = 0; i < 4; ++i)
        matrix(0, i) = cells[i];
    const Box_ box("matrix", matrix);
    const auto boxCopy = handle_cast<Box_>(JSON::ReadString(JSON::WriteString(box), false));
    ASSERT_EQ(Cell::ToDateTime(boxCopy->contents_(0, 0)), time);
    ASSERT_TRUE(Cell::IsDate(boxCopy->contents_(0, 1)));
    ASSERT_TRUE(Cell::IsString(boxCopy->contents_(0, 2)));
    ASSERT_TRUE(Cell::IsDouble(boxCopy->contents_(0, 3)));
}

TEST(StorageTest, TestJSONDateTimeReadsLegacyValuesAndRejectsMalformedArchives) {
    const auto decode = [](const String_& times, const String_& values = "[1]") {
        return handle_cast<Fixings_>(JSON::ReadString(
            "{\"~type\":\"Fixings\",\"name\":\"legacy\",\"fixing_times\":" + times + ",\"fixings\":" + values + "}", false));
    };
    for (const auto& [text, expected] : Vector_<std::pair<String_, DateTime_>>{
             {"[\"2026-02-14\"]", DateTime_(Date_(2026, 2, 14), 0.)},
             {"[\"2026-02-14 11:22\"]", DateTime_(Date_(2026, 2, 14), 11, 22)},
             {"[\"2026-02-14 00:00:10\"]", DateTime_(Date_(2026, 2, 14), 0, 0, 10)},
             {"[46067]", DateTime_(Date::FromExcel(46067), 0.)},
             {"[46067.25]", DateTime_(Date::FromExcel(46067), .25)}})
        ASSERT_EQ(decode(text)->vals_.begin()->first, expected);
    const Vector_<std::pair<String_, String_>> bad{
        {"[{\"~datetime\":\"dal.datetime/2\",\"date\":\"2026-02-14\",\"fraction\":0}]", "ARCHIVE_DATETIME_INVALID"},
        {"[{\"~datetime\":\"dal.datetime/1\",\"date\":\"2026-02-30\",\"fraction\":0}]", "ARCHIVE_DATETIME_INVALID"},
        {"[{\"~datetime\":\"dal.datetime/1\",\"date\":\"2026-02-14\",\"fraction\":1}]", "ARCHIVE_DATETIME_INVALID"},
        {"[{\"~datetime\":\"dal.datetime/1\",\"date\":\"2026-02-14\",\"fraction\":-0.1}]", "ARCHIVE_DATETIME_INVALID"},
        {"[{\"~datetime\":\"dal.datetime/1\",\"date\":\"2026-02-14\",\"fraction\":\"0\"}]", "ARCHIVE_DATETIME_INVALID"},
        {"[{\"~datetime\":\"dal.datetime/1\",\"date\":\"2026-02-14\"}]", "ARCHIVE_DATETIME_INVALID"},
        {"[{\"~datetime\":\"dal.datetime/1\",\"date\":\"2026-02-14\",\"fraction\":0,\"fraction\":0}]", "ARCHIVE_DATETIME_INVALID"},
        {"[{\"~datetime\":\"dal.datetime/1\",\"date\":\"2026-02-14\",\"fraction\":0,\"other\":0}]", "ARCHIVE_DATETIME_INVALID"},
        {"[1e100]", "ARCHIVE_DATETIME_INVALID"},
        {"[\"2026-02-14 00:00:10\",\"2026-02-14 00:00:10\"]", "ARCHIVE_FIXINGS_DUPLICATE_TIME"},
    };
    for (const auto& [times, error] : bad) {
        try {
            decode(times, times.find("10\",\"") == String_::npos ? "[1]" : "[1,2]");
            FAIL() << "invalid datetime archive accepted";
        } catch (const Exception_& caught) {
            ASSERT_NE(String_(caught.what()).find(error), String_::npos);
            ASSERT_NE(String_(caught.what()).find("fixing_times/"), String_::npos);
        }
    }
    try {
        decode("[]", "[1]");
        FAIL() << "unequal fixing arrays accepted";
    } catch (const Exception_& error) {
        const String_ message(error.what());
        ASSERT_NE(message.find("ARCHIVE_FIXINGS_LENGTH_MISMATCH"), String_::npos);
        ASSERT_NE(message.find("fixing_times=0; fixings=1"), String_::npos);
    }
}

TEST(StorageTest, TestJSONStore) {
    Vector_<> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    auto dst = JSON::WriteString(*src);
    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));
    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
}

TEST(StorageTest, TestJSONStoreFile) {
    Vector_<> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    JSON::WriteFile(*src, "src.json");
    Handle_<Storable_> rtn = JSON::ReadFile("src.json", true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));
    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
    File::Remove("src.json");
}

TEST(StorageTest, TestJSONReadStringMalformedThrows) {
    ASSERT_THROW(JSON::ReadString(String_("{\"~type\": "), true), Dal::Exception_);
    ASSERT_THROW(JSON::ReadString(String_("not json at all"), true), Dal::Exception_);
    ASSERT_THROW(JSON::ReadString(String_(""), true), Dal::Exception_);
}

TEST(StorageTest, TestJSONReadFileMalformedThrows) {
    {
        std::ofstream bad("bad.json");
        bad << "{\"~type\": \"Interp1Linear_\", \"vals\": [";
    }
    ASSERT_THROW(JSON::ReadFile("bad.json", true), Dal::Exception_);
    File::Remove("bad.json");
}

TEST(StorageTest, TestJSONReadFileMissingThrows) {
    ASSERT_THROW(JSON::ReadFile("no_such_file_exists.json", true), Dal::Exception_);
}

TEST(StorageTest, TestJSONReadStringUsesTheExactByteRange) {
    Vector_<> x = {1.0, 2.0};
    Vector_<> f = {2.5, 3.5};
    const Handle_<Interp1_> source(Interp::NewLinear("exact-range", x, f));
    const String_ valid = JSON::WriteString(*source);
    std::string withNul(valid.begin(), valid.end());
    withNul.push_back('\0');
    withNul.append("{}");
    JSONReadOptions_ options;

    ASSERT_THROW(JSON::ReadString(withNul.data(), withNul.size(), options), Dal::Exception_);
    ASSERT_THROW(JSON::ReadString(valid.data(), valid.size() - 1, options), Dal::Exception_);
    ASSERT_NO_THROW(JSON::ReadString(valid.data(), valid.size(), options));
}

TEST(StorageTest, TestJSONReadStringRejectsTrailingBytesAndInvalidUtf8) {
    JSONReadOptions_ options;
    const std::string trailing = "{\"~type\":\"Bag\",\"$tag\":\"1\"}{}";
    const std::string invalidUtf8 = "{\"~type\":\"Bag\",\"name\":\"\xC3\"}";

    ASSERT_THROW(JSON::ReadString(trailing.data(), trailing.size(), options), Dal::Exception_);
    ASSERT_THROW(JSON::ReadString(invalidUtf8.data(), invalidUtf8.size(), options), Dal::Exception_);
}

TEST(StorageTest, TestJSONReadStringRejectsDecodedNulAndInvalidUnicode) {
    const String_ escapedNul(
        R"({"~type":"DiscountPWC_v1","name":"bad\u0000name","ccy":"USD","knotDates":["2026-02-14"],"rightVals":[0.01]})");
    const String_ escapedNulKey(
        R"({"~type":"DiscountPWC_v1","bad\u0000key":"value","name":"curve","ccy":"USD","knotDates":["2026-02-14"],"rightVals":[0.01]})");
    const String_ loneHighSurrogate(
        R"({"~type":"DiscountPWC_v1","name":"bad\uD800name","ccy":"USD","knotDates":["2026-02-14"],"rightVals":[0.01]})");
    const String_ loneLowSurrogate(
        R"({"~type":"DiscountPWC_v1","name":"bad\uDC00name","ccy":"USD","knotDates":["2026-02-14"],"rightVals":[0.01]})");

    for (const auto& [payload, expectedError] :
         std::vector<std::pair<String_, String_>>{{escapedNul, "ARCHIVE_STRING_NUL"},
                                                  {escapedNulKey, "ARCHIVE_STRING_NUL"},
                                                  {loneHighSurrogate, "ARCHIVE_STRING_INVALID_UNICODE"},
                                                  {loneLowSurrogate, "ARCHIVE_STRING_INVALID_UNICODE"}}) {
        try {
            JSON::ReadString(payload, true);
            FAIL() << "unsafe decoded JSON string was accepted";
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(String_(error.what()).find(expectedError), String_::npos);
        }
    }
}

TEST(StorageTest, TestJSONReadStringPreservesSupplementaryUnicodeAcrossReserialization) {
    const String_ escaped(
        R"({"~type":"DiscountPWC_v1","name":"curve-\uD83D\uDE80","ccy":"USD","knotDates":["2026-02-14"],"rightVals":[0.01]})");
    const Handle_<Storable_> restored = JSON::ReadString(escaped, true);
    const String_ serialized = JSON::WriteString(*restored);
    const Handle_<Storable_> roundTrip = JSON::ReadString(serialized, true);

    ASSERT_EQ(restored->Name(), String_(std::string("curve-\xF0\x9F\x9A\x80")));
    ASSERT_EQ(roundTrip->Name(), restored->Name());
    ASSERT_EQ(JSON::WriteString(*roundTrip), serialized);
}

TEST(StorageTest, TestJSONWriterEscapesStringsAndUsesRoundTripDoubles) {
    Vector_<> x = {1.0, 2.0};
    Vector_<> f = {std::nextafter(1.0, 2.0), -0.0};
    const String_ name(std::string("quoted\" slash\\ tab\t newline\n utf8-\xE4\xB8\xAD"));
    const Handle_<Interp1_> source(Interp::NewLinear(name, x, f));

    const String_ first = JSON::WriteString(*source);
    const Handle_<Storable_> restored = JSON::ReadString(first, true);
    const String_ second = JSON::WriteString(*restored);

    ASSERT_EQ(first, second);
    ASSERT_NE(first.find("\\\""), String_::npos);
    ASSERT_NE(first.find("\\\\"), String_::npos);
    ASSERT_NE(first.find("\\t"), String_::npos);
    ASSERT_NE(first.find("\\n"), String_::npos);
    ASSERT_NE(first.find("1.0000000000000002"), String_::npos);
    ASSERT_EQ(first.find("-0"), String_::npos);
    ASSERT_EQ(restored->Name(), name);
}

TEST(StorageTest, TestJSONReadsIntegralDoublesBeyondIntRange) {
    Vector_<> x = {1.0, 3.0e9};
    Vector_<> f = {2.5, 5.0e12};
    const Handle_<Interp1_> source(Interp::NewLinear("large", x, f));

    const String_ written = JSON::WriteString(*source);
    ASSERT_NE(written.find("3000000000"), String_::npos);
    const Handle_<Interp1_> restored(std::dynamic_pointer_cast<const Interp1_>(JSON::ReadString(written, true)));
    ASSERT_TRUE(restored.get() != nullptr);
    ASSERT_DOUBLE_EQ((*restored)(3.0e9), 5.0e12);
}

TEST(StorageTest, TestDiscountPWCRoundTripsWithRecursiveBase) {
    const Vector_<Date_> knots{Date_(2026, 2, 15), Date_(2026, 7, 15), Date_(2027, 1, 15)};
    const Handle_<DiscountCurve_> base(NewDiscountPWC(
        "base", "USD", PiecewiseConstant_(knots, Vector_<>{0.01, 0.012, 0.014})));
    const Handle_<DiscountCurve_> spread(NewDiscountPWC(
        "spread", "USD", PiecewiseConstant_(knots, Vector_<>{0.001, 0.002, 0.003}), base));

    const String_ first = JSON::WriteString(*spread);
    const Handle_<Storable_> restored = JSON::ReadString(first, true);
    const auto curve = std::dynamic_pointer_cast<const Tape::DiscountPWC_<double>>(restored);

    ASSERT_TRUE(curve);
    ASSERT_TRUE(curve->Base());
    ASSERT_EQ(curve->KnotDates(), knots);
    ASSERT_EQ(curve->FRight(), (Vector_<>{0.001, 0.002, 0.003}));
    ASSERT_DOUBLE_EQ((*curve)(Date_(2026, 1, 15), Date_(2026, 10, 15)),
                     (*spread)(Date_(2026, 1, 15), Date_(2026, 10, 15)));
    ASSERT_EQ(JSON::WriteString(*curve), first);
}
