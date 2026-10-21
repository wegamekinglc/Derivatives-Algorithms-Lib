#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/script/instrument_internal.hpp>
#include <dal/storage/archive.hpp>

namespace Dal::CalibrationInternal {
    namespace {
        const String_ ARCHIVE_ERROR = "InvalidCalibrationArchive";
        Json_ UnitJson(const CalibrationValueUnit_& unit) { return Str(unit.Token()); }
        Json_ DateJson(const Date_& date) { return Str(Date::ToString(date)); }
        template <class E_> Json_ EnumJson(const E_& value) { return Str(Token(value, ARCHIVE_ERROR, "")); }
        template <class E_> E_ ReadEnum(const Json_& value, const String_& path) { return Enum<E_>(ReadString(value, path), ARCHIVE_ERROR, path); }
        CalibrationValueUnit_ ReadUnit(const Json_& value, const String_& path) {
            const auto text = ReadString(value, path);
            try {
                return CalibrationValueUnit_::Parse(text);
            } catch (const CalibrationError_&) {
                Fail(ARCHIVE_ERROR, path, "a supported exact unit token", text);
            }
        }
        Date_ ReadDate(const Json_& value, const String_& path) { return ParseDate(ReadString(value, path), ARCHIVE_ERROR, path); }
        int ReadInt(const Json_& value, const String_& path) {
            const double number = ReadNumber(value, path);
            Check(number >= INT_MIN && number <= INT_MAX && number == std::trunc(number), ARCHIVE_ERROR, path, "exact 32-bit integer");
            return static_cast<int>(number);
        }
        void Version(const Json_& value, const char* name, const String_& path, const char* expected) {
            const auto actual = ReadString(Field(value, name), path + "/" + name);
            Check(Bytes(actual) == expected, "UnsupportedCalibrationSchema", path + "/" + name, expected, actual);
        }
    } // namespace
    Json_ SourceJson(const ScriptInstrumentSource_& source) {
        return Obj({{"rows", Array(source.rows_,
                                   [](const ScriptSourceRow_& row) {
                                       return Obj({{"kind", EnumJson(row.kind_)}, {"key", Str(row.key_)}, {"text", Str(row.text_)}});
                                   })},
                    {"settings", Obj({{"default_index", Str(source.settings_.defaultIndex_)},
                                      {"regression_features", Array(source.settings_.regressionFeatures_, Str)}})}});
    }
    Json_ DefinitionJson(const ScriptInstrumentDefinitionSpec_& spec) {
        const auto& h = spec.header_;
        return Obj({{"schema", Str("dal.script-instrument-definition/1")},
                    {"language", Str("dal.script/1")},
                    {"semantics", Str("dal.calibration-contract/1")},
                    {"source", SourceJson(spec.source_)},
                    {"header", Obj({{"pay_currency", EnumJson(h.payCurrency_)},
                                    {"result_currency", EnumJson(h.resultCurrency_)},
                                    {"collateral_currency", EnumJson(h.collateralCurrency_)},
                                    {"collateral_type", EnumJson(h.collateralType_)},
                                    {"receiver", Str(h.receiver_)},
                                    {"notional_parameter", Str(h.notionalParameter_)},
                                    {"settlement", EnumJson(h.settlement_)},
                                    {"valuation_cashflows", EnumJson(h.valuationCashflows_)},
                                    {"discount_route", Str(h.discountRoute_)},
                                    {"convention_set", Str(h.conventionSet_)}})},
                    {"parameters", Array(spec.parameters_,
                                         [](const ScriptParameterDeclaration_& d) {
                                             return Obj({{"key", Str(d.key_)},
                                                         {"type", EnumJson(d.type_)},
                                                         {"unit", UnitJson(d.unit_)},
                                                         {"role", EnumJson(d.role_)},
                                                         {"bindable", Bool(d.bindable_)},
                                                         {"lower", Optional(d.lower_, Num)},
                                                         {"upper", Optional(d.upper_, Num)},
                                                         {"lower_inclusive", Bool(d.lowerInclusive_)},
                                                         {"upper_inclusive", Bool(d.upperInclusive_)}});
                                         })},
                    {"measures", Array(spec.measures_, [](const CalibrationMeasureSpec_& m) {
                         return Obj({{"id", Str(m.id_)},
                                     {"kind", EnumJson(m.kind_)},
                                     {"receiver", Str(m.receiver_)},
                                     {"parameter", Optional(m.parameter_, Str)},
                                     {"unit", UnitJson(m.unit_)},
                                     {"annuity_sign", Optional(m.annuitySign_, [](int i) { return Num(i); })},
                                     {"min_abs_annuity", Optional(m.minAbsAnnuity_, Num)}});
                     })}});
    }
    Json_ BindingValuesJson(const Vector_<CalibrationEffectiveParameter_>& binding, bool provenance) {
        return Array(binding, [=](const CalibrationEffectiveParameter_& value) {
            auto result = Obj({{"key", Str(value.key_)}, {"value", Num(value.value_)}, {"unit", UnitJson(value.unit_)}});
            if (provenance) {
                result.object_["origin"] = EnumJson(value.origin_);
                result.object_["quote_id"] = Optional(value.quoteId_, Str);
                result.object_["quote_fingerprint"] = Optional(value.quoteFingerprint_, Str);
            }
            return result;
        });
    }
    Json_ InstrumentJson(const CalibrationInstrument_& instrument) {
        return Obj(
            {{"schema", Str("dal.calibration-instrument/1")},
             {"instrument_id", Str(instrument.Id())},
             {"definition", DefinitionJson(instrument.Definition()->Spec())},
             {"binding", Obj({{"schema", Str("dal.calibration-parameter-binding/1")}, {"values", BindingValuesJson(instrument.Binding(), true)}})},
             {"pillar", Optional(instrument.Pillar(), DateJson)}});
    }
    ScriptInstrumentDefinitionSpec_ ReadDefinition(const Json_& value, const String_& path) {
        Fields(value, path, {"schema", "language", "semantics", "source", "header", "parameters", "measures"});
        Version(value, "schema", path, "dal.script-instrument-definition/1");
        Version(value, "language", path, "dal.script/1");
        Version(value, "semantics", path, "dal.calibration-contract/1");
        ScriptInstrumentDefinitionSpec_ spec;
        const auto& source = Field(value, "source");
        Fields(source, path + "/source", {"rows", "settings"});
        const auto& rows = ReadArray(Field(source, "rows"), path + "/source/rows");
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto& row = rows[i];
            const String_ p = path + "/source/rows/" + OrdinalText(i);
            Fields(row, p, {"kind", "key", "text"});
            spec.source_.rows_.push_back({ReadEnum<ScriptSourceRowKind_>(Field(row, "kind"), p + "/kind"), ReadString(Field(row, "key"), p + "/key"),
                                          ReadString(Field(row, "text"), p + "/text")});
        }
        const auto& settings = Field(source, "settings");
        Fields(settings, path + "/source/settings", {"default_index", "regression_features"});
        spec.source_.settings_.defaultIndex_ = ReadString(Field(settings, "default_index"), path + "/source/settings/default_index");
        const auto& features = ReadArray(Field(settings, "regression_features"), path + "/source/settings/regression_features");
        for (size_t i = 0; i < features.size(); ++i)
            spec.source_.settings_.regressionFeatures_.push_back(
                ReadString(features[i], path + "/source/settings/regression_features/" + OrdinalText(i)));
        const auto& header = Field(value, "header");
        const String_ h = path + "/header";
        Fields(header, h,
               {"pay_currency", "result_currency", "collateral_currency", "collateral_type", "receiver", "notional_parameter", "settlement",
                "valuation_cashflows", "discount_route", "convention_set"});
        spec.header_ = {ReadEnum<Ccy_>(Field(header, "pay_currency"), h + "/pay_currency"),
                        ReadEnum<Ccy_>(Field(header, "result_currency"), h + "/result_currency"),
                        ReadEnum<Ccy_>(Field(header, "collateral_currency"), h + "/collateral_currency"),
                        ReadEnum<CollateralType_>(Field(header, "collateral_type"), h + "/collateral_type"),
                        ReadString(Field(header, "receiver"), h + "/receiver"),
                        ReadString(Field(header, "notional_parameter"), h + "/notional_parameter"),
                        ReadEnum<ScriptSettlement_>(Field(header, "settlement"), h + "/settlement"),
                        ReadEnum<ScriptValuationCashflows_>(Field(header, "valuation_cashflows"), h + "/valuation_cashflows"),
                        ReadString(Field(header, "discount_route"), h + "/discount_route"),
                        ReadString(Field(header, "convention_set"), h + "/convention_set")};
        const auto& declarations = ReadArray(Field(value, "parameters"), path + "/parameters");
        for (size_t i = 0; i < declarations.size(); ++i) {
            const auto& d = declarations[i];
            const String_ p = path + "/parameters/" + OrdinalText(i);
            Fields(d, p, {"key", "type", "unit", "role", "bindable", "lower", "upper", "lower_inclusive", "upper_inclusive"});
            spec.parameters_.push_back(
                {ReadString(Field(d, "key"), p + "/key"), ReadEnum<ScriptParameterType_>(Field(d, "type"), p + "/type"),
                 ReadUnit(Field(d, "unit"), p + "/unit"), ReadEnum<ScriptParameterRole_>(Field(d, "role"), p + "/role"),
                 ReadBool(Field(d, "bindable"), p + "/bindable"), ReadOptional<double>(Field(d, "lower"), p + "/lower", ReadNumber),
                 ReadOptional<double>(Field(d, "upper"), p + "/upper", ReadNumber), ReadBool(Field(d, "lower_inclusive"), p + "/lower_inclusive"),
                 ReadBool(Field(d, "upper_inclusive"), p + "/upper_inclusive")});
        }
        const auto& measures = ReadArray(Field(value, "measures"), path + "/measures");
        for (size_t i = 0; i < measures.size(); ++i) {
            const auto& m = measures[i];
            const String_ p = path + "/measures/" + OrdinalText(i);
            Fields(m, p, {"id", "kind", "receiver", "parameter", "unit", "annuity_sign", "min_abs_annuity"});
            spec.measures_.push_back(
                {ReadString(Field(m, "id"), p + "/id"), ReadEnum<CalibrationMeasureKind_>(Field(m, "kind"), p + "/kind"),
                 ReadString(Field(m, "receiver"), p + "/receiver"), ReadOptional<String_>(Field(m, "parameter"), p + "/parameter", ReadString),
                 ReadUnit(Field(m, "unit"), p + "/unit"), ReadOptional<int>(Field(m, "annuity_sign"), p + "/annuity_sign", ReadInt),
                 ReadOptional<double>(Field(m, "min_abs_annuity"), p + "/min_abs_annuity", ReadNumber)});
        }
        return spec;
    }
    Vector_<CalibrationEffectiveParameter_> ReadBinding(const Json_& value, const ScriptInstrumentDefinition_& definition) {
        Fields(value, "/binding", {"schema", "values"});
        Version(value, "schema", "/binding", "dal.calibration-parameter-binding/1");
        const auto& rows = ReadArray(Field(value, "values"), "/binding/values");
        const auto& declarations = definition.Spec().parameters_;
        Check(rows.size() == declarations.size(), ARCHIVE_ERROR, "/binding/values", "complete binding in declaration order");
        Vector_<CalibrationEffectiveParameter_> result;
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto& row = rows[i];
            const auto& d = declarations[i];
            const String_ p = "/binding/values/" + OrdinalText(i);
            Fields(row, p, {"key", "value", "unit", "origin", "quote_id", "quote_fingerprint"});
            CalibrationEffectiveParameter_ v;
            v.key_ = ReadString(Field(row, "key"), p + "/key");
            v.value_ = ReadNumber(Field(row, "value"), p + "/value");
            v.unit_ = ReadUnit(Field(row, "unit"), p + "/unit");
            v.origin_ = ReadEnum<CalibrationBindingOrigin_>(Field(row, "origin"), p + "/origin");
            v.quoteId_ = ReadOptional<String_>(Field(row, "quote_id"), p + "/quote_id", ReadString);
            v.quoteFingerprint_ = ReadOptional<String_>(Field(row, "quote_fingerprint"), p + "/quote_fingerprint", ReadString);
            CalibrationDiagnostic_ context;
            context.parameterKey_ = d.key_;
            Check(Bytes(v.key_) == Bytes(d.key_), ARCHIVE_ERROR, p + "/key", d.key_, v.key_, context);
            Check(v.unit_ == d.unit_, ARCHIVE_ERROR, p + "/unit", d.unit_.Token(), v.unit_.Token(), context);
            ValidateParameter(v.value_, d, p + "/value", context);
            if (v.origin_ == CalibrationBindingOrigin_::Value_::SourceDefault)
                Check(v.value_ == definition.Defaults()[i], ARCHIVE_ERROR, p + "/value", "matching source default", "", context);
            Check(d.bindable_ || v.origin_ == CalibrationBindingOrigin_::Value_::SourceDefault, ARCHIVE_ERROR, p + "/origin",
                  "SourceDefault for fixed parameters", "", context);
            if (v.origin_ == CalibrationBindingOrigin_::Value_::QuoteCopy) {
                Check(v.quoteId_ && ValidId(*v.quoteId_) && v.quoteFingerprint_ && ValidFingerprint(*v.quoteFingerprint_), ARCHIVE_ERROR,
                      p + "/quote_fingerprint", "valid quote ID and SHA-256 fingerprint", "", context);
            } else {
                Check(!v.quoteId_ && !v.quoteFingerprint_, ARCHIVE_ERROR, p + "/origin", "null quote fields for SourceDefault/Literal", "", context);
            }
            result.push_back(v);
        }
        return result;
    }
    Handle_<ScriptInstrumentDefinition_> ReadDefinitionHandle(const Json_& value, const String_& path) {
        const auto spec = ReadDefinition(value, path);
        try {
            return NewScriptInstrumentDefinition(spec);
        } catch (const CalibrationError_& error) {
            auto diagnostic = error.Failure().diagnostic_;
            Fail(diagnostic.code_, path + diagnostic.field_.value_or(""), diagnostic.expected_.value_or("valid definition"),
                 diagnostic.actual_.value_or(""), diagnostic);
        }
    }
    template <class F_> auto LoadBoundary(const F_& load, const CalibrationDiagnostic_& context = {}) {
        try {
            return load();
        } catch (const CalibrationError_& error) {
            auto diagnostic = error.Failure().diagnostic_;
            diagnostic.stage_ = CalibrationStage_::Value_::Load;
            if (context.instrumentId_)
                diagnostic.instrumentId_ = context.instrumentId_;
            if (Bytes(diagnostic.code_) != "UnsupportedCalibrationSchema")
                diagnostic.code_ = ARCHIVE_ERROR;
            Fail(diagnostic.code_, diagnostic.field_.value_or(""), diagnostic.expected_.value_or("valid archive"), diagnostic.actual_.value_or(""),
                 diagnostic);
        }
    }
} // namespace Dal::CalibrationInternal

