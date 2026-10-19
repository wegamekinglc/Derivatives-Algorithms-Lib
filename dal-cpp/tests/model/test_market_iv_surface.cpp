#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/integral/quadrature.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/hybriddata.hpp>
#include <dal/model/surface/marketivsurface.hpp>
#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    MarketIvFitRequest_ Request(double rho = 0., double eta = 0., int expiries = 3) {
        const Date_ today(2026, 10, 19);
        Vector_<Date_> dates;
        for (int j = 1; j <= expiries; ++j)
            dates.push_back(today.AddDays(182 * j));
        auto market = NewFlatMarketIvMarket(today, 100., .03, .01, dates);
        Vector_<MarketIvQuote_> quotes;
        for (int j = 0; j < expiries; ++j) {
            const double time = (dates[j] - today) / 365., theta = .04 * time;
            for (int i = -3; i <= 3; ++i) {
                const double k = .12 * i, phi = eta / std::sqrt(theta * (1. + theta));
                const double w = theta / 2. * (1. + rho * phi * k + std::sqrt(std::pow(phi * k + rho, 2) + 1. - rho * rho));
                quotes.push_back({String_(std::to_string(j) + ":" + std::to_string(i)), dates[j], market.Terms()[j].forward_ * std::exp(k),
                                  OptionType_(OptionType_::Value_::CALL), MarketIvQuoteType_(MarketIvQuoteType_::Value_::DECIMAL_IV),
                                  std::sqrt(w / time)});
            }
        }
        return {market, quotes, {}};
    }
    double GridPrice(const LocalVolSurfaceData_& surface, double maxStep) {
        auto data = MakeFlatRateLocalVolHybridModelData("market", "EQ[A]", 100., .03, .01, surface.spots_, surface.times_, surface.vols_, maxStep);
        auto model = CreateModel<double>(data);
        Vector_<AAD::SampleDef_> definitions(1);
        definitions[0].numeraire_ = true;
        definitions[0].indexNames_ = {"EQ[A]"};
        model->Allocate({1.}, definitions);
        model->Init({1.}, definitions);
        AAD::Scenario_<> path;
        AAD::AllocatePath(definitions, path);
        AAD::InitializePath(path);
        Vector_<> nodes(96), weights(96), gaussian(model->SimDim());
        Quadrature::NCDFGaussHermiteWeights(&nodes, &weights);
        double price = 0.;
        for (int i = 0; i < 96; ++i) {
            gaussian.Fill(nodes[i] / std::sqrt(double(gaussian.size())));
            model->GeneratePath(gaussian, &path);
            price += weights[i] * std::max(path[0].spot_ - 100., 0.) / path[0].numeraire_;
        }
        return price;
    }
} // namespace

TEST(MarketIvSurfaceTest, TestPlatformCertificateAndExtremePut) {
    const auto curve = MarketIvDetail::NewThetaCurve({0., 1., 2.}, {0., .04, .04});
    ASSERT_TRUE(curve.certified_);
    ASSERT_EQ(MarketIvDetail::ThetaAt(curve, 1.).second, 0.);
    ASSERT_EQ(MarketIvDetail::ThetaAt(curve, 3.).second, 0.);
    ASSERT_EQ(MarketIvDetail::NormalizePrice(1.e16, false, 1., 1.e16, 1.), 1.);
    ASSERT_FALSE(MarketIvDetail::NewThetaCurve({0., 1., 2.}, {0., .04, .03}).certified_);
    const auto valley = MarketIvDetail::NewThetaCurve({0., 1., 2., 3.}, {0., .08, .12, .20});
    ASSERT_TRUE(valley.certified_);
    const double independentMidpointSlope = .04 * 5. / 6.;
    ASSERT_NEAR(MarketIvDetail::ThetaAt(valley, 1.5).second, independentMidpointSlope, 1e-15);
    ASSERT_LE(valley.lowerBounds_[1], independentMidpointSlope);
}

