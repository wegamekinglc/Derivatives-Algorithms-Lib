#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <map>
#include <set>
#include <stdexcept>

#include <dal/script/event.hpp>
#include <dal/script/instrument_internal.hpp>
#include <dal/script/parser.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    namespace {
        const String_ DEF_ERROR = "InvalidScriptInstrumentDefinition", BIND_ERROR = "InvalidCalibrationParameterBinding";
        bool IsInputParseError(const std::exception& error) {
            return dynamic_cast<const Exception_*>(&error) || dynamic_cast<const std::invalid_argument*>(&error) ||
                   dynamic_cast<const std::out_of_range*>(&error);
        }
        [[noreturn]] void ParseFailure(const std::exception& error, const Script::ParseContext_& parsed, CalibrationDiagnostic_ context) {
            String_ path = "/source/rows";
            if (parsed.source_.row_ != 0 && parsed.source_.row_ <= INT_MAX) {
                context.sourceRow_ = static_cast<int>(parsed.source_.row_);
                context.eventDate_ = parsed.source_.eventDate_;
                if (parsed.statement_ <= INT_MAX)
                    context.statement_ = static_cast<int>(parsed.statement_);
                path += "/" + OrdinalText(parsed.source_.row_ - 1) + "/text";
            }
            Fail(DEF_ERROR, path, "parseable bound Price source", String_(error.what()), context);
        }
        std::unique_ptr<Script::ScriptProduct_>
        BoundProduct(const ScriptInstrumentDefinitionSpec_& spec, const std::map<String_, double>& slots, const CalibrationDiagnostic_& context) {
            Vector_<Cell_> dates;
            Vector_<String_> events;
            for (const auto& row : spec.source_.rows_) {
                dates.push_back(row.kind_ == ScriptSourceRowKind_::Value_::Definition
                                    ? Cell_(row.key_)
                                    : Cell_(ParseDate(row.key_, DEF_ERROR, "/source/rows/key", context)));
                events.push_back(row.text_);
            }
            Script::ParseContext_ parsed;
            std::unique_ptr<Script::ScriptProduct_> product;
            try {
                product = std::make_unique<Script::ScriptProduct_>(dates, events, spec.header_.receiver_, slots, &parsed);
            } catch (const std::exception& error) {
                if (!IsInputParseError(error))
                    throw;
                ParseFailure(error, parsed, context);
            }
            Check(product->HasPays(), "NoSharedPriceDefinition", "/source/rows", "shared PAYS cashflows", "", context);
            return product;
        }
        double SourceNumber(const String_& text, const String_& path, const CalibrationDiagnostic_& context) {
            double result = 0.;
            try {
                result = String::ToDouble(text);
            } catch (const std::exception&) {
                Fail(DEF_ERROR, path, "finite numeric source default", text, context);
            }
            Check(std::isfinite(result), DEF_ERROR, path, "finite numeric source default", text, context);
            return Normalize(result);
        }
        void Unit(const CalibrationValueUnit_& unit, const String_& code, const String_& path, const CalibrationDiagnostic_& context) {
            try {
                static_cast<void>(unit.Token());
            } catch (const CalibrationError_&) {
                Fail(code, path, "a valid typed unit", "", context);
            }
        }
        void ValidateHeader(const ScriptInstrumentHeader_& header) {
            Token(header.payCurrency_, DEF_ERROR, "/header/pay_currency");
            Check(header.resultCurrency_ == header.payCurrency_ && header.collateralCurrency_ == header.payCurrency_, DEF_ERROR, "/header",
                  "matching pay/result/collateral currencies");
            Check(header.collateralType_ == CollateralType_::Value_::OIS, DEF_ERROR, "/header/collateral_type", "OIS");
            Check(Bytes(header.receiver_) == "PAY", DEF_ERROR, "/header/receiver", "PAY", header.receiver_);
            Check(Bytes(header.notionalParameter_) == "N", DEF_ERROR, "/header/notional_parameter", "N", header.notionalParameter_);
            Check(header.settlement_ == ScriptSettlement_::Value_::AtEvent, DEF_ERROR, "/header/settlement", "AtEvent");
            Check(header.valuationCashflows_ == ScriptValuationCashflows_::Value_::IncludeUnsettledAtValuation, DEF_ERROR,
                  "/header/valuation_cashflows", "IncludeUnsettledAtValuation");
            Text(header.discountRoute_, DEF_ERROR, "/header/discount_route");
            Check(!header.discountRoute_.empty(), DEF_ERROR, "/header/discount_route", "a nonempty explicit route");
            Check(Bytes(header.conventionSet_) == "dal.fixed-dates/1", DEF_ERROR, "/header/convention_set", "dal.fixed-dates/1");
        }
        std::map<std::string, double> SourceDefaults(const ScriptInstrumentSource_& source) {
            Check(source.settings_.defaultIndex_.empty() && source.settings_.regressionFeatures_.empty(), DEF_ERROR, "/source/settings",
                  "empty default_index and regression_features in P0");
            Check(!source.rows_.empty() && source.rows_.size() <= INT_MAX, DEF_ERROR, "/source/rows", "nonempty ordered source");
            std::map<std::string, double> defaults;
            bool events = false;
            for (size_t i = 0; i < source.rows_.size(); ++i) {
                const auto& row = source.rows_[i];
                CalibrationDiagnostic_ context;
                context.sourceRow_ = static_cast<int>(i + 1);
                const String_ path = "/source/rows/" + OrdinalText(i);
                Text(row.text_, DEF_ERROR, path + "/text", context);
                Check(!row.text_.empty(), DEF_ERROR, path + "/text", "nonempty source", "", context);
                if (row.kind_ == ScriptSourceRowKind_::Value_::Definition) {
                    context.parameterKey_ = row.key_;
                    Check(!events && ValidKey(row.key_), DEF_ERROR, path + "/key", "canonical numeric definitions before events", row.key_, context);
                    Check(defaults.emplace(Bytes(row.key_), SourceNumber(row.text_, path + "/text", context)).second, DEF_ERROR, path + "/key",
                          "unique source definition", row.key_, context);
                } else {
                    Check(row.kind_ == ScriptSourceRowKind_::Value_::Event, DEF_ERROR, path + "/kind", "Definition or Event", "", context);
                    static_cast<void>(ParseDate(row.key_, DEF_ERROR, path + "/key", context));
                    events = true;
                }
            }
            Check(events, "NoSharedPriceDefinition", "/source/rows", "dated PAYS cashflows");
            return defaults;
        }
        bool ParameterRoleMatches(const ScriptParameterDeclaration_& d) {
            if (Bytes(d.key_) == "N")
                return d.role_ == ScriptParameterRole_::Value_::Notional;
            if (Bytes(d.key_) == "ALPHA")
                return d.role_ == ScriptParameterRole_::Value_::Accrual;
            return d.role_ == ScriptParameterRole_::Value_::Coupon || d.role_ == ScriptParameterRole_::Value_::Strike;
        }
        String_ ParameterUnit(const ScriptParameterDeclaration_& d, const ScriptInstrumentHeader_& header) {
            if (Bytes(d.key_) == "N")
                return "Price:" + String_(header.payCurrency_.String());
            if (Bytes(d.key_) == "ALPHA")
                return "YearFraction";
            return d.role_ == ScriptParameterRole_::Value_::Coupon ? "Rate" : "BondPrice";
        }
        bool NonemptyDomain(const ScriptParameterDeclaration_& d) {
            return !d.lower_ || !d.upper_ || *d.lower_ < *d.upper_ || (*d.lower_ == *d.upper_ && d.lowerInclusive_ && d.upperInclusive_);
        }
        void ValidateDomain(const ScriptParameterDeclaration_& d, const String_& path, const CalibrationDiagnostic_& context) {
            Check((!d.lower_ || std::isfinite(*d.lower_)) && (!d.upper_ || std::isfinite(*d.upper_)), DEF_ERROR, path, "finite bounds or null", "",
                  context);
            Check((d.lower_ || d.lowerInclusive_) && (d.upper_ || d.upperInclusive_), DEF_ERROR, path, "null bounds must be inclusive", "", context);
            Check(NonemptyDomain(d), DEF_ERROR, path, "nonempty domain", "", context);
        }
        void ValidateDeclaration(const ScriptParameterDeclaration_& d,
                                 const ScriptInstrumentHeader_& header,
                                 const String_& path,
                                 const CalibrationDiagnostic_& context) {
            Check(d.type_ == ScriptParameterType_::Value_::Real, DEF_ERROR, path + "/type", "Real", "", context);
            Unit(d.unit_, DEF_ERROR, path + "/unit", context);
            const bool n = Bytes(d.key_) == "N", k = Bytes(d.key_) == "K", alpha = Bytes(d.key_) == "ALPHA";
            Check(n || k || alpha, DEF_ERROR, path + "/key", "P0 slots N, K, ALPHA", d.key_, context);
            Check(ParameterRoleMatches(d), DEF_ERROR, path + "/role", "role matching parameter slot", "", context);
            Check(d.bindable_ == (n || k), DEF_ERROR, path + "/bindable", "only N/K bindable", "", context);
            const auto expected = ParameterUnit(d, header);
            Check(Bytes(d.unit_.Token()) == Bytes(expected), DEF_ERROR, path + "/unit", expected, d.unit_.Token(), context);
            ValidateDomain(d, path, context);
        }
        Vector_<> ValidateParameters(const ScriptInstrumentDefinitionSpec_& spec, const std::map<std::string, double>& defaults, bool coupon) {
            Check(spec.parameters_.size() == defaults.size(), DEF_ERROR, "/parameters", "every numeric definition declared exactly once");
            Vector_<> values;
            std::set<std::string> keys;
            for (size_t i = 0; i < spec.parameters_.size(); ++i) {
                const auto& d = spec.parameters_[i];
                CalibrationDiagnostic_ context;
                context.parameterKey_ = d.key_;
                context.sourceRow_ = static_cast<int>(i + 1);
                const String_ path = "/parameters/" + OrdinalText(i);
                Check(ValidKey(d.key_) && keys.insert(Bytes(d.key_)).second && defaults.count(Bytes(d.key_)) != 0, DEF_ERROR, path + "/key",
                      "unique declared numeric slot", d.key_, context);
                Check(Bytes(spec.source_.rows_[i].key_) == Bytes(d.key_), DEF_ERROR, path + "/key", "source-definition order", d.key_, context);
                ValidateDeclaration(d, spec.header_, path, context);
                const double value = defaults.at(Bytes(d.key_));
                ValidateParameter(value, d, "/source/rows/" + OrdinalText(i) + "/text", context, DEF_ERROR);
                values.push_back(value);
            }
            Check(keys.count("N") != 0 && keys.count("K") != 0, DEF_ERROR, "/parameters", "N and K declarations");
            Check(keys.count("ALPHA") == static_cast<size_t>(coupon), DEF_ERROR, "/parameters", "ALPHA for Deposit coupon only");
            return values;
        }
        void ValidatePriceMeasure(const CalibrationMeasureSpec_& m,
                                  bool price,
                                  const ScriptInstrumentHeader_& header,
                                  const String_& path,
                                  const CalibrationDiagnostic_& context) {
            Check(!price && Bytes(m.id_) == "Price" && !m.parameter_ && !m.annuitySign_ && !m.minAbsAnnuity_, DEF_ERROR, path,
                  "one Price measure with null parameter/annuity fields", "", context);
            Check(Bytes(m.unit_.Token()) == "Price:" + Bytes(Token(header.payCurrency_, DEF_ERROR, path)), DEF_ERROR, path + "/unit",
                  "Price in pay currency", m.unit_.Token(), context);
        }
        bool ParRateParameter(const CalibrationMeasureSpec_& m) { return Bytes(m.id_) == "ParRate" && m.parameter_ && Bytes(*m.parameter_) == "K"; }
        bool PositiveAnnuity(const CalibrationMeasureSpec_& m) {
            return m.annuitySign_ && *m.annuitySign_ == 1 && m.minAbsAnnuity_ && std::isfinite(*m.minAbsAnnuity_) && *m.minAbsAnnuity_ > 0.;
        }
        void ValidateParMeasure(const CalibrationMeasureSpec_& m, bool coupon, const String_& path, const CalibrationDiagnostic_& context) {
            Check(m.kind_ == CalibrationMeasureKind_::Value_::ParAffineRate && coupon, "UnsupportedCalibrationMeasure", path + "/kind",
                  "P0 Price or Deposit ParAffineRate", "", context);
            Check(ParRateParameter(m) && PositiveAnnuity(m) && Bytes(m.unit_.Token()) == "Rate", DEF_ERROR, path,
                  "ParRate(K), Rate, positive annuity floor and sign +1", "", context);
        }
        void ValidateMeasures(const ScriptInstrumentDefinitionSpec_& spec, bool coupon) {
            const auto& header = spec.header_;
            std::set<std::string> measures;
            bool price = false;
            for (size_t i = 0; i < spec.measures_.size(); ++i) {
                const auto& m = spec.measures_[i];
                CalibrationDiagnostic_ context;
                context.measureId_ = m.id_;
                const String_ path = "/measures/" + OrdinalText(i);
                Check(ValidId(m.id_) && measures.insert(Bytes(m.id_)).second, DEF_ERROR, path + "/id", "unique measure ID", m.id_, context);
                Check(Bytes(m.receiver_) == Bytes(header.receiver_), DEF_ERROR, path + "/receiver", header.receiver_, m.receiver_, context);
                Unit(m.unit_, DEF_ERROR, path + "/unit", context);
                if (m.kind_ == CalibrationMeasureKind_::Value_::Price) {
                    ValidatePriceMeasure(m, price, header, path, context);
                    price = true;
                } else {
                    ValidateParMeasure(m, coupon, path, context);
                }
            }
            Check(price, "NoSharedPriceDefinition", "/measures", "exactly one Price measure");
        }
        Vector_<> ValidateDefinition(const ScriptInstrumentDefinitionSpec_& spec) {
            ValidateHeader(spec.header_);
            const auto defaults = SourceDefaults(spec.source_);
            const bool coupon = std::any_of(spec.parameters_.begin(), spec.parameters_.end(),
                                            [](const auto& d) { return d.role_ == ScriptParameterRole_::Value_::Coupon; });
            const auto values = ValidateParameters(spec, defaults, coupon);
            ValidateMeasures(spec, coupon);
            return values;
        }
        std::map<std::string, CalibrationQuoteCopy_> QuoteCopies(const Vector_<CalibrationQuoteCopy_>& quoteCopies, const String_& id) {
            std::map<std::string, CalibrationQuoteCopy_> copies;
            for (size_t i = 0; i < quoteCopies.size(); ++i) {
                const auto& copy = quoteCopies[i];
                CalibrationDiagnostic_ context;
                context.instrumentId_ = id;
                context.parameterKey_ = copy.parameterKey_;
                context.quoteId_ = copy.quoteId_;
                Check(ValidId(copy.quoteId_) && ValidFingerprint(copy.quoteFingerprint_) && copies.emplace(Bytes(copy.parameterKey_), copy).second,
                      BIND_ERROR, "/bindings/quote_copies/" + OrdinalText(i), "one valid quote ID/fingerprint per updated parameter", "", context);
            }
            return copies;
        }
        CalibrationEffectiveParameter_ WithBindingOrigin(CalibrationEffectiveParameter_ value, const CalibrationQuoteCopy_* copy) {
            value.origin_ = copy ? CalibrationBindingOrigin_::Value_::QuoteCopy : CalibrationBindingOrigin_::Value_::Literal;
            value.quoteId_ = copy ? std::optional<String_>(copy->quoteId_) : std::nullopt;
            value.quoteFingerprint_ = copy ? std::optional<String_>(copy->quoteFingerprint_) : std::nullopt;
            return value;
        }
        Vector_<CalibrationEffectiveParameter_> ApplyBindings(const ScriptInstrumentDefinition_& definition,
                                                              Vector_<CalibrationEffectiveParameter_> values,
                                                              const CalibrationParameterBindings_& updates,
                                                              const String_& id) {
            std::map<std::string, size_t> slots;
            const auto& declarations = definition.Spec().parameters_;
            for (size_t i = 0; i < declarations.size(); ++i)
                slots.emplace(Bytes(declarations[i].key_), i);
            std::set<std::string> seen;
            const auto copies = QuoteCopies(updates.quoteCopies_, id);
            for (size_t i = 0; i < updates.values_.size(); ++i) {
                const auto& update = updates.values_[i];
                CalibrationDiagnostic_ context;
                context.instrumentId_ = id;
                context.parameterKey_ = update.key_;
                const String_ path = "/bindings/values/" + OrdinalText(i);
                const auto found = slots.find(Bytes(update.key_));
                Check(ValidKey(update.key_) && found != slots.end() && seen.insert(Bytes(update.key_)).second, BIND_ERROR, path + "/key",
                      "unique known canonical parameter", update.key_, context);
                const auto slot = found->second;
                const auto& d = declarations[slot];
                Check(d.bindable_, BIND_ERROR, path + "/key", "a bindable N/K slot", update.key_, context);
                Unit(update.unit_, BIND_ERROR, path + "/unit", context);
                Check(update.unit_ == d.unit_, BIND_ERROR, path + "/unit", d.unit_.Token(), update.unit_.Token(), context);
                ValidateParameter(update.value_, d, path + "/value", context);
                auto& value = values[slot];
                value.value_ = Normalize(update.value_);
                const auto copy = copies.find(Bytes(update.key_));
                value = WithBindingOrigin(std::move(value), copy == copies.end() ? nullptr : &copy->second);
            }
            for (const auto& copy : copies) {
                CalibrationDiagnostic_ context;
                context.instrumentId_ = id;
                context.parameterKey_ = String_(copy.first);
                Check(seen.count(copy.first) != 0, BIND_ERROR, "/bindings/quote_copies", "a matching numeric update", "", context);
            }
            return values;
        }
        bool InLowerDomain(double value, const ScriptParameterDeclaration_& d) {
            return !d.lower_ || (d.lowerInclusive_ ? value >= *d.lower_ : value > *d.lower_);
        }
        bool InUpperDomain(double value, const ScriptParameterDeclaration_& d) {
            return !d.upper_ || (d.upperInclusive_ ? value <= *d.upper_ : value < *d.upper_);
        }
        String_ DomainText(const ScriptParameterDeclaration_& d) {
            return String_(d.lowerInclusive_ ? "[" : "(") + (d.lower_ ? NumberText(*d.lower_) : String_("-inf")) + "," +
                   (d.upper_ ? NumberText(*d.upper_) : String_("+inf")) + (d.upperInclusive_ ? "]" : ")");
        }
    } // namespace
    namespace CalibrationInternal {
        void ValidateParameter(
            double value, const ScriptParameterDeclaration_& d, const String_& path, const CalibrationDiagnostic_& context, const String_& code) {
            const bool lower = InLowerDomain(value, d);
            const bool upper = InUpperDomain(value, d);
            const bool positive = d.role_ != ScriptParameterRole_::Value_::Coupon;
            const String_ domain = DomainText(d);
            Check(std::isfinite(value) && lower && upper && (!positive || value > 0.), code, path,
                  "finite " + d.unit_.Token() + " in " + domain + (positive ? "; strictly positive" : ""), NumberText(value), context);
        }
    } // namespace CalibrationInternal
    ScriptInstrumentDefinition_::ScriptInstrumentDefinition_(ScriptInstrumentDefinitionSpec_ spec)
        : Storable_("ScriptInstrumentDefinition", ""), spec_(std::move(spec)), defaults_(ValidateDefinition(spec_)) {
        for (auto& d : spec_.parameters_) {
            if (d.lower_)
                d.lower_ = Normalize(*d.lower_);
            if (d.upper_)
                d.upper_ = Normalize(*d.upper_);
        }
        std::map<String_, double> defaults;
        for (size_t i = 0; i < spec_.parameters_.size(); ++i)
            defaults.emplace(spec_.parameters_[i].key_, defaults_[i]);
        static_cast<void>(BoundProduct(spec_, defaults, {}));
        sourceIdentity_ = Fingerprint(SourceJson(spec_.source_));
        definitionIdentity_ = Fingerprint(DefinitionJson(spec_));
    }
    Handle_<ScriptInstrumentDefinition_> NewScriptInstrumentDefinition(const ScriptInstrumentDefinitionSpec_& spec) {
        return Handle_<ScriptInstrumentDefinition_>(new ScriptInstrumentDefinition_(spec));
    }
    std::unique_ptr<ScriptInstrumentDefinition_> ScriptInstrumentDefinition_::Clone() const {
        return std::unique_ptr<ScriptInstrumentDefinition_>(new ScriptInstrumentDefinition_(spec_));
    }
    CalibrationInstrument_::CalibrationInstrument_(String_ id,
                                                   Handle_<ScriptInstrumentDefinition_> definition,
                                                   Vector_<CalibrationEffectiveParameter_> binding,
                                                   std::optional<Date_> pillar)
        : Storable_("CalibrationInstrument", id), id_(std::move(id)), definition_(std::move(definition)), binding_(std::move(binding)),
          pillar_(pillar) {
        CalibrationDiagnostic_ context;
        context.instrumentId_ = id_;
        Check(ValidId(id_), BIND_ERROR, "/instrument_id", "a stable case-sensitive ID", id_, context);
        Check(bool(definition_), BIND_ERROR, "/definition", "nonnull immutable definition", "", context);
        Check(!pillar_ || pillar_->IsValid(), BIND_ERROR, "/pillar", "valid date or null", "", context);
        bindingIdentity_ =
            Fingerprint(Obj({{"definition_identity", Str(definition_->DefinitionIdentity())}, {"values", BindingValuesJson(binding_, false)}}));
        instrumentIdentity_ = Fingerprint(Obj({{"instrument_id", Str(id_)},
                                               {"definition_identity", Str(definition_->DefinitionIdentity())},
                                               {"binding_identity", Str(bindingIdentity_)},
                                               {"pillar", Optional(pillar_, [](const Date_& d) { return Str(Date::ToString(d)); })}}));
    }
    Handle_<CalibrationInstrument_> NewScriptCalibrationInstrument(const ScriptCalibrationInstrumentSpec_& spec) {
        Check(bool(spec.definition_), BIND_ERROR, "/definition", "nonnull immutable definition");
        Vector_<CalibrationEffectiveParameter_> binding;
        const auto& declarations = spec.definition_->Spec().parameters_;
        for (size_t i = 0; i < declarations.size(); ++i) {
            CalibrationEffectiveParameter_ row;
            row.key_ = declarations[i].key_;
            row.unit_ = declarations[i].unit_;
            row.value_ = spec.definition_->Defaults()[i];
            binding.push_back(row);
        }
        return Handle_<CalibrationInstrument_>(new CalibrationInstrument_(
            spec.id_, spec.definition_, ApplyBindings(*spec.definition_, std::move(binding), spec.bindings_, spec.id_), spec.pillar_));
    }
    Handle_<CalibrationInstrument_> BindCalibrationInstrumentParameters(const Handle_<CalibrationInstrument_>& instrument,
                                                                        const CalibrationParameterBindings_& updates) {
        Check(bool(instrument), BIND_ERROR, "/instrument", "nonnull immutable instrument");
        return Handle_<CalibrationInstrument_>(new CalibrationInstrument_(
            instrument->Id(), instrument->Definition(), ApplyBindings(*instrument->Definition(), instrument->Binding(), updates, instrument->Id()),
            instrument->Pillar()));
    }
    std::unique_ptr<CalibrationInstrument_> CalibrationInstrument_::Clone() const {
        return std::unique_ptr<CalibrationInstrument_>(new CalibrationInstrument_(id_, definition_, binding_, pillar_));
    }
    std::unique_ptr<Script::ScriptProduct_> CalibrationInstrument_::Product() const {
        std::map<String_, double> slots;
        for (const auto& value : binding_)
            slots.emplace(value.key_, value.value_);
        CalibrationDiagnostic_ context;
        context.instrumentId_ = id_;
        context.stage_ = CalibrationStage_::Value_::Prepare;
        return BoundProduct(definition_->Spec(), slots, context);
    }
} // namespace Dal