namespace Dal {
    using namespace CalibrationInternal;
    String_ DumpScriptInstrumentDefinitionJson(const Handle_<ScriptInstrumentDefinition_>& definition) {
        Check(bool(definition), "InvalidScriptInstrumentDefinition", "/definition", "nonnull definition");
        return String_(Jcs(DefinitionJson(definition->Spec())));
    }
    Handle_<ScriptInstrumentDefinition_> LoadScriptInstrumentDefinitionJson(const String_& json) {
        return LoadBoundary([&] { return ReadDefinitionHandle(Parse(json), ""); });
    }
    String_ DumpCalibrationInstrumentJson(const Handle_<CalibrationInstrument_>& instrument) {
        Check(bool(instrument), "InvalidCalibrationParameterBinding", "/instrument", "nonnull instrument");
        return String_(Jcs(InstrumentJson(*instrument)));
    }
    Handle_<CalibrationInstrument_> LoadCalibrationInstrumentJson(const String_& json) {
        CalibrationDiagnostic_ context;
        return LoadBoundary(
            [&] {
                const auto value = Parse(json);
                Fields(value, "", {"schema", "instrument_id", "definition", "binding", "pillar"});
                const auto schema = ReadString(Field(value, "schema"), "/schema");
                Check(Bytes(schema) == "dal.calibration-instrument/1", "UnsupportedCalibrationSchema", "/schema", "dal.calibration-instrument/1",
                      schema);
                const auto id = ReadString(Field(value, "instrument_id"), "/instrument_id");
                context.instrumentId_ = id;
                const auto definition = ReadDefinitionHandle(Field(value, "definition"), "/definition");
                return Handle_<CalibrationInstrument_>(
                    new CalibrationInstrument_(id, definition, ReadBinding(Field(value, "binding"), *definition),
                                               ReadOptional<Date_>(Field(value, "pillar"), "/pillar", [](const Json_& v, const String_& p) {
                                                   return ParseDate(ReadString(v, p), "InvalidCalibrationArchive", p);
                                               })));
            },
            context);
    }
    void ScriptInstrumentDefinition_::Write(Archive::Store_& dst) const {
        dst.SetType("ScriptInstrumentDefinition_v1");
        Archive::Utils::Set(dst, "json", String_(Jcs(DefinitionJson(spec_))));
        dst.Done();
    }
    void CalibrationInstrument_::Write(Archive::Store_& dst) const {
        dst.SetType("CalibrationInstrument_v1");
        Archive::Utils::Set(dst, "json", String_(Jcs(InstrumentJson(*this))));
        dst.Done();
    }
    namespace {
        [[noreturn]] void ArchiveJsonFailure() {
            CalibrationDiagnostic_ context;
            context.stage_ = CalibrationStage_::Value_::Load;
            Fail("InvalidCalibrationArchive", "/json", "required portable JSON string", "", context);
        }
        String_ ReadPortableArchiveJson(const Archive::View_& view) {
            try {
                return view.Child("json").AsString();
            } catch (const Exception_&) {
                ArchiveJsonFailure();
            }
        }
        template <class T_, Handle_<T_> (*Load_)(const String_&)> class PortableReader_ final : public Archive::Reader_ {
            Storable_* Build() const override { ArchiveJsonFailure(); }
            Storable_* Build(const Archive::View_& view, Archive::Built_&) const override {
                return Load_(ReadPortableArchiveJson(view))->Clone().release();
            }
        };
        const PortableReader_<ScriptInstrumentDefinition_, LoadScriptInstrumentDefinitionJson> DEFINITION_READER;
        const PortableReader_<CalibrationInstrument_, LoadCalibrationInstrumentJson> INSTRUMENT_READER;
        RUN_AT_LOAD(Archive::Register("ScriptInstrumentDefinition_v1", &DEFINITION_READER))
        RUN_AT_LOAD(Archive::Register("CalibrationInstrument_v1", &INSTRUMENT_READER))
    } // namespace
} // namespace Dal