TEST(MarketIvSurfaceTest, TestObjectiveTieUsesEarliestNearGlobalMinimum) {
    Vector_<MarketIvStart_> starts(4);
    for (int i = 0; i < 3; ++i) {
        starts[i].converged_ = starts[i].certified_ = starts[i].qualityAccepted_ = true;
        starts[i].objective_ = Vector_<>{1., .91, .89}[i];
    }
    ASSERT_EQ(MarketIvDetail::SelectStart(starts, .1), 1);
}

TEST(MarketIvSurfaceTest, TestParameterCertificateOneUlpBoundaries) {
    for (const auto box : {std::pair<double, double>{.999, 1e-8}, {.25, .99}}) {
        MarketIvFitSettings_ s;
        s.rhoLimit_ = box.first;
        s.constraintMargin_ = box.second;
        const double cap = MarketIvDetail::EtaMax(s);
        ASSERT_TRUE(MarketIvDetail::ParametersCertified(s, s.rhoLimit_, cap));
        ASSERT_TRUE(MarketIvDetail::ParametersCertified(s, -s.rhoLimit_, std::nextafter(cap, 0.)));
        ASSERT_FALSE(MarketIvDetail::ParametersCertified(s, 0., std::nextafter(cap, std::numeric_limits<double>::infinity())));
        ASSERT_FALSE(MarketIvDetail::ParametersCertified(s, std::nextafter(s.rhoLimit_, 1.), cap));
        const auto bound = MarketIvDetail::GlobalBounds(cap, s.rhoLimit_);
        ASSERT_LE(static_cast<long double>(cap) * cap * (1.L + s.rhoLimit_), static_cast<long double>(bound.first));
        ASSERT_LE(static_cast<long double>(bound.first), 4.L * (1.L - s.constraintMargin_));
    }
}

TEST(MarketIvSurfaceTest, TestThetaSlopesAvoidRepresentableIntermediateRangeLoss) {
    const auto tiny = MarketIvDetail::NewThetaCurve({0., 1., 2.}, {0., 1e-310, 2e-310});
    ASSERT_TRUE(tiny.certified_);
    for (const auto slope : tiny.slopes_) {
        ASSERT_GT(slope, 0.);
        ASSERT_NEAR(slope, 1e-310, 2. * std::numeric_limits<double>::denorm_min());
    }
    const auto huge = MarketIvDetail::NewThetaCurve({0., 10., 11.}, {0., 1e308, 1.1e308});
    ASSERT_TRUE(huge.certified_);
    for (const auto slope : huge.slopes_)
        ASSERT_NEAR(slope, 1e307, 1e293);
}

TEST(MarketIvSurfaceTest, TestIndependentEightyDigitSmileDerivativeFixtures) {
    const std::array<std::array<double, 10>, 6> cases{{{.04, .998, 1.4, .5, .177113033093138743, .274274052411300164, .0000432977061086657141,
                                                        2.64768173715897232, .0841334596636640026, .097955018718321487},
                                                       {.04, -.998, 1.4, -.5, .177113033093138743, -.274274052411300164, .0000432977061086657141,
                                                        2.64768173715897232, -.0841334596636640026, .097955018718321487},
                                                       {.04, -.4, 1e-14, .5, .0399999999999996078, -7.84464540552715935e-16, 4.03846153846165726e-29,
                                                        .999999999999995286, 9.80580675690924967e-16, -.0392232270276357968},
                                                       {4e-12, -.4, .8, .01, 4.80120021001139934e-9, 4.79999978994840197e-7, 4.20125992113500844e-12,
                                                        600.30007875689959, 8.00200019995148625e-9, 5.99999973743550246e-9},
                                                       {.04, -.4, 0., .5, .04, 0., 0., 1., 0., -.0392232270276368064},
                                                       {.04, .998, 1.4, -.5, .000193695728823065864, -.000228288857257875412, .000260855459069899601,
                                                        .00336071073356639753, -.0968315257030162139, .0000815317347349555043}}};
    for (const auto& c : cases) {
        const auto s = MarketIvDetail::Smile(c[0], c[1], c[2], c[3]);
        const std::array<double, 6> actual{s.w_, s.wk_, s.wkk_, s.wTheta_, s.wRho_, s.wEta_};
        ASSERT_FALSE(s.derivativeRange_);
        for (size_t i = 0; i < actual.size(); ++i)
            ASSERT_NEAR(actual[i], c[i + 4], 1e-12 * std::abs(c[i + 4]) + 1e-42);
    }
}

