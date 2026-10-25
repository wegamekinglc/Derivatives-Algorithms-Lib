#include "../../src/bindings/bindings.h"
#include "../../src/bindings/calibrationinput.hpp"
#include <dal-public/src/global.hpp>
#include <dal-public/src/xccymontecarlobasket.hpp>
#include <dal/script/xccymcbasket_internal.hpp>

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Python::Calibration;

namespace {
    PreparedXccyMcBasket_ PrepareCore(const Vector_<XccyMcBasketMember_>& rows,
                                      const Handle_<XccyGaussianModelData_>& data,
                                      const XccyMonteCarloBasketRequest_& input,
                                      bool retain = false) {
        XccyScriptContext_ x;
        x.valuationTime_ = data->market_->valuationTime_;
        x.pair_ = data->market_->pair_;
        x.collateralCurrency_ = data->market_->collateralCurrency_;
        x.discountRoute_ = data->market_->discountRoute_;
        x.fixings_ = input.fixings_;
        CalibrationPreparationContext_ context;
        context.xccy_ = std::make_shared<const XccyScriptContext_>(x);
        XccyMcSettings_ s;
        s.phase_ = input.phase_;
        s.batches_ = input.sampling_.batches_;
        s.pathsPerBatch_ = input.sampling_.pathsPerBatch_;
        s.seed_ = input.sampling_.seed_;
        s.streamVersion_ = input.sampling_.streamVersion_;
        s.compiled_ = input.simulation_.compiled_.value_or(Script::DEFAULT_COMPILED);
        s.aad_ = input.derivativeMethod_ == CalibrationDerivative_::Value_::AAD;
        s.parameterLabels_ = input.parameterLabels_;
        s.retainPaths_ = retain;
        return PreparedXccyMcBasket_(rows, context, data, s);
    }
} // namespace

void XccyBasketThrowForTesting(const std::string& fault,
                               const Vector_<XccyMcBasketMember_>& rows,
                               const Handle_<XccyGaussianModelData_>& data,
                               const XccyMonteCarloBasketRequest_& request) {
    const auto plan = PrepareCore(fault == "Early" ? Vector_<XccyMcBasketMember_>() : rows, data, request, true);
    const std::map<std::string, std::tuple<String_, size_t, size_t>> locations{
        {"LastMember", {"Preflight", 2, 0}}, {"LateMember", {"AfterDraw", 1, 0}}, {"LateBatch", {"AfterDraw", 1, 1}}};
    const auto& target = locations.at(fault);
    static_cast<void>(XccyMcBasketInternal::TestAccess_::Evaluate(plan, {}, [&](const auto& stage, size_t i, size_t batch, size_t) {
        if (stage == std::get<0>(target) && i == std::get<1>(target) && batch == std::get<2>(target))
            CalibrationInputFailure(fault == "LastMember" ? "CalibrationContextMismatch" : "CalibrationNumericalFailure", "/execution/payment",
                                    "controlled basket projection failure");
    }));
    THROW("test fault was not reached");
}

void init_xccy_mc_basket_test(py::module_& m) {
    m.def("_XccyBasketCoreForTesting",
          [](const py::object& members, const std::shared_ptr<XccyGaussianModelData_>& model, const XccyMonteCarloBasketRequest_& request) {
              const auto rows = Input_<Vector_<XccyMcBasketMember_>>::Read(members, "/members");
              const Handle_<XccyGaussianModelData_> data(model);
              const auto input = request;
              py::gil_scoped_release release;
              return PrepareCore(rows, data, input).Evaluate();
          });
}
