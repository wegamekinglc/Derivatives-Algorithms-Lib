#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <limits>
#include <sys/resource.h>

#include <rapidjson/document.h>

#include "bbs.hpp"

namespace {
    uint64_t bsCalls = 0, visitedNodes = 0;
    double Price(const PdeRiskReference::Input_& r, double spot, double dt) {
        ++bsCalls;
        return PdeRiskBbs::European(r, spot, dt);
    }
    void AssertErrorDetails(const PdeRiskReference::Input_& r, double spot, double dt) {
        try {
            static_cast<void>(Price(r, spot, dt));
            FAIL() << "invalid scalar input was admitted";
        } catch (const std::runtime_error& error) {
            const std::string message(error.what());
            std::cout << "BBS_ERROR " << message << '\n';
            for (const char* field :
                 {"BBS invalid European value", "call=", "spot=", "strike=", "vol=", "rate=", "yield=", "dt=", "sd=", "d1=", "d2=", "value="})
                ASSERT_NE(message.find(field), std::string::npos) << field << ": " << message;
        }
    }
} // namespace

TEST(BbsScalarTest, TestRejectedPutTailRetainsRepresentableSubnormalValue) {
    PdeRiskReference::Input_ r;
    r.carry_[0].yield_ = 0.0;
    const double expected = 17 * std::numeric_limits<double>::denorm_min();
    ASSERT_EQ(Price(r, 108.82078834416117, 0.00012205541315757354), expected);
}

TEST(BbsScalarTest, TestTailAndNormalInputsAgainstIndependentHighPrecision) {
    std::ifstream input(DAL502_BBS_CASES);
    ASSERT_TRUE(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    rapidjson::Document data;
    data.Parse<rapidjson::kParseFullPrecisionFlag>(text.c_str());
    ASSERT_FALSE(data.HasParseError());
    ASSERT_EQ(data["cases"].Size(), 41);
    for (const auto& row : data["cases"].GetArray()) {
        SCOPED_TRACE(row["name"].GetString());
        PdeRiskReference::Input_ r;
        r.call_ = row["call"].GetBool();
        r.strike_ = row["strike"].GetDouble();
        r.volatility_ = row["vol"].GetDouble();
        r.carry_[0].rate_ = row["rate"].GetDouble();
        r.carry_[0].yield_ = row["yield"].GetDouble();
        const double spot = row["spot"].GetDouble(), expected = row["rounded_reference"].GetDouble();
        const double actual = Price(r, spot, row["dt"].GetDouble());
        std::cout << "BBS_SCALAR " << row["name"].GetString() << ' ' << std::setprecision(17) << actual << '\n';
        ASSERT_TRUE(std::isfinite(actual));
        ASSERT_GE(actual, 0.0);
        if (expected == 0.0) {
            ASSERT_TRUE(row["below_half_min_subnormal"].GetBool());
            ASSERT_EQ(actual, 0.0);
        } else if (expected < std::numeric_limits<double>::min()) {
            ASSERT_LE(std::abs(actual - expected), std::numeric_limits<double>::denorm_min());
        } else if (std::string(row["name"].GetString()).find("normal-") == 0) {
            ASSERT_NEAR(actual, expected, 16 * std::numeric_limits<double>::epsilon() * std::max(spot, r.strike_));
        } else {
            ASSERT_NEAR(actual / expected, 1.0, 2e-10);
        }
    }
}

TEST(BbsScalarTest, TestInvalidArithmeticRetainsReproducibleErrorDetails) {
    PdeRiskReference::Input_ r;
    ASSERT_NO_FATAL_FAILURE(AssertErrorDetails(r, 100.0, 0.0));
    ASSERT_NO_FATAL_FAILURE(AssertErrorDetails(r, std::numeric_limits<double>::quiet_NaN(), 1.0));
    r.carry_[0].rate_ = -1000.0;
    ASSERT_NO_FATAL_FAILURE(AssertErrorDetails(r, 100.0, 1.0));
}

TEST(BbsScalarTest, TestOriginalFailurePrefixesCompleteWithoutRollback) {
    const int sizes[] = {8192, 8193, 16385}, limits[] = {4118, 4115, 8177};
    const double roots[] = {99.0, 100.5, 99.0};
    for (int i = 0; i < 3; ++i) {
        PdeRiskReference::Input_ r;
        r.call_ = i == 2;
        r.carry_[0].yield_ = r.call_ ? .08 : 0.0;
        const double dt = 1.0 / sizes[i], u = std::exp(r.volatility_ * std::sqrt(dt));
        for (int j = 0; j < limits[i]; ++j) {
            ++visitedNodes;
            const double spot = roots[i] * std::exp((2 * j - (sizes[i] - 1)) * std::log(u));
            const double value = Price(r, spot, dt);
            ASSERT_TRUE(std::isfinite(value)) << sizes[i] << ':' << j;
            ASSERT_GE(value, 0.0) << sizes[i] << ':' << j;
        }
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    const int status = RUN_ALL_TESTS();
    struct rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    std::cout << "{\"visited_nodes\":" << visitedNodes << ",\"bs_evaluations\":" << bsCalls << ",\"max_rss_kib\":" << usage.ru_maxrss << "}\n";
    return status;
}