TEST(MarketIvSurfaceTest, TestFlatFitAndSeedProjection) {
    auto request = Request();
    request.settings_.rhoLimit_ = .25;
    request.settings_.constraintMargin_ = .99;
    const auto fit = FitMarketIvSurface(request);
    ASSERT_TRUE(fit.IsSuccess()) << fit.Status() << ": " << fit.Failure();
    ASSERT_EQ(fit.Starts().size(), 4);
    for (int i = 1; i < 4; ++i)
        ASSERT_TRUE(fit.Starts()[i].projectedSeed_);
    const auto snapshot = fit.Snapshot();
    ASSERT_EQ(snapshot.Eta(), 0.);
    ASSERT_EQ(snapshot.Rho(), 0.);
    for (const auto& row : fit.Quotes())
        ASSERT_NEAR(*row.nativeFitted_, .2, 1e-10);
    auto point = EvaluateMarketIvSurface(snapshot, {100., .75});
    ASSERT_EQ(point.DupireStatus(), "SUCCESS");
    ASSERT_NEAR(*point.LocalVol(), .2, 1e-10);
    ASSERT_NEAR(*point.DensityFactor(), 1., 1e-12);
    ASSERT_LE(fit.Execution().numericPayloadBytes_, fit.Execution().plannedNumericPayloadBytes_);
    ASSERT_LE(fit.Execution().peakWorkspaceBytes_, fit.Execution().plannedWorkspaceBytes_);
}

TEST(MarketIvSurfaceTest, TestSparseDataWarnsWithEffectiveFlatRank) {
    auto request = Request(0., 0., 1);
    request.quotes_ = {request.quotes_[3]};
    const auto fit = FitMarketIvSurface(request);
    ASSERT_TRUE(fit.IsSuccess());
    ASSERT_TRUE(fit.Underidentified());
    ASSERT_EQ(fit.SelectedStart(), 0);
    ASSERT_EQ(fit.Starts()[0].rank_, 1);
    ASSERT_EQ(fit.Status(), "SUCCESS_WITH_WARNINGS");
}

TEST(MarketIvSurfaceTest, TestSyntheticRecoveryAndWorkFailure) {
    auto request = Request(-.4, .8);
    const auto fit = FitMarketIvSurface(request);
    ASSERT_TRUE(fit.IsSuccess()) << fit.Status() << ": " << fit.Failure();
    ASSERT_NEAR(fit.Snapshot().Rho(), -.4, 1e-7);
    ASSERT_NEAR(fit.Snapshot().Eta(), .8, 1e-7);
    for (const auto& row : fit.Quotes())
        ASSERT_LT(std::abs(*row.normalizedResidual_), 1e-10);
    request.settings_.maxModelRowEvaluations_ = 0;
    const auto limited = FitMarketIvSurface(request);
    ASSERT_EQ(limited.Status(), "WORK_LIMIT");
    ASSERT_EQ(limited.Execution().modelRowEvaluations_, 0);
    ASSERT_THROW((void)limited.Snapshot(), Exception_);
    request.settings_.maxModelRowEvaluations_ = 5000000;
    request.settings_.numericPayloadBudgetBytes_ = fit.Execution().plannedNumericPayloadBytes_ - 1;
    ASSERT_THROW(FitMarketIvSurface(request), Exception_);
    request.settings_.maxIterationsPerStart_ = 1;
    const auto notConverged = FitMarketIvSurface(request);
    ASSERT_EQ(notConverged.Status(), "NO_CONVERGENCE");
    ASSERT_THROW((void)notConverged.Snapshot(), Exception_);
}

