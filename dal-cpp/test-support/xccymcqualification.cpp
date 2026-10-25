#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <dal/script/calibrationmc_internal.hpp>
#include <dal/script/xccymc.hpp>
#include <dal/storage/json.hpp>

#include "../tests/curve/xccygaussianexecutionreference.hpp"
#include "../tests/script/xccymcfixture.hpp"

namespace {
    using namespace Dal;
    using namespace Dal::CalibrationInternal;
    using namespace Dal::TestXccyMoments;
    using namespace Dal::TestXccyExecution;
    using namespace Dal::TestXccyMc;

    const Expectation_& Reference() {
        for (const auto& row : EXPECTATIONS)
            if (String_(row.mode_) == "MARK_TO_MARKET" && !row.foreign_ && row.receive_ && !row.zero_ && !row.nonflat_)
                return row;
        THROW("frozen MTM reference absent");
    }
    Json_ Flags(const XccyMcValidation_& check) {
        const auto& v = check.validation_;
        return Obj({{"parity", Bool(v.parityPassed_)},
                    {"precision", Bool(v.precisionPassed_)},
                    {"combined", Bool(v.valueErrorPassed_)},
                    {"numerics", Bool(v.numericalValidationPassed_)},
                    {"budget_exhausted", Bool(v.budgetExhausted_)},
                    {"precision_indicator", Num(v.precisionIndicator_)},
                    {"combined_indicator", Num(v.errorIndicator_)}});
    }
    Json_ Compare(const XccyMcResult_& result, double fast, const std::array<long double, 4>& reference, double scale, double epsilon) {
        const long double rounding = std::numeric_limits<long double>::epsilon() * std::max(std::abs(reference[0]), std::abs(reference[2]));
        const long double oracleError = reference[1] + rounding;
        const long double numeric = 4.L * std::max(std::abs(static_cast<long double>(fast) - reference[2]) + rounding, reference[3] + rounding);
        REQUIRE(oracleError <= epsilon * 2.5L && numeric <= epsilon * 2.5L, "direct numeric indicators meet frozen quarter budget");
        REQUIRE(std::abs(static_cast<long double>(fast) - reference[0]) <= oracleError + numeric + epsilon,
                "direct fast and independent cashflow gate");
        CalibrationMcPrecision_ precision;
        precision.errorScale_ = scale;
        precision.epsilonValue_ = epsilon * 10.;
        CalibrationMcReference_ oracle, analytic;
        oracle.value_ = static_cast<double>(reference[0]);
        oracle.numericalError_ = static_cast<double>(oracleError);
        analytic.value_ = fast;
        analytic.numericalError_ = static_cast<double>(numeric);
        const auto independent = ValidateXccyMc(result, oracle, precision), fastCheck = ValidateXccyMc(result, analytic, precision);
        return Obj({{"mean", Num(result.estimate_.mean_)},
                    {"standard_error", Num(result.estimate_.standardError_)},
                    {"fast", Num(fast)},
                    {"oracle", Num(oracle.value_)},
                    {"oracle_indicator", Num(oracle.numericalError_)},
                    {"fast_numeric_indicator", Num(analytic.numericalError_)},
                    {"scale", Num(scale)},
                    {"independent", Flags(independent)},
                    {"fast_comparison", Flags(fastCheck)}});
    }
} // namespace

int main(int argc, char** argv) try {
    REQUIRE(argc == 3, "usage: xccymcqualification OUTPUT.json REFINEMENT(1,2,4)");
    const String_ refinement(argv[2]);
    REQUIRE(refinement == "1" || refinement == "2" || refinement == "4", "predeclared x1/x2/x4 refinement only");
    const int factor = refinement == "1" ? 1 : (refinement == "2" ? 2 : 4);
    RegisterAll_::Init();
    const auto spec = ModelSpec();
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    auto settings = Settings(16, 16384 * factor);
    const PreparedXccyMc_ prepared(instrument, "Price", context, model, settings);
    const auto start = std::chrono::steady_clock::now();
    const auto price = prepared.Evaluate();
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const auto parPrepared = PrepareMoment(instrument, context, "BasisSpread");
    const auto fast = EvaluateXccyGaussianMoments<double>(parPrepared, *model);
    Vector_<CalibrationAffineCoefficients_> coefficients;
    for (const auto& batch : price.estimate_.batches_)
        coefficients.push_back(*batch.coefficients_);
    auto par = price;
    par.estimate_.measureId_ = "BasisSpread";
    par.estimate_.affine_ =
        CalibrationMcInternal::AffineEstimate(coefficients, *parPrepared.Measure().annuitySign_, *parPrepared.Measure().minAbsAnnuity_, 3.);
    par.estimate_.mean_ = par.estimate_.affine_->value_.value_;
    par.estimate_.standardError_ = par.estimate_.affine_->standardError_;
    const auto& reference = Reference();
    const auto json = Obj({{"schema", Str("dal.xccy-m4-qualification/1")},
                           {"model", Str(model->Identity())},
                           {"source", Str(instrument->Definition()->SourceIdentity())},
                           {"instrument", Str(instrument->InstrumentIdentity())},
                           {"plan", Str(prepared.PlanIdentity())},
                           {"mapping", Str(price.mapping_.identity_)},
                           {"range_layout", Str(price.reservation_.version_)},
                           {"reservation", Str(price.reservation_.identity_)},
                           {"phase", Str(settings.phase_.String())},
                           {"numeraire", Str(price.mapping_.numeraire_)},
                           {"stream", Str(settings.streamVersion_)},
                           {"seed", Num(settings.seed_)},
                           {"batches", Num(settings.batches_)},
                           {"paths_per_batch", Num(settings.pathsPerBatch_)},
                           {"dimension", Num(static_cast<double>(price.mapping_.dimension_))},
                           {"coordinates_consumed", Num(static_cast<double>(price.consumedCoordinates_))},
                           {"seconds", Num(seconds)},
                           {"gradient_qualification", Str("NotRequested")},
                           {"ranges", Array(price.reservation_.ranges_,
                                            [](const auto& r) {
                                                return Obj({{"first_path", Num(static_cast<double>(r.firstPath_))},
                                                            {"path_end", Num(static_cast<double>(r.pathEnd_))},
                                                            {"first_coordinate", Num(static_cast<double>(r.firstCoordinate_))},
                                                            {"coordinate_end", Num(static_cast<double>(r.coordinateEnd_))}});
                                            })},
                           {"price", Compare(price, fast.price_, reference.quantities_[2], .01, 1e-11)},
                           {"par_from_paired_batches", Compare(par, fast.Value(parPrepared), reference.quantities_[3], 1e-4, 1e-13)},
                           {"a", Num(price.coefficients_.a_)},
                           {"b", Num(price.coefficients_.b_)},
                           {"batch_coefficients", Array(coefficients, [](const auto& c) { return Obj({{"a", Num(c.a_)}, {"b", Num(c.b_)}}); })}});
    std::ofstream output(argv[1]);
    output << Jcs(json) << '\n';
    REQUIRE(output.good(), "qualification artifact written");
    std::cout << "paths=" << price.estimate_.pathsPerView_ << " seconds=" << seconds << " price=" << std::setprecision(16) << price.estimate_.mean_
              << " SE=" << price.estimate_.standardError_ << '\n';
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
