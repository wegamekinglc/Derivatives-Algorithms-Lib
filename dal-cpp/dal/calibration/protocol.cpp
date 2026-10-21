#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <map>
#include <set>

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/instrument.hpp>

namespace Dal {
#include <dal/auto/MG_CalibrationBindingOrigin_enum.inc>
#include <dal/auto/MG_CalibrationMeasureKind_enum.inc>
#include <dal/auto/MG_CalibrationQuoteConventionId_enum.inc>
#include <dal/auto/MG_CalibrationStage_enum.inc>
#include <dal/auto/MG_CalibrationUnitKind_enum.inc>
#include <dal/auto/MG_ScriptParameterRole_enum.inc>
#include <dal/auto/MG_ScriptParameterType_enum.inc>
#include <dal/auto/MG_ScriptSettlement_enum.inc>
#include <dal/auto/MG_ScriptSourceRowKind_enum.inc>
#include <dal/auto/MG_ScriptValuationCashflows_enum.inc>

    using namespace CalibrationInternal;
    namespace {
        const String_ QUOTE_ERROR = "InvalidCalibrationQuote", OBS_ERROR = "InvalidCalibrationObservation";
        CalibrationQuoteConventionId_ ConventionFor(const CalibrationValueUnit_& unit, const CalibrationDiagnostic_& context) {
            switch (unit.kind_.Switch()) {
            case CalibrationUnitKind_::Value_::Rate:
                return CalibrationQuoteConventionId_::Value_::DecimalRate;
            case CalibrationUnitKind_::Value_::BasisSpread:
                return CalibrationQuoteConventionId_::Value_::DecimalSpread;
            case CalibrationUnitKind_::Value_::Price:
                return CalibrationQuoteConventionId_::Value_::PresentValueAmount;
            case CalibrationUnitKind_::Value_::FxOutright:
                return CalibrationQuoteConventionId_::Value_::QuotePerBase;
            default:
                Fail(QUOTE_ERROR, "/unit", "a market quote unit", unit.Token(), context);
            }
        }
    } // namespace

    CalibrationValueUnit_ CalibrationValueUnit_::Parse(const String_& token) {
        const std::string text = Bytes(token);
        CalibrationValueUnit_ result;
        if (text.rfind("Price:", 0) == 0) {
            result.kind_ = CalibrationUnitKind_::Value_::Price;
            result.currency_ = Enum<Ccy_>(String_(text.substr(6)), QUOTE_ERROR, "/unit");
        } else if (text.rfind("FxOutright:", 0) == 0) {
            Check(text.size() == 18 && text[14] == '/', QUOTE_ERROR, "/unit", "FxOutright:AAA/BBB", token);
            result.kind_ = CalibrationUnitKind_::Value_::FxOutright;
            result.baseCurrency_ = Enum<Ccy_>(String_(text.substr(11, 3)), QUOTE_ERROR, "/unit");
            result.quoteCurrency_ = Enum<Ccy_>(String_(text.substr(15, 3)), QUOTE_ERROR, "/unit");
        } else {
            result.kind_ = Enum<CalibrationUnitKind_>(token, QUOTE_ERROR, "/unit");
        }
        Check(Bytes(result.Token()) == text, QUOTE_ERROR, "/unit", "a canonical unit token", token);
        return result;
    }
    String_ CalibrationValueUnit_::Token() const {
        const String_ kind = CalibrationInternal::Token(kind_, QUOTE_ERROR, "/unit");
        const bool price = kind_ == CalibrationUnitKind_::Value_::Price, fx = kind_ == CalibrationUnitKind_::Value_::FxOutright;
        Check(currency_.has_value() == price && baseCurrency_.has_value() == fx && quoteCurrency_.has_value() == fx, QUOTE_ERROR, "/unit",
              "currency fields matching unit kind", kind);
        if (price)
            return kind + ":" + CalibrationInternal::Token(*currency_, QUOTE_ERROR, "/unit");
        if (fx) {
            Check(*baseCurrency_ != *quoteCurrency_, QUOTE_ERROR, "/unit", "distinct base and quote currencies", kind);
            return kind + ":" + CalibrationInternal::Token(*baseCurrency_, QUOTE_ERROR, "/unit") + "/" +
                   CalibrationInternal::Token(*quoteCurrency_, QUOTE_ERROR, "/unit");
        }
        return kind;
    }
    bool operator==(const CalibrationValueUnit_& lhs, const CalibrationValueUnit_& rhs) { return Bytes(lhs.Token()) == Bytes(rhs.Token()); }