TEST(MarketIvSurfaceTest, TestExactBudgetsFinalRecomputeAndInvalidQuotes) {
    auto request = Request(-.4, .8);
    const auto fit = FitMarketIvSurface(request);
    ASSERT_TRUE(fit.IsSuccess());
    request.settings_.numericPayloadBudgetBytes_ = fit.Execution().plannedNumericPayloadBytes_;
    request.settings_.workspaceBudgetBytes_ = fit.Execution().plannedWorkspaceBytes_;
    request.settings_.maxModelRowEvaluations_ = fit.Execution().modelRowEvaluations_;
    ASSERT_TRUE(FitMarketIvSurface(request).IsSuccess());
    --request.settings_.maxModelRowEvaluations_;
    const auto limited = FitMarketIvSurface(request);
    ASSERT_EQ(limited.Status(), "WORK_LIMIT");
    ASSERT_EQ(limited.Execution().modelRowEvaluations_, request.settings_.maxModelRowEvaluations_);
    ASSERT_THROW((void)limited.Snapshot(), Exception_);
    --request.settings_.workspaceBudgetBytes_;
    ASSERT_THROW(FitMarketIvSurface(request), Exception_);
    request = Request();
    request.quotes_[1].id_ = request.quotes_[0].id_;
    ASSERT_THROW(FitMarketIvSurface(request), Exception_);
    request = Request();
    request.quotes_[0].bid_ = .19;
    ASSERT_THROW(FitMarketIvSurface(request), Exception_);
    request = Request();
    request.quotes_[0].weight_ = 0.;
    ASSERT_THROW(FitMarketIvSurface(request), Exception_);
}

TEST(MarketIvSurfaceTest, TestNonflatCarryAndRightDerivativeAgainstBlack) {
    const auto original = Request();
    auto terms = original.market_.Terms();
    terms[0].forward_ = 101.;
    terms[0].discount_ = .99;
    terms[1].forward_ = 100.5;
    terms[1].discount_ = 1.01;
    auto request = original;
    request.market_ = MarketIvMarket_(original.market_.EvaluationDate(), 100., terms);
    const auto snapshot = FitMarketIvSurface(request).Snapshot();
    const double time = (terms[0].expiry_ - request.market_.EvaluationDate()) / 365.;
    const double nextTime = (terms[1].expiry_ - request.market_.EvaluationDate()) / 365.;
    const double rate = -std::log(terms[1].discount_ / terms[0].discount_) / (nextTime - time);
    const double drift = std::log(terms[1].forward_ / terms[0].forward_) / (nextTime - time), yield = rate - drift;
    const double root = .2 * std::sqrt(time), d1 = std::log(terms[0].forward_ / 100.) / root + root / 2., d2 = d1 - root;
    const double call = terms[0].discount_ * (terms[0].forward_ * .5 * std::erfc(-d1 / std::sqrt(2.)) - 100. * .5 * std::erfc(-d2 / std::sqrt(2.)));
    const double ck = -terms[0].discount_ * .5 * std::erfc(-d2 / std::sqrt(2.));
    const double ckk = terms[0].discount_ * std::exp(-.5 * d2 * d2) / (std::sqrt(2. * std::acos(-1.)) * 100. * root);
    const auto point = EvaluateMarketIvSurface(snapshot, {100., time});
    ASSERT_EQ(point.CarryDerivative(), "RIGHT_CARRY_DERIVATIVE");
    ASSERT_NEAR(*point.CallPrice(), call, 1e-10);
    ASSERT_NEAR(*point.CallK(), ck, 1e-10);
    ASSERT_NEAR(*point.CallKK(), ckk, 1e-10);
    ASSERT_NEAR(*point.CallT(), .5 * .04 * 10000. * ckk - yield * call - drift * 100. * ck, 1e-10);
    ASSERT_NEAR(*point.LocalVol(), .2, 1e-10);
}

