#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <dal-public/src/xccymontecarlo.hpp>

namespace py = pybind11;

void init_xccy_mc_core_test(py::module_& m) {
    m.def("_XccyMcEvaluateCoreForTesting", [](const std::shared_ptr<Dal::CalibrationInstrument_>& instrument,
                                              const std::shared_ptr<Dal::XccyGaussianModelData_>& model, const Dal::XccyMonteCarloRequest_& request) {
        using namespace Dal;
        const Handle_<CalibrationInstrument_> owned(instrument);
        const Handle_<XccyGaussianModelData_> data(model);
        const auto input = request;
        XccyMcResult_ result;
        {
            py::gil_scoped_release release;
            XccyScriptContext_ xccy;
            xccy.valuationTime_ = data->market_->valuationTime_;
            xccy.pair_ = data->market_->pair_;
            xccy.collateralCurrency_ = data->market_->collateralCurrency_;
            xccy.discountRoute_ = data->market_->discountRoute_;
            xccy.fixings_ = input.fixings_;
            CalibrationPreparationContext_ context;
            context.xccy_ = std::make_shared<const XccyScriptContext_>(xccy);
            XccyMcSettings_ settings;
            settings.phase_ = input.phase_;
            settings.batches_ = input.sampling_.batches_;
            settings.pathsPerBatch_ = input.sampling_.pathsPerBatch_;
            settings.seed_ = input.sampling_.seed_;
            settings.streamVersion_ = input.sampling_.streamVersion_;
            settings.compiled_ = input.simulation_.compiled_.value_or(Script::DEFAULT_COMPILED);
            settings.aad_ = input.derivativeMethod_ == CalibrationDerivative_::Value_::AAD;
            settings.parameterLabels_ = input.parameterLabels_;
            result = PreparedXccyMc_(owned, input.measureId_, context, data, settings).Evaluate();
        }
        py::dict output;
        output["mean"] = result.estimate_.mean_;
        output["standard_error"] = result.estimate_.standardError_;
        output["mapping_identity"] = std::string(result.mapping_.identity_.data(), result.mapping_.identity_.size());
        output["range_identity"] = std::string(result.reservation_.identity_.data(), result.reservation_.identity_.size());
        return output;
    });
}
