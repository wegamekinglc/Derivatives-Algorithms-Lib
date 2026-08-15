//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::Handle_;
using Dal::String_;
using Dal::Vector_;

namespace {
    // A model data type outside the store supported by ValueByMonteCarlo
    class DummyModelData_ : public Dal::ModelData_ {
    public:
        DummyModelData_() : Dal::ModelData_("DummyModelData_", "dummy") {}
        void Write(Dal::Archive::Store_& dst) const override {}

    private:
        std::unique_ptr<Dal::ModelData_> MutantModel(const String_* newName, const Dal::Slide_* slide) const override {
            return std::make_unique<DummyModelData_>();
        }
    };

    // 1y European call, strike 100, paying at 2023/9/25 (exactly ACT/365F = 1y after 2022/9/25)
    Handle_<Dal::ScriptProductData_> MakeEuropeanCall(const char* name) {
        const Vector_<Cell_> dates = {Cell_("STRIKE"), Cell_(Date_(2023, 9, 25))};
        const Vector_<String_> events = {String_("100.0"), String_("call pays MAX(spot() - STRIKE, 0.0)")};
        return Dal::NewScriptProduct(String_(name), dates, events);
    }

    Handle_<Dal::ModelData_> MakeBSModel(const char* name) {
        return Dal::NewBSModelData(String_(name), 100.0, 0.2, 0.05, 0.02);
    }

    double NormalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

    double NormalPdf(double x) { return std::exp(-0.5 * x * x) / std::sqrt(8.0 * std::atan(1.0)); }

    // spot=100, strike=100, vol=0.2, rate=0.05, div=0.02, mat=1
    void BlackScholesReference(double* call, double* delta, double* vega) {
        const double spot = 100.0, strike = 100.0, vol = 0.2, rate = 0.05, div = 0.02, mat = 1.0;
        const double fwd = spot * std::exp((rate - div) * mat);
        const double stdDev = vol * std::sqrt(mat);
        const double d1 = (std::log(fwd / strike) + 0.5 * stdDev * stdDev) / stdDev;
        const double d2 = d1 - stdDev;
        *call = std::exp(-rate * mat) * (fwd * NormalCdf(d1) - strike * NormalCdf(d2));
        *delta = std::exp(-div * mat) * NormalCdf(d1);
        *vega = spot * std::exp(-div * mat) * NormalPdf(d1) * std::sqrt(mat);
    }

    class ScopedEvaluationDate_ {
        Date_ previous_;

    public:
        explicit ScopedEvaluationDate_(const Date_& d) : previous_(Dal::GetEvaluationDate()) { Dal::SetEvaluationDate(d); }
        ~ScopedEvaluationDate_() { Dal::SetEvaluationDate(previous_); }
    };
} // namespace

TEST(ValueTest, TestEuropeanCallMatchesBlackScholes) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_bs_call");
    const auto model = MakeBSModel("value_bs_call_model");

    const auto result = Dal::ValueByMonteCarlo(product, model, 65536);

    double call, delta, vega;
    BlackScholesReference(&call, &delta, &vega);
    ASSERT_NEAR(result.at(String_("PV")), call, 0.05);
}

TEST(ValueTest, TestDeterministicAcrossRuns) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_deterministic");
    const auto model = MakeBSModel("value_deterministic_model");

    {
        const auto first = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"));
        const auto second = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"));
        ASSERT_DOUBLE_EQ(first.at(String_("PV")), second.at(String_("PV")));
    }
    {
        const auto first = Dal::ValueByMonteCarlo(product, model, 4096, String_("mrg32"));
        const auto second = Dal::ValueByMonteCarlo(product, model, 4096, String_("mrg32"));
        ASSERT_DOUBLE_EQ(first.at(String_("PV")), second.at(String_("PV")));
    }
    {
        const auto first = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"), true);
        const auto second = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"), true);
        ASSERT_DOUBLE_EQ(first.at(String_("PV")), second.at(String_("PV")));
    }
}

TEST(ValueTest, TestCompiledMatchesInterpreted) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_compiled");
    const auto model = MakeBSModel("value_compiled_model");

    const auto interpreted = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"), false, false, 0.01, false);
    const auto compiled = Dal::ValueByMonteCarlo(product, model, 4096, String_("sobol"), false, false, 0.01, true);

    ASSERT_DOUBLE_EQ(interpreted.at(String_("PV")), compiled.at(String_("PV")));
}

TEST(ValueTest, TestAadReturnsValueAndGreeks) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_aad");
    const auto model = MakeBSModel("value_aad_model");

    const auto result = Dal::ValueByMonteCarlo(product, model, 65536, String_("sobol"), false, true);

    double call, delta, vega;
    BlackScholesReference(&call, &delta, &vega);
    ASSERT_NEAR(result.at(String_("PV")), call, 0.05);
    ASSERT_NEAR(result.at(String_("d_spot")), delta, 0.01);
    ASSERT_NEAR(result.at(String_("d_vol")), vega, 0.2);
    ASSERT_TRUE(result.find(String_("d_rate")) != result.end());
    ASSERT_TRUE(result.find(String_("d_div")) != result.end());
}

TEST(ValueTest, TestRejectsNonPositivePathCounts) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_bad_paths");
    const auto model = MakeBSModel("value_bad_paths_model");

    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 0), Dal::Exception_);
    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, -1), Dal::Exception_);
}

TEST(ValueTest, TestRejectsUnsupportedModelType) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_bad_model");
    const Handle_<Dal::ModelData_> model(new DummyModelData_);

    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 1), Dal::Exception_);
}

TEST(ValueTest, TestRejectsUnknownRngMethod) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeEuropeanCall("value_bad_rsg");
    const auto model = MakeBSModel("value_bad_rsg_model");

    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 16, String_("not_a_rng")), Dal::Exception_);
}
