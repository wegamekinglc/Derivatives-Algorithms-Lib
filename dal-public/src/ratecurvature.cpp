//
// Created by Codex on 2026/10/10.
//

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <typeinfo>
#include <variant>

#include <dal-public/src/calibrationrisk.hpp>
#include <dal-public/src/ratecurvature.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/math/aad/detail/gradientbumps.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    namespace {
        namespace Bumps = AAD::GradientBumpsDetail;
        using Source_ =
            std::variant<CurveCalibrationSpec_, JointMultiCurveCalibrationSpec_, CrossCurrencyCalibrationSpec_, JointXccyCalibrationSpec_>;

        template <class T_> Handle_<YCInstrument_> RequoteIndexed(const YCInstrument_& instrument, double quote) {
            const auto& typed = static_cast<const T_&>(instrument);
            const auto span = typed.TimeSpan();
            return Handle_<YCInstrument_>(new T_(typed.TradeDate(), span.first, span.second, quote, typed.FloatConvention()));
        }

        template <class T_> Handle_<YCInstrument_> RequoteSwap(const YCInstrument_& instrument, double quote) {
            const auto& typed = static_cast<const T_&>(instrument);
            const auto span = typed.TimeSpan();
            return Handle_<YCInstrument_>(new T_(typed.TradeDate(), span.first, span.second, quote, typed.FixedLegConvention(),
                                                 typed.FloatConvention(), typed.FloatLegConvention()));
        }

        Handle_<YCInstrument_> Requote(const Handle_<YCInstrument_>& instrument, double quote) {
            REQUIRE(instrument, "RateCalibration: empty calibration instrument");
            REQUIRE(std::isfinite(quote), "RateCalibration: quotes must be finite");
            const auto& type = typeid(*instrument);
            if (type == typeid(Deposit_))
                return RequoteIndexed<Deposit_>(*instrument, quote);
            if (type == typeid(FRA_))
                return RequoteIndexed<FRA_>(*instrument, quote);
            if (type == typeid(STIR_))
                return RequoteIndexed<STIR_>(*instrument, quote);
            if (type == typeid(Swap_))
                return RequoteSwap<Swap_>(*instrument, quote);
            if (type == typeid(OISSwap_))
                return RequoteSwap<OISSwap_>(*instrument, quote);
            if (type == typeid(Future_)) {
                const auto& typed = static_cast<const Future_&>(*instrument);
                const auto span = typed.TimeSpan();
                return Handle_<YCInstrument_>(
                    new Future_(typed.TradeDate(), span.first, span.second, quote, typed.FloatConvention(), typed.ConvexityAdjustment()));
            }
            if (type == typeid(BasisSwap_)) {
                const auto& typed = static_cast<const BasisSwap_&>(*instrument);
                const auto span = typed.TimeSpan();
                return Handle_<YCInstrument_>(new BasisSwap_(typed.TradeDate(), span.first, span.second, quote, typed.SpreadIndexConvention(),
                                                             typed.SpreadLegConvention(), typed.ReferenceIndexConvention(),
                                                             typed.ReferenceLegConvention()));
            }
            THROW("RateCalibration: unsupported native instrument type");
        }

        Vector_<Handle_<YCInstrument_>> SealInstruments(const Vector_<Handle_<YCInstrument_>>& instruments) {
            Vector_<Handle_<YCInstrument_>> sealed;
            for (const auto& instrument : instruments) {
                // Type admission precedes the caller's virtual MarketRate accessor.
                const auto admitted = Requote(instrument, 0.0);
                sealed.push_back(Requote(admitted, instrument->MarketRate()));
            }
            return OrderInstruments(sealed);
        }

        Handle_<CrossCurrencySwap_> Requote(const Handle_<CrossCurrencySwap_>& instrument, double quote) {
            REQUIRE(instrument, "RateCalibration: empty cross-currency instrument");
            REQUIRE(std::isfinite(quote), "RateCalibration: quotes must be finite");
            const auto span = instrument->TimeSpan();
            return Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(instrument->TradeDate(), span.first, span.second, quote, instrument->Config()));
        }

        Vector_<Handle_<CrossCurrencySwap_>> SealInstruments(const Vector_<Handle_<CrossCurrencySwap_>>& instruments) {
            Vector_<Handle_<CrossCurrencySwap_>> sealed;
            for (const auto& instrument : instruments) {
                REQUIRE(instrument, "RateCalibration: empty cross-currency instrument");
                sealed.push_back(Requote(instrument, instrument->MarketRate()));
            }
            return sealed;
        }

        class CurveSealer_ {
            Date_ today_;
            std::map<const DiscountCurve_*, Handle_<DiscountCurve_>> copies_;
            std::set<const DiscountCurve_*> active_;

        public:
            explicit CurveSealer_(const Date_& today) : today_(today) {}
            Handle_<DiscountCurve_> Seal(const Handle_<DiscountCurve_>& curve) {
                if (!curve)
                    return {};
                return Seal(*curve);
            }
            Handle_<DiscountCurve_> Seal(const DiscountCurve_& curve) {
                const auto found = copies_.find(&curve);
                if (found != copies_.end())
                    return found->second;
                const auto& type = typeid(curve);
                REQUIRE(type == typeid(Tape::DiscountPWC_<double>) || type == typeid(Tape::DiscountPWLF_<double>) ||
                            type == typeid(Tape::DiscountLogDF_<double>) || type == typeid(Tape::DiscountZeroRate_<double>),
                        "RateCalibration: unsupported native fixed curve type");
                REQUIRE(active_.insert(&curve).second, "RateCalibration: cyclic fixed curve base");
                const auto state = InspectCurveParameters(curve, today_);
                const auto copy =
                    Handle_<DiscountCurve_>(BuildDiscountCurveT<double>(state.definition_, state.passiveParameters_, Seal(state.passiveBase_)));
                active_.erase(&curve);
                copies_.emplace(&curve, copy);
                return copy;
            }
        };

        Handle_<CurveBlock_> SealBlock(const Handle_<CurveBlock_>& block, CurveSealer_* curves) {
            REQUIRE(block, "RateCalibration: empty fixed curve block");
            REQUIRE(typeid(*block) == typeid(CurveBlock_), "RateCalibration: unsupported native fixed curve block type");
            if (block->DiscountCurves().empty())
                return Handle_<CurveBlock_>(
                    new CurveBlock_(curves->Seal(block->Discount(CollateralType_(CollateralType_::Value_::OIS))), block->LiborBasis()));
            auto discounts = block->DiscountCurves();
            auto forwards = block->ForwardCurves();
            for (auto& entry : discounts)
                entry.second = curves->Seal(entry.second);
            for (auto& entry : forwards)
                entry.second = curves->Seal(entry.second);
            return Handle_<CurveBlock_>(new CurveBlock_(block->Name(), block->ccy_.String(), discounts, forwards, block->LiborBasis()));
        }

        Handle_<MarketFixingSnapshot_> SealFixings(const Handle_<MarketFixingSnapshot_>& fixings,
                                                   const Vector_<Handle_<CrossCurrencySwap_>>& instruments,
                                                   const DateTime_& valuationTime) {
            if (fixings)
                return fixings;
            Vector_<FixingRequest_> requests;
            for (const auto& instrument : instruments) {
                const auto span = instrument->TimeSpan();
                const auto plan = BuildXccyCashflowPlan(span.first, span.second, instrument->Config());
                requests.Append(RequiredHistoricalFixings(plan, valuationTime));
            }
            return SnapshotGlobalFixings(requests);
        }

        void RequireExact(CurveSolveMode_ mode) {
            REQUIRE(mode == CurveSolveMode_::Value_::EXACT, "RateCalibration: curvature requires EXACT calibration");
        }

        void RequireCurrency(const Ccy_& currency) {
            REQUIRE(currency.Switch() != Ccy_::Value_::_NOT_SET, "RateCalibration: currencies must be specified");
        }

        void RequireSquare(size_t parameters, size_t quotes) {
            REQUIRE(quotes > 0 && parameters == quotes, "RateCalibration: curvature requires a nonempty square calibration system");
            REQUIRE(quotes <= static_cast<size_t>(std::numeric_limits<int>::max() / 2),
                    "RateCalibration: objective input count exceeds integer range");
        }

        CurveCalibrationSpec_ Seal(const CurveCalibrationSpec_& spec) {
            RequireExact(spec.solveMode_);
            auto sealed = spec;
            sealed.instruments_ = SealInstruments(spec.instruments_);
            sealed.knotDates_ = BuildCurveCalibrationKnots(spec.today_, sealed.instruments_, spec.knotDates_, spec.knotPolicy_);
            sealed.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
            RequireSquare(InspectCurveCalibrationExecutionIdentity(sealed).freeParameters_.size(), sealed.instruments_.size());
            CurveSealer_ curves(spec.today_);
            sealed.baseCurve_ = curves.Seal(spec.baseCurve_);
            for (auto& entry : sealed.discountCurves_)
                entry.second = curves.Seal(entry.second);
            for (auto& entry : sealed.forwardCurves_)
                entry.second = curves.Seal(entry.second);
            return sealed;
        }

        struct SealedDeclarations_ {
            Vector_<JointCurveDeclaration_> curves_;
            size_t parameters_ = 0;
            size_t quotes_ = 0;
        };

        SealedDeclarations_ SealDeclarations(Vector_<JointCurveDeclaration_> curves, const Date_& today, const String_& ccy, DayBasis_ basis) {
            SealedDeclarations_ result;
            for (auto& curve : curves) {
                curve.instruments_ = SealInstruments(curve.instruments_);
                const auto definition =
                    MakeCurveDefinition(curve.curveName_, ccy, curve.parameterization_, curve.logDfScheme_, curve.knotDates_, today, basis);
                result.parameters_ = Bumps::Sum(result.parameters_, BuildCurveParameterLayout(definition).parameterCount_);
                result.quotes_ = Bumps::Sum(result.quotes_, curve.instruments_.size());
            }
            result.curves_ = std::move(curves);
            return result;
        }

        JointMultiCurveCalibrationSpec_ Seal(const JointMultiCurveCalibrationSpec_& spec) {
            RequireExact(spec.solveMode_);
            auto sealed = spec;
            auto declarations = SealDeclarations(spec.curves_, spec.today_, spec.ccy_, spec.liborBasis_);
            RequireSquare(declarations.parameters_, declarations.quotes_);
            sealed.curves_ = std::move(declarations.curves_);
            return sealed;
        }

        SealedDeclarations_ SealXccyDeclarations(const JointCurrencyCurveSpec_& currency, const Date_& today) {
            auto result = SealDeclarations(currency.curves_, today, currency.ccy_.String(), currency.liborBasis_);
            for (size_t index = 0; index < result.curves_.size(); ++index) {
                auto& name = result.curves_[index].curveName_;
                REQUIRE(!name.empty(), "RateCalibration: joint curve declarations require names");
                name = String_(std::to_string(index)) + ":" + name;
            }
            return result;
        }

        CrossCurrencyCalibrationSpec_ Seal(const CrossCurrencyCalibrationSpec_& spec) {
            RequireExact(spec.solveMode_);
            RequireCurrency(spec.basisPair_.domestic_);
            RequireCurrency(spec.basisPair_.foreign_);
            auto sealed = spec;
            if (spec.valuationTime_.IsValid()) {
                REQUIRE(!spec.today_.IsValid() || spec.today_ == spec.valuationTime_.Date(),
                        "RateCalibration: today must match the explicit valuation date");
                sealed.today_ = spec.valuationTime_.Date();
            } else {
                REQUIRE(spec.today_.IsValid(), "RateCalibration: today or an explicit valuation time required");
                sealed.valuationTime_ = DateTime_(spec.today_);
            }
            if (sealed.collateralCurrency_.Switch() == Ccy_::Value_::_NOT_SET)
                sealed.collateralCurrency_ = spec.basisPair_.domestic_;
            sealed.instruments_ = SealInstruments(spec.instruments_);
            RequireSquare(sealed.knotDates_.size(), sealed.instruments_.size());
            CurveSealer_ curves(sealed.today_);
            sealed.domesticCurveBlock_ = SealBlock(spec.domesticCurveBlock_, &curves);
            sealed.foreignCurveBlock_ = SealBlock(spec.foreignCurveBlock_, &curves);
            sealed.fixings_ = SealFixings(spec.fixings_, sealed.instruments_, sealed.valuationTime_);
            return sealed;
        }

        JointXccyCalibrationSpec_ Seal(const JointXccyCalibrationSpec_& spec) {
            RequireExact(spec.solveMode_);
            REQUIRE(spec.valuationTime_.IsValid(), "RateCalibration: a valid valuation time required");
            for (const auto& currency :
                 {spec.pair_.domestic_, spec.pair_.foreign_, spec.domestic_.ccy_, spec.foreign_.ccy_, spec.collateralCurrency_})
                RequireCurrency(currency);
            auto sealed = spec;
            const auto today = spec.valuationTime_.Date();
            auto domestic = SealXccyDeclarations(spec.domestic_, today);
            auto foreign = SealXccyDeclarations(spec.foreign_, today);
            const auto basis = MakeCurveDefinition(spec.basis_.curveName_, spec.pair_.domestic_.String(), spec.basis_.parameterization_,
                                                   spec.basis_.logDfScheme_, spec.basis_.knotDates_, today, DayBasis::Act365F());
            RequireSquare(Bumps::Sum(Bumps::Sum(domestic.parameters_, foreign.parameters_), BuildCurveParameterLayout(basis).parameterCount_),
                          Bumps::Sum(Bumps::Sum(domestic.quotes_, foreign.quotes_), spec.basis_.instruments_.size()));
            sealed.domestic_.curves_ = std::move(domestic.curves_);
            sealed.foreign_.curves_ = std::move(foreign.curves_);
            sealed.basis_.instruments_ = SealInstruments(spec.basis_.instruments_);
            sealed.fixings_ = SealFixings(spec.fixings_, sealed.basis_.instruments_, spec.valuationTime_);
            return sealed;
        }

        void ValidateInverseShape(const Matrix_<>& matrix, int count) {
            REQUIRE(matrix.Rows() == count && matrix.Cols() == count, "RateCalibration: native analytic Jacobian and inverse must be available");
        }

        void ValidateInverseInputs(const Matrix_<>& jacobian, const Matrix_<>& inverse, double tolerance, int count) {
            ValidateInverseShape(jacobian, count);
            ValidateInverseShape(inverse, count);
            REQUIRE(std::isfinite(tolerance) && tolerance > 0.0, "RateCalibration: invalid residual tolerance");
        }

        double InverseProduct(const Matrix_<>& jacobian, const Matrix_<>& inverse, double tolerance, int row, int column) {
            double value = 0.0;
            for (int inner = 0; inner < jacobian.Cols(); ++inner)
                value = std::fma(jacobian(row, inner), inverse(inner, column) / tolerance, value);
            return value;
        }

        void ValidateInverse(const Matrix_<>& jacobian, const Matrix_<>& inverse, double tolerance, int count) {
            ValidateInverseInputs(jacobian, inverse, tolerance, count);
            const double allowance = 256.0 * std::numeric_limits<double>::epsilon() * count;
            for (int row = 0; row < count; ++row) {
                for (int column = 0; column < count; ++column) {
                    const double value = InverseProduct(jacobian, inverse, tolerance, row, column);
                    REQUIRE(std::isfinite(value) && std::abs(value - (row == column ? 1.0 : 0.0)) <= allowance,
                            "RateCalibration: at-solution Jacobian inverse identity failed; locally singular or ill-conditioned system");
                }
            }
        }

        Matrix_<> RefineInverse(const Matrix_<>& jacobian, const Matrix_<>& inverse, double tolerance, int count) {
            ValidateInverseInputs(jacobian, inverse, tolerance, count);
            Matrix_<> residual(count, count);
            for (int row = 0; row < count; ++row)
                for (int column = 0; column < count; ++column)
                    residual(row, column) = (row == column ? 1.0 : 0.0) - InverseProduct(jacobian, inverse, tolerance, row, column);
            auto refined = inverse;
            for (int row = 0; row < count; ++row)
                for (int column = 0; column < count; ++column)
                    for (int inner = 0; inner < count; ++inner)
                        refined(row, column) = std::fma(inverse(row, inner), residual(inner, column), refined(row, column));
            return refined;
        }

        template <class T_> Vector_<> InstrumentQuotes(const Vector_<Handle_<T_>>& instruments) {
            Vector_<> quotes;
            for (const auto& instrument : instruments)
                quotes.push_back(instrument->MarketRate());
            return quotes;
        }

        template <class T_> Vector_<> Quotes(const T_& spec) { return InstrumentQuotes(spec.instruments_); }

        template <class T_> Vector_<> CurveQuotes(const T_& spec) {
            Vector_<> quotes;
            for (const auto& curve : spec.curves_)
                quotes.Append(InstrumentQuotes(curve.instruments_));
            return quotes;
        }

        Vector_<> Quotes(const JointMultiCurveCalibrationSpec_& spec) { return CurveQuotes(spec); }

        Vector_<> Quotes(const JointXccyCalibrationSpec_& spec) {
            auto quotes = CurveQuotes(spec.domestic_);
            quotes.Append(CurveQuotes(spec.foreign_));
            quotes.Append(InstrumentQuotes(spec.basis_.instruments_));
            return quotes;
        }

        template <class T_>
        Vector_<Handle_<T_>> RequoteInstruments(const Vector_<Handle_<T_>>& instruments, const Vector_<>& quotes, size_t* offset) {
            Vector_<Handle_<T_>> result;
            for (const auto& instrument : instruments)
                result.push_back(Requote(instrument, quotes[(*offset)++]));
            return result;
        }

        template <class T_> T_ WithQuotes(T_ spec, const Vector_<>& quotes) {
            size_t offset = 0;
            spec.instruments_ = RequoteInstruments(spec.instruments_, quotes, &offset);
            return spec;
        }

        Vector_<JointCurveDeclaration_> RequoteCurves(Vector_<JointCurveDeclaration_> curves, const Vector_<>& quotes, size_t* offset) {
            for (auto& curve : curves)
                curve.instruments_ = RequoteInstruments(curve.instruments_, quotes, offset);
            return curves;
        }

        JointMultiCurveCalibrationSpec_ WithQuotes(JointMultiCurveCalibrationSpec_ spec, const Vector_<>& quotes) {
            size_t offset = 0;
            spec.curves_ = RequoteCurves(std::move(spec.curves_), quotes, &offset);
            return spec;
        }

        JointXccyCalibrationSpec_ WithQuotes(JointXccyCalibrationSpec_ spec, const Vector_<>& quotes) {
            size_t offset = 0;
            spec.domestic_.curves_ = RequoteCurves(std::move(spec.domestic_.curves_), quotes, &offset);
            spec.foreign_.curves_ = RequoteCurves(std::move(spec.foreign_.curves_), quotes, &offset);
            spec.basis_.instruments_ = RequoteInstruments(spec.basis_.instruments_, quotes, &offset);
            return spec;
        }
    } // namespace

    struct RateCalibrationSnapshot_::Data_ {
        std::shared_ptr<const Source_> source_;
        Vector_<> point_;
        Vector_<> parameters_;
        RateQuoteRiskProvenance_ provenance_;
    };

    namespace {
        std::shared_ptr<const RateCalibrationSnapshot_::Data_> Calibrate(const std::shared_ptr<const Source_>& source,
                                                                         const CurveCalibrationSpec_& spec) {
            const CurveCalibrationOptions_ options;
            auto result = CalibrateYieldCurve(spec, options);
            const auto point = Quotes(spec);
            ValidateInverse(result.diagnostics_.jacobian_, result.diagnostics_.effJacobianInverse_, spec.tolerance_, static_cast<int>(point.size()));
            const auto parameters = InspectCurveParameters(*result.curve_, spec.today_).passiveParameters_;
            RatePricingMarket_ market;
            market.valuationTime_ = DateTime_(spec.today_);
            market.resultCurrency_ = Ccy_(spec.ccy_);
            market.curveComponents_[spec.curveName_] =
                Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), result.curve_.get()));
            RateQuoteRiskProvenanceConfig_ config{"rate-quote-curvature", {{spec.curveName_, spec.curveName_}}, true};
            auto provenance = BuildSingleCurveQuoteRiskProvenance(spec, result, options, market, config);
            REQUIRE(provenance.Available(), "RateCalibration: " + provenance.Reason());
            return std::make_shared<const RateCalibrationSnapshot_::Data_>(
                RateCalibrationSnapshot_::Data_{source, point, parameters, std::move(provenance)});
        }

        std::shared_ptr<const RateCalibrationSnapshot_::Data_> Calibrate(const std::shared_ptr<const Source_>& source,
                                                                         const JointMultiCurveCalibrationSpec_& spec) {
            JointMultiCurveCalibrationOptions_ options;
            options.computeEffJacobianInverse_ = true;
            const auto result = CalibrateJointMultiCurve(spec, options);
            const auto point = Quotes(spec);
            ValidateInverse(result.jacobianAtSolution_, result.effJacobianInverse_, spec.tolerance_, static_cast<int>(point.size()));
            RatePricingMarket_ market;
            market.valuationTime_ = DateTime_(spec.today_);
            market.resultCurrency_ = Ccy_(spec.ccy_);
            RateQuoteRiskProvenanceConfig_ config;
            config.calibrationId_ = "rate-quote-curvature";
            config.retainCalibrationRecord_ = true;
            Vector_<> parameters;
            for (int index = 0; index < static_cast<int>(spec.curves_.size()); ++index) {
                const auto& declaration = spec.curves_[index];
                const auto& curve = declaration.calibrateDiscountCurve_ ? result.discountCurves_.at(declaration.targetCollateral_)
                                                                        : result.forwardCurves_.at(declaration.targetTenor_);
                const String_ key = "curve:" + String::FromInt(index);
                market.curveComponents_[key] = curve;
                config.componentKeyByParameterBlock_[key] = key;
                parameters.Append(InspectCurveParameters(*curve, spec.today_).passiveParameters_);
            }
            auto provenance = BuildJointMultiCurveQuoteRiskProvenance(spec, result, options, market, config);
            REQUIRE(provenance.Available(), "RateCalibration: " + provenance.Reason());
            return std::make_shared<const RateCalibrationSnapshot_::Data_>(
                RateCalibrationSnapshot_::Data_{source, point, parameters, std::move(provenance)});
        }

        std::shared_ptr<const RateCalibrationSnapshot_::Data_> Calibrate(const std::shared_ptr<const Source_>& source,
                                                                         const CrossCurrencyCalibrationSpec_& spec) {
            const CrossCurrencyCalibrationOptions_ options;
            auto result = CalibrateCrossCurrencyMarket(spec, options);
            const auto point = Quotes(spec);
            result.diagnostics_.effJacobianInverse_ = RefineInverse(result.diagnostics_.jacobian_, result.diagnostics_.effJacobianInverse_,
                                                                    spec.tolerance_, static_cast<int>(point.size()));
            ValidateInverse(result.diagnostics_.jacobian_, result.diagnostics_.effJacobianInverse_, spec.tolerance_, static_cast<int>(point.size()));
            const auto& basis = result.basisCurves_.at(spec.basisPair_);
            const auto parameters = InspectCurveParameters(*basis, spec.today_).passiveParameters_;
            const String_ key = String_("basis:xccy_basis_") + spec.basisPair_.domestic_.String();
            RatePricingMarket_ market;
            market.valuationTime_ = spec.valuationTime_;
            market.resultCurrency_ = spec.basisPair_.domestic_;
            market.curveComponents_[key] = basis;
            market.fixings_ = result.market_.Fixings();
            market.xccyMarket_ = std::make_shared<CrossCurrencyMarket_>(result.market_);
            RateQuoteRiskProvenanceConfig_ config{"rate-quote-curvature", {{key, key}}, true};
            auto provenance = BuildStagedXccyBasisQuoteRiskProvenance(spec, result, options, market, config);
            REQUIRE(provenance.Available(), "RateCalibration: " + provenance.Reason());
            return std::make_shared<const RateCalibrationSnapshot_::Data_>(
                RateCalibrationSnapshot_::Data_{source, point, parameters, std::move(provenance)});
        }

        Vector_<> BindCurrency(const JointCurrencyCurveSpec_& currency,
                               const Handle_<CurveBlock_>& block,
                               const String_& group,
                               RatePricingMarket_* market,
                               RateQuoteRiskProvenanceConfig_* config) {
            Vector_<> parameters;
            for (const auto& declaration : currency.curves_) {
                const auto& curve = declaration.calibrateDiscountCurve_ ? block->Discount(declaration.targetCollateral_)
                                                                        : block->Forward(declaration.targetTenor_, declaration.targetCollateral_);
                const String_ key = group + ":" + declaration.curveName_;
                market->curveComponents_[key] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(block, &curve));
                config->componentKeyByParameterBlock_[key] = key;
                parameters.Append(InspectCurveParameters(curve, market->valuationTime_.Date()).passiveParameters_);
            }
            return parameters;
        }

        std::shared_ptr<const RateCalibrationSnapshot_::Data_> Calibrate(const std::shared_ptr<const Source_>& source,
                                                                         const JointXccyCalibrationSpec_& spec) {
            const JointXccyCalibrationOptions_ options;
            auto result = CalibrateJointXccyMarket(spec, options);
            const auto point = Quotes(spec);
            // The native weighted solve can leave roundoff in its square inverse.
            result.effJacobianInverse_ =
                RefineInverse(result.jacobianAtSolution_, result.effJacobianInverse_, spec.tolerance_, static_cast<int>(point.size()));
            ValidateInverse(result.jacobianAtSolution_, result.effJacobianInverse_, spec.tolerance_, static_cast<int>(point.size()));
            RatePricingMarket_ market;
            market.valuationTime_ = spec.valuationTime_;
            market.resultCurrency_ = spec.pair_.domestic_;
            market.fixings_ = result.fixings_;
            auto xccy = std::make_shared<CrossCurrencyMarket_>(result.domesticCurveBlock_, result.foreignCurveBlock_, spec.fxSpot_,
                                                               spec.valuationTime_, spec.collateralCurrency_, result.fixings_);
            xccy->SetBasisCurve(result.basisCurve_);
            market.xccyMarket_ = xccy;
            RateQuoteRiskProvenanceConfig_ config;
            config.calibrationId_ = "rate-quote-curvature";
            config.retainCalibrationRecord_ = true;
            auto parameters = BindCurrency(spec.domestic_, result.domesticCurveBlock_, "domestic", &market, &config);
            parameters.Append(BindCurrency(spec.foreign_, result.foreignCurveBlock_, "foreign", &market, &config));
            const String_ key = String_("basis:") + spec.basis_.curveName_;
            market.curveComponents_[key] = result.basisCurve_;
            config.componentKeyByParameterBlock_[key] = key;
            parameters.Append(InspectCurveParameters(*result.basisCurve_, spec.valuationTime_.Date()).passiveParameters_);
            auto provenance = BuildJointXccyQuoteRiskProvenance(spec, result, options, market, config);
            REQUIRE(provenance.Available(), "RateCalibration: " + provenance.Reason());
            return std::make_shared<const RateCalibrationSnapshot_::Data_>(
                RateCalibrationSnapshot_::Data_{source, point, parameters, std::move(provenance)});
        }

        std::shared_ptr<const RateCalibrationSnapshot_::Data_> CalibrateSource(const std::shared_ptr<const Source_>& source,
                                                                               const Source_& pointSpec) {
            auto* tape = AAD::Tape();
            const AAD::NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
            tape->multi_ = false;
            tape->numAdj_ = 1;
            return std::visit([&](const auto& spec) { return Calibrate(source, spec); }, pointSpec);
        }

        template <class T_> std::shared_ptr<const RateCalibrationSnapshot_::Data_> CaptureCalibration(const T_& spec) {
            AAD::RequireRecordingModeChangeAllowed();
            auto source = std::make_shared<const Source_>(Seal(spec));
            return CalibrateSource(source, *source);
        }

        struct Gradient_ {
            double value_;
            Vector_<> gradient_;
            RateCalibrationSnapshot_ calibration_;
            [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        };

        Gradient_ EvaluateGradient(const AAD::NativeScalarFunction_& objective,
                                   const RateCalibrationSnapshot_& original,
                                   const Vector_<>& point,
                                   const std::optional<size_t>& recordingCap,
                                   const String_& context,
                                   RateQuoteCurvatureExecution_* execution) {
            String_ stage = "calibration";
            try {
                const auto calibration = [&] {
                    AAD::TapeCapacityBudget_ budget(recordingCap.value_or(std::numeric_limits<size_t>::max()));
                    AAD::TapeCapacityScope_ capacity(&budget, true);
                    auto rebuilt = RecalibrateRateWithRisk(original, point);
                    execution->peakTapeBytes_ = std::max(execution->peakTapeBytes_, budget.PeakCapacityBytes());
                    execution->cleanupReserveBytes_ = std::max(execution->cleanupReserveBytes_, AAD::TapeCleanupCapacityBytes());
                    capacity.Close();
                    return rebuilt;
                }();
                auto objectivePoint = calibration.Parameters();
                objectivePoint.Append(calibration.Point());
                AAD::BumpOverAADRequest_ firstOrder;
                firstOrder.directions_ = Matrix_<>(0, static_cast<int>(objectivePoint.size()));
                firstOrder.recordingCapacityBudgetBytes_ = recordingCap;
                stage = "objective";
                const auto differentiated = AAD::EvaluateBumpOverAAD(objective, objectivePoint, firstOrder);
                execution->peakTapeBytes_ = std::max(execution->peakTapeBytes_, differentiated.Execution().peakTapeBytes_);
                execution->cleanupReserveBytes_ = std::max(execution->cleanupReserveBytes_, differentiated.Execution().cleanupReserveBytes_);
                const int count = static_cast<int>(point.size());
                Matrix_<> parameters(count, 1), direct(count, 1);
                std::copy_n(differentiated.Gradient().begin(), count, parameters.Data());
                std::copy_n(differentiated.Gradient().begin() + count, count, direct.Data());
                stage = "calibration pullback";
                const auto pullback = NewCalibrationPullback(calibration.Provenance());
                const auto risk = PullbackCalibration(pullback, NewCalibrationParameterAdjoints(pullback, parameters),
                                                      NewCalibrationDirectQuoteAdjoints(pullback, direct));
                return {differentiated.Value(), Vector_<>(risk.TotalAdjoints().begin(), risk.TotalAdjoints().end()), calibration};
            } catch (const std::exception& error) {
                THROW("RateQuoteCurvature: " + context + "; stage=" + stage + "; " + error.what());
            }
        }
    } // namespace

    const Vector_<>& RateCalibrationSnapshot_::Point() const { return data_->point_; }
    const Vector_<>& RateCalibrationSnapshot_::Parameters() const { return data_->parameters_; }
    const RateQuoteRiskProvenance_& RateCalibrationSnapshot_::Provenance() const { return data_->provenance_; }

    RateCalibrationSnapshot_ NewRateCalibration(const CurveCalibrationSpec_& spec) { return RateCalibrationSnapshot_(CaptureCalibration(spec)); }

    RateCalibrationSnapshot_ NewRateCalibration(const JointMultiCurveCalibrationSpec_& spec) {
        return RateCalibrationSnapshot_(CaptureCalibration(spec));
    }

    RateCalibrationSnapshot_ NewRateCalibration(const CrossCurrencyCalibrationSpec_& spec) {
        return RateCalibrationSnapshot_(CaptureCalibration(spec));
    }

    RateCalibrationSnapshot_ NewRateCalibration(const JointXccyCalibrationSpec_& spec) { return RateCalibrationSnapshot_(CaptureCalibration(spec)); }

    RateCalibrationSnapshot_ RecalibrateRateWithRisk(const RateCalibrationSnapshot_& calibration, const Vector_<>& quotes) {
        AAD::RequireRecordingModeChangeAllowed();
        REQUIRE(quotes.size() == calibration.Point().size(), "RateCalibration: exactly one value per raw quote required");
        for (double quote : quotes)
            REQUIRE(std::isfinite(quote), "RateCalibration: quotes must be finite");
        const auto& source = calibration.data_->source_;
        const Source_ pointSpec = std::visit([&](const auto& spec) -> Source_ { return WithQuotes(spec, quotes); }, *source);
        const auto rebuilt = CalibrateSource(source, pointSpec);
        REQUIRE(rebuilt->provenance_.Axis().fingerprint_ == calibration.Provenance().Axis().fingerprint_,
                "RateCalibration: quote or parameter axis changed during replay");
        return RateCalibrationSnapshot_(rebuilt);
    }

    RateQuoteCurvatureResult_ EvaluateRateQuoteCurvature(const AAD::NativeScalarFunction_& objective,
                                                         const RateCalibrationSnapshot_& calibration,
                                                         const AAD::BumpOverAADRequest_& request) {
        AAD::RequireRecordingModeChangeAllowed();
        const auto fixedObjective = objective;
        const auto fixedCalibration = calibration;
        auto fixedRequest = request;
        auto point = calibration.Point();
        REQUIRE(static_cast<bool>(fixedObjective), "RateQuoteCurvature: objective must be available");
        RateQuoteCurvatureExecution_ execution;
        execution.numericPayloadBytes_ = Bumps::Validate(point, fixedRequest);
        execution.quoteGradientEvaluations_ = Bumps::Sum(1, Bumps::Product(2, fixedRequest.steps_.size()));
        execution.calibrations_ = execution.quoteGradientEvaluations_;
        execution.objectiveReverseSweeps_ = execution.quoteGradientEvaluations_;
        auto* tape = AAD::Tape();
        const AAD::NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        auto result = Bumps::Evaluate(
            [&](const Vector_<>& inputs, const String_& context) {
                return EvaluateGradient(fixedObjective, fixedCalibration, inputs, fixedRequest.recordingCapacityBudgetBytes_, context, &execution);
            },
            point, fixedRequest);
        return {result.base_.value_,         std::move(result.base_.gradient_),    std::move(point),    std::move(fixedRequest),
                std::move(result.products_), std::move(result.base_.calibration_), std::move(execution)};
    }
} // namespace Dal
