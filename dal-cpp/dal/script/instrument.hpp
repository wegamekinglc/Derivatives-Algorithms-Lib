#pragma once

#include <dal/calibration/types.hpp>
#include <dal/script/settings.hpp>
#include <dal/storage/storable.hpp>

namespace Dal {
    namespace Script {
        class ScriptProduct_;
    } // namespace Script

    struct ScriptSourceRow_ {
        ScriptSourceRowKind_ kind_;
        String_ key_, text_;
    };
    struct ScriptInstrumentSource_ {
        Vector_<ScriptSourceRow_> rows_;
        Script::ScriptProductSettings_ settings_;
    };
    struct ScriptInstrumentHeader_ {
        Ccy_ payCurrency_, resultCurrency_, collateralCurrency_;
        CollateralType_ collateralType_ = CollateralType_::Value_::OIS;
        String_ receiver_ = "PAY", notionalParameter_ = "N";
        ScriptSettlement_ settlement_ = ScriptSettlement_::Value_::AtEvent;
        ScriptValuationCashflows_ valuationCashflows_ = ScriptValuationCashflows_::Value_::IncludeUnsettledAtValuation;
        String_ discountRoute_, conventionSet_ = "dal.fixed-dates/1";
    };
    struct ScriptParameterDeclaration_ {
        String_ key_;
        ScriptParameterType_ type_;
        CalibrationValueUnit_ unit_;
        ScriptParameterRole_ role_;
        bool bindable_ = false;
        std::optional<double> lower_, upper_;
        bool lowerInclusive_ = true, upperInclusive_ = true;
    };
    struct ScriptInstrumentDefinitionSpec_ {
        ScriptInstrumentSource_ source_;
        ScriptInstrumentHeader_ header_;
        Vector_<ScriptParameterDeclaration_> parameters_;
        Vector_<CalibrationMeasureSpec_> measures_;
    };
    class ScriptInstrumentDefinition_ final : public Storable_ {
        ScriptInstrumentDefinitionSpec_ spec_;
        Vector_<> defaults_;
        String_ sourceIdentity_, definitionIdentity_;
        explicit ScriptInstrumentDefinition_(ScriptInstrumentDefinitionSpec_ spec);
        friend Handle_<ScriptInstrumentDefinition_> NewScriptInstrumentDefinition(const ScriptInstrumentDefinitionSpec_&);

    public:
        [[nodiscard]] const ScriptInstrumentDefinitionSpec_& Spec() const { return spec_; }
        [[nodiscard]] const Vector_<>& Defaults() const { return defaults_; }
        [[nodiscard]] const String_& SourceIdentity() const { return sourceIdentity_; }
        [[nodiscard]] const String_& DefinitionIdentity() const { return definitionIdentity_; }
        [[nodiscard]] std::unique_ptr<ScriptInstrumentDefinition_> Clone() const;
        void Write(Archive::Store_& dst) const override;
    };
    struct CalibrationParameterValue_ {
        String_ key_;
        double value_ = 0.;
        CalibrationValueUnit_ unit_;
    };
    struct CalibrationQuoteCopy_ {
        String_ parameterKey_, quoteId_, quoteFingerprint_;
    };
    struct CalibrationParameterBindings_ {
        Vector_<CalibrationParameterValue_> values_;
        Vector_<CalibrationQuoteCopy_> quoteCopies_;
    };
    struct CalibrationEffectiveParameter_ : CalibrationParameterValue_ {
        CalibrationBindingOrigin_ origin_ = CalibrationBindingOrigin_::Value_::SourceDefault;
        std::optional<String_> quoteId_, quoteFingerprint_;
    };
    struct ScriptCalibrationInstrumentSpec_ {
        String_ id_;
        Handle_<ScriptInstrumentDefinition_> definition_;
        CalibrationParameterBindings_ bindings_;
        std::optional<Date_> pillar_;
    };
    class CalibrationInstrument_ final : public Storable_ {
        String_ id_;
        Handle_<ScriptInstrumentDefinition_> definition_;
        Vector_<CalibrationEffectiveParameter_> binding_;
        std::optional<Date_> pillar_;
        String_ bindingIdentity_, instrumentIdentity_;
        CalibrationInstrument_(String_ id,
                               Handle_<ScriptInstrumentDefinition_> definition,
                               Vector_<CalibrationEffectiveParameter_> binding,
                               std::optional<Date_> pillar);
        friend Handle_<CalibrationInstrument_> NewScriptCalibrationInstrument(const ScriptCalibrationInstrumentSpec_&);
        friend Handle_<CalibrationInstrument_> BindCalibrationInstrumentParameters(const Handle_<CalibrationInstrument_>&,
                                                                                   const CalibrationParameterBindings_&);
        friend Handle_<CalibrationInstrument_> LoadCalibrationInstrumentJson(const String_&);

    public:
        [[nodiscard]] const String_& Id() const { return id_; }
        [[nodiscard]] const Handle_<ScriptInstrumentDefinition_>& Definition() const { return definition_; }
        [[nodiscard]] const Vector_<CalibrationEffectiveParameter_>& Binding() const { return binding_; }
        [[nodiscard]] const std::optional<Date_>& Pillar() const { return pillar_; }
        [[nodiscard]] const String_& BindingIdentity() const { return bindingIdentity_; }
        [[nodiscard]] const String_& InstrumentIdentity() const { return instrumentIdentity_; }
        [[nodiscard]] std::unique_ptr<CalibrationInstrument_> Clone() const;
        [[nodiscard]] std::unique_ptr<Script::ScriptProduct_> Product() const;
        void Write(Archive::Store_& dst) const override;
    };
    Handle_<ScriptInstrumentDefinition_> NewScriptInstrumentDefinition(const ScriptInstrumentDefinitionSpec_& spec);
    Handle_<CalibrationInstrument_> NewScriptCalibrationInstrument(const ScriptCalibrationInstrumentSpec_& spec);
    Handle_<CalibrationInstrument_> BindCalibrationInstrumentParameters(const Handle_<CalibrationInstrument_>& instrument,
                                                                        const CalibrationParameterBindings_& updates);
    String_ DumpScriptInstrumentDefinitionJson(const Handle_<ScriptInstrumentDefinition_>& definition);
    Handle_<ScriptInstrumentDefinition_> LoadScriptInstrumentDefinitionJson(const String_& json);
    String_ DumpCalibrationInstrumentJson(const Handle_<CalibrationInstrument_>& instrument);
    Handle_<CalibrationInstrument_> LoadCalibrationInstrumentJson(const String_& json);
} // namespace Dal
