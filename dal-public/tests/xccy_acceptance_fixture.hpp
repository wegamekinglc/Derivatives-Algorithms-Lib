#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>

#include <dal-public/src/storage.hpp>
#include <dal-public/src/xccymontecarlo.hpp>
#include <dal/math/aad/recording.hpp>

#include "../../dal-cpp/tests/curve/xccygaussianmomentfixture.hpp"

namespace Dal::TestXccyAcceptance {
    inline String_ ReadFixture(const std::string& name) {
        std::ifstream input(std::filesystem::path(__FILE__).parent_path() / "fixtures" / "xccy_acceptance" / name);
        REQUIRE(input.good(), "missing immutable XCCY acceptance fixture");
        const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        return String_(text);
    }
    inline Handle_<CalibrationInstrument_> Instrument(XccyNotionalMode_ mode) {
        return LoadCalibrationInstrumentJson(ReadFixture(std::string(mode.String()) + ".json"));
    }
    inline Handle_<XccyGaussianModelData_> Model() {
        const auto text = ReadFixture("model.json");
        return handle_cast<XccyGaussianModelData_>(ReadObjectJson(text.data(), text.size()));
    }
    inline XccyMonteCarloRequest_ Request(XccyMonteCarloPhase_ phase, const String_& measure) {
        XccyMonteCarloRequest_ request;
        request.measureId_ = measure;
        request.phase_ = phase;
        request.sampling_ = {"PseudoRandom", 8, 4, 1024, "dal.mrg32-normal-paths/1"};
        request.fixings_.reset(new MarketFixingSnapshot_({}));
        request.simulation_.compiled_ = true;
        request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
        request.parameterLabels_ = {"sigma:EUR/USD:2029-10-20", "fxspot:EUR/USD"};
        return request;
    }
    inline CalibrationPreparationContext_ Context(const XccyGaussianModelData_& model, const Handle_<MarketFixingSnapshot_>& fixings) {
        XccyScriptContext_ xccy;
        xccy.valuationTime_ = model.market_->valuationTime_;
        xccy.pair_ = model.market_->pair_;
        xccy.collateralCurrency_ = model.market_->collateralCurrency_;
        xccy.discountRoute_ = model.market_->discountRoute_;
        xccy.fixings_ = fixings;
        CalibrationPreparationContext_ context;
        context.xccy_ = std::make_shared<const XccyScriptContext_>(xccy);
        return context;
    }
    inline XccyMcResult_
    Core(const Handle_<CalibrationInstrument_>& instrument, const Handle_<XccyGaussianModelData_>& model, const XccyMonteCarloRequest_& request) {
        XccyMcSettings_ settings;
        settings.phase_ = request.phase_;
        settings.pathsPerBatch_ = request.sampling_.pathsPerBatch_;
        settings.batches_ = request.sampling_.batches_;
        settings.seed_ = request.sampling_.seed_;
        settings.streamVersion_ = request.sampling_.streamVersion_;
        settings.compiled_ = *request.simulation_.compiled_;
        settings.aad_ = request.derivativeMethod_ == CalibrationDerivative_::Value_::AAD;
        settings.parameterLabels_ = request.parameterLabels_;
        return PreparedXccyMc_(instrument, request.measureId_, Context(*model, request.fixings_), model, settings).Evaluate();
    }
    inline String_ Json(const XccyMcResult_& core) {
        XccyMonteCarloResult_ result;
        result.details_ = std::make_shared<const XccyMcResult_>(core);
        return DumpXccyMonteCarloResultJson(result);
    }
    struct Case_ {
        XccyNotionalMode_ mode_;
        String_ measure_;
        XccyMonteCarloPhase_ phase_;
        size_t firstPath_;
    };
    inline Vector_<Case_> Cases() {
        return {{XccyNotionalMode_::Value_::FIXED, "Price", XccyMonteCarloPhase_::Value_::Fit, 0},
                {XccyNotionalMode_::Value_::RESETTABLE, "BasisSpread", XccyMonteCarloPhase_::Value_::Validation, 262144},
                {XccyNotionalMode_::Value_::MARK_TO_MARKET, "Price", XccyMonteCarloPhase_::Value_::HeldOut, 1310720}};
    }
} // namespace Dal::TestXccyAcceptance
