#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/model/gsrbondoption.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal {
    using namespace CalibrationInternal;

    GSRPriceResult_ PriceGaussianBondOption(const PreparedCalibrationInstrument_& plan,
                                            const Handle_<CalibrationInstrument_>& instrument,
                                            const CalibrationPreparationContext_& context) {
        plan.RequireCompatible(instrument, context);
        CalibrationDiagnostic_ d;
        d.stage_ = CalibrationStage_::Value_::Evaluate;
        d.instrumentId_ = instrument->Id();
        d.measureId_ = plan.Measure().id_;
        const auto* descriptor = std::get_if<CalibrationBondOptionDescriptor_>(&plan.Descriptor());
        Check(descriptor && plan.Capability().backend_ == CalibrationBackend_::Value_::FastGaussian &&
                  plan.Measure().kind_ == CalibrationMeasureKind_::Value_::Price,
              "UnsupportedFastEvaluator", "/plan", "admitted hard Gaussian BondOption Price", "", d);
        Check(descriptor->maturity_ <= context.curve_->nodeDates_.back(), "UnsupportedFastEvaluator", "/source/rows",
              "bond maturity within the frozen curve", "", d);
        const BondOption_ option{descriptor->expiry_, descriptor->maturity_, descriptor->strike_, OptionType_(descriptor->call_ ? "CALL" : "PUT")};
        auto result = typeid(*context.model_) == typeid(MultiFactorGSRModelData_)
                          ? PriceGSREuropeanOption(static_cast<const MultiFactorGSRModelData_&>(*context.model_), option)
                          : PriceGSREuropeanOption(static_cast<const GSRModelData_&>(*context.model_), option);
        result.price_ *= descriptor->notional_;
        result.numericalError_ *= descriptor->notional_;
        Check(std::isfinite(result.price_), "CalibrationNumericalFailure", "/price", "finite contract amount", "", d);
        return result;
    }

    Handle_<CalibrationInstrument_> NewBondOptionScriptInstrument(const BondOption_& option, const BondOptionScriptImportSettings_& settings) {
        CalibrationDiagnostic_ d;
        d.instrumentId_ = settings.instrumentId_;
        const String_ code = "UnsupportedCalibrationProduct";
        Check(option.expiry_.IsValid() && option.maturity_.IsValid() && option.expiry_ < option.maturity_, code, "/option/dates",
              "valid expiry before bond maturity", "", d);
        Check(option.type_ == OptionType_::Value_::CALL || option.type_ == OptionType_::Value_::PUT, code, "/option/type", "CALL or PUT", "", d);
        const auto currency = Token(settings.currency_, code, "/import/currency", d);
        const auto priceUnit = CalibrationValueUnit_::Parse("Price:" + currency);
        ScriptInstrumentDefinitionSpec_ spec;
        spec.header_.payCurrency_ = spec.header_.resultCurrency_ = spec.header_.collateralCurrency_ = settings.currency_;
        spec.header_.discountRoute_ = settings.discountRoute_;
        const String_ fixing = "FIX(IR[" + currency + ",DF," + Date::ToString(option.maturity_) + "])";
        const String_ difference = option.type_ == OptionType_::Value_::CALL ? fixing + "-K" : "K-" + fixing;
        spec.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", NumberText(settings.notional_)},
                              {ScriptSourceRowKind_::Value_::Definition, "K", NumberText(option.strike_)},
                              {ScriptSourceRowKind_::Value_::Event, Date::ToString(option.expiry_), "PAY PAYS N*MAX(" + difference + ",0)"}};
        spec.parameters_ = {{"N", ScriptParameterType_::Value_::Real, priceUnit, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
                            {"K",
                             ScriptParameterType_::Value_::Real,
                             CalibrationValueUnit_::Parse("BondPrice"),
                             ScriptParameterRole_::Value_::Strike,
                             true,
                             0.,
                             {},
                             false,
                             true}};
        spec.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, priceUnit}};
        return NewScriptCalibrationInstrument({settings.instrumentId_, NewScriptInstrumentDefinition(spec)});
    }
} // namespace Dal
