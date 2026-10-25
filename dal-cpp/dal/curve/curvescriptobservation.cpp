#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/curvescript_internal.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/script/event.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal::CurveScriptInternal {
    using namespace CalibrationInternal;
    String_ ProjectionRoute(const Ccy_& currency, const PeriodLength_& tenor) {
        Token(currency, "UnsupportedCalibrationProduct", "/import/currency");
        Token(tenor, "UnsupportedCalibrationProduct", "/import/projection");
        Check(tenor.Months() == 3 || tenor.Months() == 6, "UnsupportedCalibrationProduct", "/import/projection", "canonical 3M/6M LCH route");
        return "IR:" + String_(currency.String()) + ",LIBOR_" + (tenor.Months() == 3 ? String_("3M") : String_("6M")) + "_LCH";
    }
    String_ ContextIdentity(const CalibrationPreparationContext_& context, const String_& base, const CalibrationDiagnostic_& diagnostic) {
        return Fingerprint(Obj({{"base", Str(base)},
                                {"libor_day_basis", Str(Token(Ccy::Conventions::LiborDayBasis()(context.currency_), "CalibrationContextMismatch",
                                                              "/context/conventions/libor_day_basis", diagnostic))}}));
    }
    namespace {
        CalibrationCurveObservation_
        DiscountGeometry(const Index::DF_& df, const CalibrationPreparationContext_& context, CalibrationCurveObservation_ result) {
            Check(df.ccy_ == context.currency_ && Cell::IsDate(df.maturity_) && (Cell::IsEmpty(df.start_) || Cell::IsDate(df.start_)),
                  "UnsupportedFastEvaluator", Field(result.source_), "same-currency DF with concrete dates", result.index_, result.source_);
            result.start_ = Cell::IsEmpty(df.start_) ? result.fixing_ : Cell::ToDate(df.start_);
            result.end_ = Cell::ToDate(df.maturity_);
            return result;
        }
        CalibrationCurveObservation_
        ProjectionGeometry(const Index::Libor_& libor, const CalibrationPreparationContext_& context, CalibrationCurveObservation_ result) {
            const auto& source = result.source_;
            Check(libor.ccy_ == context.currency_ && Cell::IsDate(libor.start_), "UnsupportedFastEvaluator", Field(source),
                  "explicit same-currency canonical Libor start", result.index_, source);
            Check(libor.tenor_ == TradedRate_::Value_::LIBOR_3M_LCH || libor.tenor_ == TradedRate_::Value_::LIBOR_6M_LCH, "UnsupportedFastEvaluator",
                  Field(source), "3M/6M LCH projection observation; no futures adjustment", result.index_, source);
            result.projection_ = true;
            result.tenor_ = libor.tenor_.Period();
            result.start_ = Cell::ToDate(libor.start_);
            result.end_ = Date::NominalMaturity(result.start_, result.tenor_, context.currency_);
            result.basis_ = Ccy::Conventions::LiborDayBasis()(context.currency_);
            result.route_ = ProjectionRoute(context.currency_, result.tenor_);
            return result;
        }
        CalibrationCurveObservation_
        IndexGeometry(const Script::NodeFix_& node, const CalibrationPreparationContext_& context, const CalibrationDiagnostic_& source) {
            CalibrationCurveObservation_ result;
            result.index_ = node.index_->Name();
            result.route_ = context.discountRoute_;
            result.fixing_ = node.fixingDate_.value_or(*source.eventDate_);
            result.source_ = source;
            const auto* df = dynamic_cast<const Index::DF_*>(node.index_.get());
            if (df)
                return DiscountGeometry(*df, context, std::move(result));
            const auto* libor = dynamic_cast<const Index::Libor_*>(node.index_.get());
            Check(libor != nullptr, "UnsupportedFastEvaluator", Field(source), "explicit same-currency DF or canonical Libor start", result.index_,
                  source);
            return ProjectionGeometry(*libor, context, std::move(result));
        }
        bool IsHistorical(const Date_& fixing, const CalibrationPreparationContext_& context) {
            const auto today = *context.valuation_.evaluationDate_;
            return fixing < today || (fixing == today && context.valuation_.todayFixingPolicy_ == TodayFixingPolicy_::Value_::REQUIREHISTORICAL);
        }
    } // namespace
    CalibrationCurveObservation_
    Observation(const Script::NodeFix_& node, const CalibrationPreparationContext_& context, const CalibrationDiagnostic_& source) {
        auto result = IndexGeometry(node, context, source);
        Check(result.fixing_ <= *source.eventDate_ && result.fixing_ <= result.start_ && result.start_ < result.end_, "UnsupportedFastEvaluator",
              Field(source), "fixing <= payment and fixing <= concrete start < end", result.index_, source);
        if (IsHistorical(result.fixing_, context)) {
            result.known_ = context.valuation_.fixings_->Find(result.index_, DateTime_(result.fixing_, 0.));
            Check(result.known_.has_value(), "MissingFixing", Field(source), "explicit historical fixing " + result.index_,
                  Date::ToString(result.fixing_), source);
            Check(std::isfinite(*result.known_) && (result.projection_ || *result.known_ > 0.), "CalibrationNumericalFailure", Field(source),
                  "finite historical rate or positive DF", NumberText(*result.known_), source);
        } else {
            Check(result.start_ >= *context.valuation_.evaluationDate_ && result.end_ <= context.curve_->nodeDates_.back(),
                  "CalibrationContextMismatch", Field(source), "live observation dates covered by frozen curve layout", result.index_, source);
        }
        return result;
    }
} // namespace Dal::CurveScriptInternal