TEST(MarketIvSurfaceTest, TestExtremePutAndDisjointParityNeverCreateSnapshot) {
    const Date_ d(2026, 10, 19), expiry(2027, 10, 19);
    const auto market = NewFlatMarketIvMarket(d, 1., 0., 0., {expiry});
    MarketIvFitRequest_ request{market, {{"extreme", expiry, 1e16, OptionType_("PUT"), MarketIvQuoteType_("PRICE"), 1e16}}, {}};
    const auto extreme = FitMarketIvSurface(request);
    ASSERT_FALSE(extreme.IsSuccess());
    ASSERT_EQ(extreme.Quotes()[0].normalizedMid_, 1.);
    ASSERT_THROW((void)extreme.Snapshot(), Exception_);
    request.quotes_ = {{"call", expiry, 1., OptionType_("CALL"), MarketIvQuoteType_("PRICE"), .1, .09, .11},
                       {"put", expiry, 1., OptionType_("PUT"), MarketIvQuoteType_("PRICE"), .2, .19, .21}};
    const auto inconsistent = FitMarketIvSurface(request);
    ASSERT_EQ(inconsistent.Status(), "INCONSISTENT_QUOTES");
    ASSERT_EQ(inconsistent.Execution().modelRowEvaluations_, 0);
}

TEST(MarketIvSurfaceTest, TestPlateauZeroLocalVolAndUnweightedBadBand) {
    auto request = Request(0., 0., 2);
    for (auto& quote : request.quotes_)
        quote.mid_ = std::sqrt(.04 / ((quote.expiry_ - request.market_.EvaluationDate()) / 365.));
    const auto fit = FitMarketIvSurface(request);
    ASSERT_TRUE(fit.IsSuccess()) << fit.Status();
    const auto snapshot = fit.Snapshot();
    const auto point = EvaluateMarketIvSurface(snapshot, {100., 3.});
    ASSERT_EQ(point.DupireStatus(), "SUCCESS");
    ASSERT_EQ(*point.LocalVol(), 0.);
    ASSERT_EQ(*point.VarianceT(), 0.);
    ASSERT_LT(snapshot.Certificate().wingSlopeBound_, 2.);
    for (const double lower : snapshot.Certificate().derivativeLowerBounds_)
        ASSERT_GE(lower, 0.);
    request = Request();
    for (auto& quote : request.quotes_)
        quote.weight_ = 1e308;
    request.quotes_[0].weight_ = std::numeric_limits<double>::denorm_min();
    request.quotes_[0].mid_ = .8;
    const auto rejected = FitMarketIvSurface(request);
    ASSERT_EQ(rejected.Status(), "FIT_QUALITY_FAILURE");
    ASSERT_EQ(rejected.Quotes()[0].normalizedWeight_, 0.);
    ASSERT_THROW((void)rejected.Snapshot(), Exception_);
}

TEST(MarketIvSurfaceTest, TestExportArchiveAndFlatGridRepricing) {
    const auto snapshot = FitMarketIvSurface(Request()).Snapshot();
    const auto coarse = CalibrateMarketIvDupire(snapshot, {{80., 120.}, {.1, 1.}, {}});
    const auto fine = CalibrateMarketIvDupire(snapshot, {{70., 80., 90., 100., 110., 120., 130.}, {.1, .2, .4, .6, .8, 1.}, {}});
    ASSERT_TRUE(coarse.IsSuccess());
    ASSERT_TRUE(fine.IsSuccess());
    const auto restored = handle_cast<LocalVolSurfaceData_>(JSON::ReadString(JSON::WriteString(fine.SurfaceData()), false));
    ASSERT_EQ(restored->spots_, fine.SurfaceData().spots_);
    ASSERT_EQ(restored->times_, fine.SurfaceData().times_);
    for (const auto vol : restored->vols_)
        ASSERT_NEAR(vol, .2, 1e-10);
    const double forward = 100. * std::exp(.02), d1 = .02 / .2 + .1, d2 = d1 - .2;
    const double expected = std::exp(-.03) * (forward * .5 * std::erfc(-d1 / std::sqrt(2.)) - 100. * .5 * std::erfc(-d2 / std::sqrt(2.)));
    const double coarsePrice = GridPrice(coarse.SurfaceData(), 1.), finePrice = GridPrice(*restored, 1. / 16.);
    ASSERT_NEAR(coarsePrice, expected, .06);
    ASSERT_NEAR(finePrice, expected, .06);
    ASSERT_NEAR(coarsePrice, finePrice, 1e-9);
}