    namespace CalibrationInternal {
        Json_ QuoteJson(const CalibrationMarketQuote_& quote) {
            return Obj({{"id", Str(quote.Id())},
                        {"value", Num(quote.Value())},
                        {"unit", Str(quote.Unit().Token())},
                        {"convention", Obj({{"id", Str(Token(quote.Convention().id_, QUOTE_ERROR, "/convention/id"))},
                                            {"version", Num(quote.Convention().version_)}})}});
        }
    } // namespace CalibrationInternal
    CalibrationMarketQuote_::CalibrationMarketQuote_(String_ id, double value, CalibrationValueUnit_ unit, CalibrationQuoteConvention_ convention)
        : id_(std::move(id)), value_(Normalize(value)), unit_(std::move(unit)), convention_(convention) {
        fingerprint_ = CalibrationInternal::Fingerprint(QuoteJson(*this));
    }
    Handle_<CalibrationMarketQuote_> NewCalibrationMarketQuote(const String_& id,
                                                               double value,
                                                               const CalibrationValueUnit_& unit,
                                                               const std::optional<CalibrationQuoteConvention_>& convention) {
        CalibrationDiagnostic_ context;
        context.quoteId_ = id;
        Check(ValidId(id), QUOTE_ERROR, "/id", "a stable case-sensitive ID", id, context);
        Check(std::isfinite(value), QUOTE_ERROR, "/value", "finite number", NumberText(value), context);
        try {
            static_cast<void>(unit.Token());
        } catch (const CalibrationError_&) {
            Fail(QUOTE_ERROR, "/unit", "a valid typed unit", "", context);
        }
        const auto expected = ConventionFor(unit, context);
        const auto resolved = convention.value_or(CalibrationQuoteConvention_{expected, 1});
        Check(resolved.id_ == expected && resolved.version_ == 1, QUOTE_ERROR, "/convention", "unit-compatible convention version 1", "", context);
        if (unit.kind_ == CalibrationUnitKind_::Value_::FxOutright)
            Check(value > 0., QUOTE_ERROR, "/value", "strictly positive FX quote", NumberText(value), context);
        return Handle_<CalibrationMarketQuote_>(new CalibrationMarketQuote_(id, value, unit, resolved));
    }
    CalibrationObservation_::CalibrationObservation_(
        String_ id, Handle_<CalibrationInstrument_> instrument, String_ measureId, Handle_<CalibrationMarketQuote_> quote, double errorScale)
        : id_(std::move(id)), measureId_(std::move(measureId)), instrument_(std::move(instrument)), quote_(std::move(quote)),
          errorScale_(errorScale) {}
    Handle_<CalibrationObservation_> NewCalibrationObservation(const String_& id,
                                                               const Handle_<CalibrationInstrument_>& instrument,
                                                               const String_& measureId,
                                                               const Handle_<CalibrationMarketQuote_>& quote,
                                                               double errorScale) {
        CalibrationDiagnostic_ context;
        context.observationId_ = id;
        context.measureId_ = measureId;
        if (instrument)
            context.instrumentId_ = instrument->Id();
        if (quote)
            context.quoteId_ = quote->Id();
        Check(ValidId(id), OBS_ERROR, "/id", "a stable case-sensitive ID", id, context);
        Check(bool(instrument), OBS_ERROR, "/instrument", "nonnull instrument", "", context);
        Check(bool(quote), OBS_ERROR, "/quote", "nonnull quote", "", context);
        Check(std::isfinite(errorScale) && errorScale > 0., OBS_ERROR, "/error_scale", "finite strictly positive scale", NumberText(errorScale),
              context);
        const auto& measures = instrument->Definition()->Spec().measures_;
        const auto found = std::find_if(measures.begin(), measures.end(), [&](const auto& m) { return Bytes(m.id_) == Bytes(measureId); });
        Check(found != measures.end(), "UnsupportedCalibrationMeasure", "/measure_id", "a declared measure ID", measureId, context);
        Check(found->unit_ == quote->Unit(), OBS_ERROR, "/quote/unit", found->unit_.Token(), quote->Unit().Token(), context);
        return Handle_<CalibrationObservation_>(new CalibrationObservation_(id, instrument, measureId, quote, errorScale));
    }
    CalibrationObservationAxes_ BuildCalibrationObservationAxes(const Vector_<Handle_<CalibrationObservation_>>& observations) {
        Check(observations.size() <= INT_MAX, OBS_ERROR, "/observations", "at most 2^31-1 rows");
        CalibrationObservationAxes_ result;
        std::set<std::string> ids;
        std::map<std::string, int> quotes;
        std::map<std::string, String_> instruments;
        for (size_t i = 0; i < observations.size(); ++i) {
            const auto& row = observations[i];
            const String_ path = "/observations/" + OrdinalText(i);
            Check(bool(row), OBS_ERROR, path, "nonnull observation");
            CalibrationDiagnostic_ context;
            context.observationId_ = row->Id();
            context.instrumentId_ = row->Instrument()->Id();
            context.quoteId_ = row->Quote()->Id();
            context.measureId_ = row->MeasureId();
            Check(ids.insert(Bytes(row->Id())).second, OBS_ERROR, path + "/id", "unique observation ID", row->Id(), context);
            const auto instrument = instruments.emplace(Bytes(row->Instrument()->Id()), row->Instrument()->InstrumentIdentity());
            Check(instrument.first->second == row->Instrument()->InstrumentIdentity(), OBS_ERROR, path + "/instrument/id",
                  "identical definition/binding/pillar for repeated instrument ID", row->Instrument()->Id(), context);
            const auto quote = quotes.emplace(Bytes(row->Quote()->Id()), static_cast<int>(result.quotes_.size()));
            if (quote.second)
                result.quotes_.push_back(row->Quote());
            else
                Check(Jcs(QuoteJson(*result.quotes_[quote.first->second])) == Jcs(QuoteJson(*row->Quote())), OBS_ERROR, path + "/quote/id",
                      "identical quote data for repeated quote ID", row->Quote()->Id(), context);
            result.quoteIndex_.push_back(quote.first->second);
        }
        result.observations_ = observations;
        result.quoteMap_.Resize(static_cast<int>(observations.size()), static_cast<int>(result.quotes_.size()));
        result.quoteMap_.Fill(0.);
        for (size_t i = 0; i < observations.size(); ++i)
            result.quoteMap_(static_cast<int>(i), result.quoteIndex_[i]) = 1.;
        return result;
    }
    CalibrationReport_::CalibrationReport_(CalibrationStage_ stage, CalibrationObservationAxes_ axes, Vector_<CalibrationDiagnostic_> errors)
        : Storable_("CalibrationReport", ""), stage_(stage), axes_(std::move(axes)), errors_(std::move(errors)) {}
    void CalibrationReport_::Write(Archive::Store_&) const { Fail("CalibrationPersistenceUnsupported", "", "instrument persistence only"); }
    CalibrationError_::CalibrationError_(const std::string& file,
                                         long line,
                                         const std::string& function,
                                         std::shared_ptr<const CalibrationFailure_> failure)
        : Exception_(file, line, function, failure ? failure->diagnostic_.message_ : String_("CalibrationNumericalFailure: missing payload")),
          failure_(failure ? std::make_shared<const CalibrationFailure_>(*failure) : nullptr) {
        REQUIRE(failure_ && failure_->report_, "CalibrationError requires an owning report");
    }
} // namespace Dal
