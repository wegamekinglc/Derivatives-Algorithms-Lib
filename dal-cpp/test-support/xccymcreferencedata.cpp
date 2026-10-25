#include <fstream>

#include <dal/curve/xccygaussianstaged.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/storage/json.hpp>

#include "../tests/script/xccymcfixture.hpp"

namespace {
    using namespace Dal;
    using namespace Dal::CalibrationInternal;
    using namespace Dal::TestXccyMc;
    using namespace Dal::TestXccyMoments;
    using namespace Dal::TestXccyExecution;
    Json_ Numbers(const Vector_<>& values) {
        return Array(values, [](double x) { return Num(x); });
    }
    Json_ CurveData(const Handle_<XccyGaussianCurveData_>& curve, const Date_& valuation) {
        return Obj({{"currency", Str(curve->currency_)},
                    {"route", Str(curve->route_)},
                    {"days", Array(curve->nodeDates_, [&](const auto& d) { return Num(d - valuation); })},
                    {"labels", Array(curve->nativeLabels_, [](const auto& x) { return Str(x); })},
                    {"values", Numbers(curve->nativeValues_)},
                    {"jacobian", Array(curve->nodeDates_, [&](const auto& d) {
                         const int row =
                             static_cast<int>(std::find(curve->nodeDates_.begin(), curve->nodeDates_.end(), d) - curve->nodeDates_.begin());
                         Vector_<> values(curve->nativeLabels_.size());
                         for (size_t col = 0; col < values.size(); ++col)
                             values[col] = curve->snapshotJacobian_(row, static_cast<int>(col));
                         return Numbers(values);
                     })}});
    }
    Json_ Case(const String_& name, const Handle_<XccyGaussianModelData_>& model, const XccyGaussianModelSpec_& spec, bool foreignSpread) {
        const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, foreignSpread);
        const auto context = Context(spec, Empty());
        auto settings = Settings(2, 3);
        const Script::PreparedXccyScript_ source(instrument, context, model);
        const Script::XccyScriptPathProvider_<> provider(source);
        const PreparedXccyMc_ mc(instrument, "Price", context, model, settings);
        auto rng = New(RNGType_("MRG32"), settings.seed_, mc.Mapping().dimension_, true);
        Vector_<Vector_<>> normals;
        for (const auto& range : mc.Reservation().ranges_) {
            rng->SkipNormalTo(range.firstPath_);
            for (size_t p = range.firstPath_; p < range.pathEnd_; ++p) {
                Vector_<> z(mc.Mapping().dimension_);
                rng->FillNormal(&z);
                normals.push_back(z);
            }
        }
        const auto& market = *model->market_;
        const Date_ valuation = market.valuationTime_.Date();
        Vector_<Handle_<XccyGaussianCurveData_>> curves{market.domestic_, market.foreign_, market.basis_};
        curves.Append(market.projections_);
        const auto& d = *model->dynamics_;
        static_cast<void>(XccyGaussianMomentNativeMap(*model, &settings.parameterLabels_));
        settings.aad_ = true;
        const auto aad = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
        return Obj(
            {{"name", Str(name)},
             {"foreign_spread", Bool(foreignSpread)},
             {"spot", Num(market.fxSpot_)},
             {"grid", Array(provider.Model().Grid(), [&](const auto& x) { return Num(x - valuation); })},
             {"curves", Array(curves, [&](const auto& c) { return CurveData(c, valuation); })},
             {"bucket_days", Array(d.bucketEndDates_, [&](const auto& x) { return Num(x - valuation); })},
             {"bucket_dates", Array(d.bucketEndDates_, [](const auto& x) { return Str(Date::ToString(x)); })},
             {"a", Numbers({d.domesticA_, d.foreignA_})},
             {"sigma", Array(Vector_<Vector_<>>{d.domesticSigma_, d.foreignSigma_, d.fxSigma_}, Numbers)},
             {"correlation", Array(Vector_<int>{0, 1, 2},
                                   [&](int row) { return Numbers({d.correlation_(row, 0), d.correlation_(row, 1), d.correlation_(row, 2)}); })},
             {"normals", Array(normals, Numbers)},
             {"labels", Array(settings.parameterLabels_, [](const auto& x) { return Str(x); })},
             {"observed_aad", Numbers(aad.estimate_.gradient_)},
             {"observed_price", Num(aad.estimate_.mean_)}});
    }
} // namespace

int main(int argc, char** argv) {
    REQUIRE(argc == 2, "reference data output path");
    Dal::RegisterAll_::Init();
    const auto nonflat = NonflatSpec();
    auto spec = ModelSpec();
    const auto definition =
        MakeCurveDefinition("basis", "USD", CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, LogDfScheme_::Value_::LOG_LINEAR,
                            {TestXccy::TODAY.AddDays(365), TestXccy::TODAY.AddDays(1095)}, TestXccy::TODAY, DayBasis::Act365F());
    spec.market_.basis_ = Handle_<DiscountCurve_>(BuildDiscountCurveT<double>(definition, {.003, .004}));
    auto staged = TestXccy::Spec();
    staged.domesticCurveBlock_ = spec.market_.domesticBlock_;
    staged.foreignCurveBlock_ = spec.market_.foreignBlock_;
    staged.gaussianDynamics_.reset(new XccyGaussianDynamics_("dynamics", spec.dynamics_));
    const auto pwc = XccyGaussianBasisCandidate(staged, spec.market_.basis_);
    std::ofstream output(argv[1]);
    const Vector_<Json_> cases{Case("Nonflat", Data(nonflat), nonflat, true), Case("Pwc", pwc, spec, false)};
    output << Jcs(Obj({{"cases", Array(cases, [](const auto& value) { return value; })}})) << '\n';
    REQUIRE(output.good(), "reference inputs written");
}