TEST(MarketIvSurfaceTest, TestRefitIsDistinctFromFixedBaseSpreadAndPreservesRecording) {
    AAD::RecordingScope_ recording;
    AAD::Number_ input;
    recording.RegisterInput(input, 2.);
    recording.StartRecording();
    AAD::Number_ output = input * input;
    const auto request = Request(-.4, .8);
    const auto snapshot = FitMarketIvSurface(request).Snapshot();
    const MarketIvRefitRiskRequest_ risk{{request.quotes_[10].id_}, {{{80., .75}, MarketIvQuantity_("CALL_PRICE")}}, {}, {1e-4}};
    const auto refit = EvaluateMarketIvRefitRisk(snapshot, risk);
    ASSERT_TRUE(refit.Complete());
    ASSERT_GT(std::abs(*refit.Responses()[0].symmetricBumpSlope_), 1e-5);
    const auto legacy = MarketIvSurfaceAsLegacyIVS(snapshot);
    AAD::RiskView_<double> spread({80., request.quotes_[10].strike_, 140.}, {snapshot.Times()[1], snapshot.Times()[2], snapshot.Times()[3]});
    spread.Bump(1, 1, 1e-4);
    ASSERT_DOUBLE_EQ(legacy->Call(80., .75, &spread), legacy->Call(80., .75));
    recording.FinishRecording();
    AAD::NativeOperations_::AddSeed(output, 1.);
    recording.Reverse();
    ASSERT_EQ(AAD::NativeOperations_::ReadAdjoint(input), 4.);
    recording.Close();
}

TEST(MarketIvSurfaceTest, TestExactDupireGridAndCompleteRefit) {
    const auto request = Request(-.4, .8);
    const auto base = FitMarketIvSurface(request).Snapshot();
    const auto grid = CalibrateMarketIvDupire(base, {{85., 100., 120.}, {.2, .8}, {}});
    ASSERT_TRUE(grid.IsSuccess()) << grid.Failure();
    ASSERT_EQ(grid.Nodes().size(), 6);
    ASSERT_EQ(grid.SurfaceData().vols_.Rows(), 3);
    ASSERT_EQ(grid.SurfaceData().vols_.Cols(), 2);
    const auto unavailable = CalibrateMarketIvDupire(base, {{100.}, {1e-12}, {}});
    ASSERT_FALSE(unavailable.IsSuccess());
    ASSERT_THROW((void)unavailable.SurfaceData(), Exception_);
    MarketIvRefitRiskRequest_ risk{{request.quotes_[10].id_},
                                   {{{100., .75}, MarketIvQuantity_(MarketIvQuantity_::Value_::CALL_PRICE)},
                                    {{110., .8}, MarketIvQuantity_(MarketIvQuantity_::Value_::LOCAL_VOL)}},
                                   {},
                                   {1e-5}};
    const auto response = EvaluateMarketIvRefitRisk(base, risk);
    ASSERT_TRUE(response.Complete());
    ASSERT_EQ(response.Responses().size(), 2);
    for (int side : {-1, 1}) {
        auto bumped = request;
        bumped.quotes_[10].mid_ += side * 1e-5;
        const auto independent = FitMarketIvSurface(bumped);
        ASSERT_TRUE(independent.IsSuccess());
        for (size_t o = 0; o < 2; ++o) {
            const auto point = EvaluateMarketIvSurface(independent.Snapshot(), risk.outputs_[o].query_);
            const double value = o == 0 ? *point.CallPrice() : *point.LocalVol();
            ASSERT_NEAR(side == 1 ? *response.Responses()[o].plus_ : *response.Responses()[o].minus_, value, 1e-11);
        }
    }
    risk.settings_.maxTotalModelRowEvaluations_ = 1;
    const auto limited = EvaluateMarketIvRefitRisk(base, risk);
    ASSERT_FALSE(limited.Complete());
    ASSERT_EQ(limited.Execution().modelRowEvaluations_, 1);
    ASSERT_FALSE(limited.Responses()[0].symmetricBumpSlope_);
}

