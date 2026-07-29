//
// Created by wegam on 2023/1/22.
//

#include <gtest/gtest.h>
#include <cmath>
#include <fstream>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/vectors.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/file.hpp>

using namespace Dal;

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