TEST(MarketIvSurfaceTest, TestGridAndRefitExactByteBudgets) {
    const auto request = Request(-.4, .8);
    const auto base = FitMarketIvSurface(request).Snapshot();
    MarketIvDupireRequest_ grid{{90., 110.}, {.5, 1.}, {}};
    const auto fitted = CalibrateMarketIvDupire(base, grid);
    grid.settings_.numericPayloadBudgetBytes_ = fitted.Execution().plannedNumericPayloadBytes_;
    grid.settings_.workspaceBudgetBytes_ = fitted.Execution().plannedWorkspaceBytes_;
    ASSERT_TRUE(CalibrateMarketIvDupire(base, grid).IsSuccess());
    --grid.settings_.numericPayloadBudgetBytes_;
    ASSERT_THROW(CalibrateMarketIvDupire(base, grid), Exception_);
    ++grid.settings_.numericPayloadBudgetBytes_;
    --grid.settings_.workspaceBudgetBytes_;
    ASSERT_THROW(CalibrateMarketIvDupire(base, grid), Exception_);
    MarketIvRefitRiskRequest_ risk{{request.quotes_[10].id_}, {{{100., .75}, MarketIvQuantity_("CALL_PRICE")}}, {}, {1e-5}};
    const auto response = EvaluateMarketIvRefitRisk(base, risk);
    risk.settings_.numericPayloadBudgetBytes_ = response.Execution().plannedNumericPayloadBytes_;
    risk.settings_.workspaceBudgetBytes_ = response.Execution().plannedWorkspaceBytes_;
    ASSERT_TRUE(EvaluateMarketIvRefitRisk(base, risk).Complete());
    --risk.settings_.numericPayloadBudgetBytes_;
    ASSERT_THROW(EvaluateMarketIvRefitRisk(base, risk), Exception_);
    ++risk.settings_.numericPayloadBudgetBytes_;
    --risk.settings_.workspaceBudgetBytes_;
    ASSERT_THROW(EvaluateMarketIvRefitRisk(base, risk), Exception_);
}

TEST(MarketIvSurfaceTest, TestStableDefaultPriceBumpAtExtremeCarryScale) {
    const Date_ d(2026, 10, 19), expiry(2027, 10, 19);
    const double forward = 1e308, discount = std::numeric_limits<double>::denorm_min(), scale = discount * forward;
    const double mid = scale * std::erf(.2 / (2. * std::sqrt(2.)));
    const MarketIvFitRequest_ request{MarketIvMarket_(d, forward, {{expiry, forward, discount}}),
                                      {{"price", expiry, forward, OptionType_("CALL"), MarketIvQuoteType_("PRICE"), mid}},
                                      {}};
    const auto base = FitMarketIvSurface(request).Snapshot();
    const auto response = EvaluateMarketIvRefitRisk(base, {{"price"}, {{{forward, 1.}, MarketIvQuantity_("CALL_PRICE")}}, {}, {}});
    ASSERT_TRUE(response.Complete());
    ASSERT_EQ(response.Axis()[0].step_, scale * 1e-4);
    ASSERT_NEAR(*response.Responses()[0].symmetricBumpSlope_, 1., 1e-7);
}
