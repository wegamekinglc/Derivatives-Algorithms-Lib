#include <gtest/gtest.h>

#include <cstdlib>
#include <limits>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <stdexcept>

#include <dal/calibration/protocol.hpp>
#include <dal/script/event.hpp>
#include <dal/script/instrument.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    const String_ DEPOSIT_SPECIMEN = R"json(
{
  "schema": "dal.calibration-instrument/1",
  "instrument_id": "usd-deposit-1y",
  "definition": {
    "schema": "dal.script-instrument-definition/1",
    "language": "dal.script/1",
    "semantics": "dal.calibration-contract/1",
    "source": {
      "rows": [
        {"kind": "Definition", "key": "N", "text": "1"},
        {"kind": "Definition", "key": "K", "text": "0.025"},
        {"kind": "Definition", "key": "ALPHA", "text": "1.0138888888888888"},
        {"kind": "Event", "key": "2026-10-21", "text": "pay PAYS -N"},
        {"kind": "Event", "key": "2027-10-21", "text": "pay PAYS N*(1+K*ALPHA)"}
      ],
      "settings": {"default_index": "", "regression_features": []}
    },
    "header": {
      "pay_currency": "USD", "result_currency": "USD", "collateral_currency": "USD",
      "collateral_type": "OIS", "receiver": "PAY", "notional_parameter": "N",
      "settlement": "AtEvent", "valuation_cashflows": "IncludeUnsettledAtValuation",
      "discount_route": "USD.OIS", "convention_set": "dal.fixed-dates/1"
    },
    "parameters": [
      {"key": "N", "type": "Real", "unit": "Price:USD", "role": "Notional", "bindable": true,
       "lower": 0, "upper": null, "lower_inclusive": false, "upper_inclusive": true},
      {"key": "K", "type": "Real", "unit": "Rate", "role": "Coupon", "bindable": true,
       "lower": null, "upper": null, "lower_inclusive": true, "upper_inclusive": true},
      {"key": "ALPHA", "type": "Real", "unit": "YearFraction", "role": "Accrual", "bindable": false,
       "lower": 0, "upper": null, "lower_inclusive": false, "upper_inclusive": true}
    ],
    "measures": [
      {"id": "Price", "kind": "Price", "receiver": "PAY", "parameter": null,
       "unit": "Price:USD", "annuity_sign": null, "min_abs_annuity": null},
      {"id": "ParRate", "kind": "ParAffineRate", "receiver": "PAY", "parameter": "K",
       "unit": "Rate", "annuity_sign": 1, "min_abs_annuity": 1e-12}
    ]
  },
  "binding": {
    "schema": "dal.calibration-parameter-binding/1",
    "values": [
      {"key": "N", "value": 1, "unit": "Price:USD", "origin": "SourceDefault", "quote_id": null, "quote_fingerprint": null},
      {"key": "K", "value": 0.03, "unit": "Rate", "origin": "Literal", "quote_id": null, "quote_fingerprint": null},
      {"key": "ALPHA", "value": 1.0138888888888888, "unit": "YearFraction", "origin": "SourceDefault", "quote_id": null, "quote_fingerprint": null}
    ]
  },
  "pillar": "2027-10-21"
}
)json";
    ScriptInstrumentDefinitionSpec_ DepositDefinition() {
        const auto amount = CalibrationValueUnit_::Parse("Price:USD");
        ScriptInstrumentDefinitionSpec_ spec;
        spec.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", "1"},
                              {ScriptSourceRowKind_::Value_::Definition, "K", "0.025"},
                              {ScriptSourceRowKind_::Value_::Definition, "ALPHA", "1"},
                              {ScriptSourceRowKind_::Value_::Event, "2026-10-21", "pay PAYS -N"},
                              {ScriptSourceRowKind_::Value_::Event, "2027-10-21", "pay PAYS N*(1+K*ALPHA)"}};
        spec.header_.payCurrency_ = spec.header_.resultCurrency_ = spec.header_.collateralCurrency_ = Ccy_("USD");
        spec.header_.discountRoute_ = "USD.OIS";
        spec.parameters_ = {{"N", ScriptParameterType_::Value_::Real, amount, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
                            {"K",
                             ScriptParameterType_::Value_::Real,
                             CalibrationValueUnit_::Parse("Rate"),
                             ScriptParameterRole_::Value_::Coupon,
                             true,
                             {},
                             {},
                             true,
                             true},
                            {"ALPHA",
                             ScriptParameterType_::Value_::Real,
                             CalibrationValueUnit_::Parse("YearFraction"),
                             ScriptParameterRole_::Value_::Accrual,
                             false,
                             0.,
                             {},
                             false,
                             true}};
        spec.measures_ = {
            {"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, amount, {}, {}},
            {"ParRate", CalibrationMeasureKind_::Value_::ParAffineRate, "PAY", String_("K"), CalibrationValueUnit_::Parse("Rate"), 1, 1e-12}};
        return spec;
    }
    Handle_<CalibrationInstrument_> Instrument(const String_& id = "deposit") {
        return NewScriptCalibrationInstrument({id, NewScriptInstrumentDefinition(DepositDefinition())});
    }
    template <class F_>
    void AssertFailure(const F_& action, const String_& code, const String_& field, const std::optional<String_>& parameter = std::nullopt) {
        try {
            action();
            FAIL() << "expected " << code;
        } catch (const CalibrationError_& error) {
            const auto& d = error.Failure().diagnostic_;
            ASSERT_EQ(d.code_, code) << error.what();
            ASSERT_EQ(d.field_, std::optional<String_>(field)) << error.what();
            if (parameter)
                ASSERT_EQ(d.parameterKey_, parameter);
            ASSERT_TRUE(error.Failure().report_);
            ASSERT_EQ(error.Failure().report_->Errors().size(), 1);
            ASSERT_FALSE(error.Failure().report_->QuoteRiskAvailable());
            ASSERT_FALSE(error.Failure().candidate_);
        }
    }
    String_ JsonText(const rapidjson::Value& value) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);
        return String_(buffer.GetString());
    }
    template <class F_> String_ MutateArchive(const F_& mutation) {
        const auto json = DumpCalibrationInstrumentJson(Instrument());
        rapidjson::Document document;
        document.Parse(json.c_str());
        mutation(document);
        return JsonText(document);
    }
    String_ NestedJson(size_t depth, bool objects) {
        String_ result;
        for (size_t i = 0; i < depth; ++i)
            result += objects ? "{\"nested\":" : "[";
        return result + "0" + String_(depth, objects ? '}' : ']');
    }
    bool OwnedNestingFailure(const CalibrationError_& error) {
        const auto& failure = error.Failure();
        if (!failure.report_)
            return false;
        const auto& d = failure.diagnostic_;
        return d.code_ == "InvalidCalibrationArchive" && d.stage_ == CalibrationStage_::Value_::Load &&
               d.expected_ == std::optional<String_>("at most 128 nested JSON containers") &&
               failure.report_->Stage() == CalibrationStage_::Value_::Load && failure.report_->Errors().size() == 1;
    }
    int NestingFailureExitCode(const String_& json, bool definition) {
        try {
            if (definition)
                static_cast<void>(LoadScriptInstrumentDefinitionJson(json));
            else
                static_cast<void>(LoadCalibrationInstrumentJson(json));
            return 41;
        } catch (const CalibrationError_& error) {
            return OwnedNestingFailure(error) ? 0 : 42;
        } catch (const std::exception&) {
            return 43;
        }
    }
    void AssertBoundedArchiveDepth(bool objects) {
        ::testing::FLAGS_gtest_death_test_style = "threadsafe";
        for (size_t depth : {12000, 30000, 129}) {
            const auto json = NestedJson(depth, objects);
            for (bool definition : {false, true}) {
                SCOPED_TRACE(depth);
                ASSERT_EXIT(std::_Exit(NestingFailureExitCode(json, definition)), ::testing::ExitedWithCode(0), "");
            }
        }
    }
    template <class F_>
    void AssertParseFailure(
        const F_& action, const String_& field, CalibrationStage_ stage, int statement = 0, const String_& cause = "unexpected end of statement") {
        std::optional<CalibrationError_> saved;
        try {
            action();
        } catch (const CalibrationError_& error) {
            saved = error;
        } catch (const std::exception& error) {
            FAIL() << "unstructured parser failure: " << error.what();
        }
        ASSERT_TRUE(saved) << "expected an owned calibration failure";
        const auto& failure = saved->Failure();
        const auto& d = failure.diagnostic_;
        ASSERT_EQ(d.code_, stage == CalibrationStage_::Value_::Load ? "InvalidCalibrationArchive" : "InvalidScriptInstrumentDefinition");
        ASSERT_EQ(d.stage_, stage);
        ASSERT_EQ(d.field_, std::optional<String_>(field));
        ASSERT_EQ(d.sourceRow_, std::optional<int>(5));
        ASSERT_EQ(d.eventDate_, std::optional<Date_>(Date_(2027, 10, 21)));
        ASSERT_EQ(d.statement_, std::optional<int>(statement));
        ASSERT_TRUE(d.actual_);
        ASSERT_NE(d.actual_->find(cause), String_::npos);
        ASSERT_TRUE(failure.report_);
        ASSERT_EQ(failure.report_->Stage(), stage);
        ASSERT_EQ(failure.report_->Errors().size(), 1);
        ASSERT_EQ(failure.report_->Errors()[0].code_, d.code_);
        ASSERT_EQ(failure.report_->Errors()[0].field_, d.field_);
        ASSERT_EQ(failure.report_->Errors()[0].sourceRow_, d.sourceRow_);
        ASSERT_EQ(failure.report_->Errors()[0].eventDate_, d.eventDate_);
        ASSERT_EQ(failure.report_->Errors()[0].statement_, d.statement_);
    }
    void AssertInputFailureAtAllBoundaries(const String_& text, const String_& cause, int statement = 0, bool merged = false) {
        AssertParseFailure(
            [&] {
                auto spec = DepositDefinition();
                if (merged)
                    spec.source_.rows_[3].key_ = "2027-10-21";
                spec.source_.rows_[4].text_ = text;
                static_cast<void>(NewScriptInstrumentDefinition(spec));
            },
            "/source/rows/4/text", CalibrationStage_::Value_::Input, statement, cause);
        for (bool definition : {false, true}) {
            AssertParseFailure(
                [&] {
                    const auto json = MutateArchive([&](auto& d) {
                        if (merged)
                            d["definition"]["source"]["rows"][3]["key"].SetString("2027-10-21");
                        d["definition"]["source"]["rows"][4]["text"].SetString(text.c_str(), d.GetAllocator());
                    });
                    if (definition) {
                        rapidjson::Document document;
                        document.Parse(json.c_str());
                        static_cast<void>(LoadScriptInstrumentDefinitionJson(JsonText(document["definition"])));
                    } else
                        static_cast<void>(LoadCalibrationInstrumentJson(json));
                },
                definition ? "/source/rows/4/text" : "/definition/source/rows/4/text", CalibrationStage_::Value_::Load, statement, cause);
        }
    }
    void LegacyProduct(const String_& text) { Script::ScriptProduct_ product({Cell_("N"), Cell_(Date_(2027, 10, 21))}, {"1", text}, "PAY"); }
} // namespace

TEST(ScriptInstrumentTest, TestStandardNumericErrorsOwnSourceContext) {
    for (const char* text : {"pay PAYS 1e999", "pay PAYS ."}) {
        SCOPED_TRACE(text);
        AssertInputFailureAtAllBoundaries(text, "stod");
    }
}

TEST(ScriptInstrumentTest, TestLexicalErrorsOwnSourceContext) { AssertInputFailureAtAllBoundaries("pay PAYS N$", "unexpected character '$'"); }

TEST(ScriptInstrumentTest, TestRemainingInputErrorsOwnMergedContext) {
    for (const char* text : {"pay PAYS 1e999", "pay PAYS .", "pay PAYS N$"}) {
        SCOPED_TRACE(text);
        AssertInputFailureAtAllBoundaries(text, text == String_("pay PAYS N$") ? "unexpected character '$'" : "stod", 1, true);
    }
}

TEST(ScriptInstrumentTest, TestLexicalErrorsAfterValidStatementOwnContext) {
    AssertInputFailureAtAllBoundaries("x = 1 pay PAYS N$", "unexpected character '$'", 1);
}

TEST(ScriptInstrumentTest, TestLegacyProductKeepsNumericAndLexExceptionTypes) {
    ASSERT_THROW(LegacyProduct("pay PAYS 1e999"), std::out_of_range);
    ASSERT_THROW(LegacyProduct("pay PAYS ."), std::invalid_argument);
    ASSERT_THROW(LegacyProduct("pay PAYS N$"), ScriptError_);
}

TEST(ScriptInstrumentTest, TestArchiveArrayDepthIsBoundedBeforeParsing) { AssertBoundedArchiveDepth(false); }

TEST(ScriptInstrumentTest, TestArchiveObjectDepthIsBoundedBeforeParsing) { AssertBoundedArchiveDepth(true); }

TEST(ScriptInstrumentTest, TestArchiveNestingBoundaryAndQuotedDelimiters) {
    for (bool objects : {false, true}) {
        try {
            static_cast<void>(LoadCalibrationInstrumentJson(NestedJson(128, objects)));
            FAIL() << "non-schema JSON was accepted";
        } catch (const CalibrationError_& error) {
            ASSERT_FALSE(OwnedNestingFailure(error));
            ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Load);
        }
    }
    const String_ route = String_(1000, '[') + "\"\\{}\"" + String_(1000, ']');
    const auto json = MutateArchive([&](auto& d) { d["definition"]["header"]["discount_route"].SetString(route.c_str(), d.GetAllocator()); });
    ASSERT_EQ(LoadCalibrationInstrumentJson(json)->Definition()->Spec().header_.discountRoute_, route);
}

TEST(ScriptInstrumentTest, TestPortableLoadKeepsUtf8BomCompatibility) {
    const auto instrument = Instrument();
    const String_ bom(std::string("\xef\xbb\xbf", 3));
    ASSERT_EQ(LoadCalibrationInstrumentJson(bom + DumpCalibrationInstrumentJson(instrument))->InstrumentIdentity(), instrument->InstrumentIdentity());
    const auto& definition = instrument->Definition();
    ASSERT_EQ(LoadScriptInstrumentDefinitionJson(bom + DumpScriptInstrumentDefinitionJson(definition))->DefinitionIdentity(),
              definition->DefinitionIdentity());
}

TEST(ScriptInstrumentTest, TestFactoryParserErrorsOwnSourceContext) {
    for (const char* text : {"pay PAYS N^", "pay PAYS N*"}) {
        SCOPED_TRACE(text);
        AssertParseFailure(
            [&] {
                auto spec = DepositDefinition();
                spec.source_.rows_[4].text_ = text;
                static_cast<void>(NewScriptInstrumentDefinition(spec));
            },
            "/source/rows/4/text", CalibrationStage_::Value_::Input);
    }
}

TEST(ScriptInstrumentTest, TestPortableParserErrorsOwnSourceContext) {
    for (const char* text : {"pay PAYS N^", "pay PAYS N*"}) {
        SCOPED_TRACE(text);
        AssertParseFailure(
            [&] {
                const auto json = MutateArchive([&](auto& d) { d["definition"]["source"]["rows"][4]["text"].SetString(text, d.GetAllocator()); });
                static_cast<void>(LoadCalibrationInstrumentJson(json));
            },
            "/definition/source/rows/4/text", CalibrationStage_::Value_::Load);
    }
}

TEST(ScriptInstrumentTest, TestDefinitionLoadParserErrorsOwnSourceContext) {
    for (const char* text : {"pay PAYS N^", "pay PAYS N*"}) {
        SCOPED_TRACE(text);
        AssertParseFailure(
            [&] {
                const auto json = MutateArchive([&](auto& d) { d["definition"]["source"]["rows"][4]["text"].SetString(text, d.GetAllocator()); });
                rapidjson::Document document;
                document.Parse(json.c_str());
                static_cast<void>(LoadScriptInstrumentDefinitionJson(JsonText(document["definition"])));
            },
            "/source/rows/4/text", CalibrationStage_::Value_::Load);
    }
}

TEST(ScriptInstrumentTest, TestMergedEventParserFailureRetainsFailingRowAndStatement) {
    for (const char* text : {"pay PAYS N^", "pay PAYS N*"}) {
        AssertParseFailure(
            [&] {
                auto spec = DepositDefinition();
                spec.source_.rows_[3].key_ = "2027-10-21";
                spec.source_.rows_[4].text_ = text;
                static_cast<void>(NewScriptInstrumentDefinition(spec));
            },
            "/source/rows/4/text", CalibrationStage_::Value_::Input, 1);
        AssertParseFailure(
            [&] {
                const auto json = MutateArchive([&](auto& d) {
                    d["definition"]["source"]["rows"][3]["key"].SetString("2027-10-21");
                    d["definition"]["source"]["rows"][4]["text"].SetString(text, d.GetAllocator());
                });
                static_cast<void>(LoadCalibrationInstrumentJson(json));
            },
            "/definition/source/rows/4/text", CalibrationStage_::Value_::Load, 1);
    }
}

TEST(ScriptInstrumentTest, TestLegacyProductKeepsParserExceptionTypes) {
    const auto check = [](const char* text, bool typed) {
        try {
            Script::ScriptProduct_ product({Cell_("N"), Cell_(Date_(2027, 10, 21))}, {"1", text}, "PAY");
            FAIL() << "invalid legacy source was accepted";
        } catch (const CalibrationError_&) {
            FAIL() << "legacy Product acquired a calibration exception";
        } catch (const Exception_& error) {
            ASSERT_EQ(dynamic_cast<const ScriptError_*>(&error) != nullptr, typed);
        }
    };
    check("pay PAYS N^", false);
    check("pay PAYS N*", true);
}

TEST(ScriptInstrumentTest, TestBindingReturnsNewCompleteContract) {
    auto spec = DepositDefinition();
    const auto definition = NewScriptInstrumentDefinition(spec);
    const auto original = NewScriptCalibrationInstrument({"deposit", definition});
    const auto rebound = BindCalibrationInstrumentParameters(original, {{{"K", 0.03, CalibrationValueUnit_::Parse("Rate")}}, {}});
    spec.source_.rows_[1].text_ = "99";
    ASSERT_EQ(original->Binding().size(), 3);
    ASSERT_DOUBLE_EQ(original->Binding()[1].value_, 0.025);
    ASSERT_DOUBLE_EQ(rebound->Binding()[1].value_, 0.03);
    ASSERT_EQ(original->Definition()->DefinitionIdentity(), rebound->Definition()->DefinitionIdentity());
    ASSERT_NE(original->BindingIdentity(), rebound->BindingIdentity());
    ASSERT_NE(original->InstrumentIdentity(), rebound->InstrumentIdentity());
    ASSERT_EQ(definition->Spec().source_.rows_[1].text_, "0.025");
}

TEST(CalibrationProtocolTest, TestQuoteIdAlphabetAndLength) {
    const std::string initials = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    const std::string subsequent = initials + "_.:/-";
    const auto rate = CalibrationValueUnit_::Parse("Rate");
    const auto check = [&](const String_& id, bool accepted) {
        if (accepted)
            ASSERT_EQ(NewCalibrationMarketQuote(id, 0.01, rate)->Id(), id);
        else
            AssertFailure([&] { static_cast<void>(NewCalibrationMarketQuote(id, 0.01, rate)); }, "InvalidCalibrationQuote", "/id");
    };
    for (int byte = 0; byte < 256; ++byte) {
        SCOPED_TRACE(byte);
        const char c = static_cast<char>(byte);
        check(String_(1, c), initials.find(c) != std::string::npos);
        check("Q" + String_(1, c), subsequent.find(c) != std::string::npos);
    }
    check("", false);
    check(String_(128, 'A'), true);
    check(String_(129, 'A'), false);
}

TEST(ScriptInstrumentTest, TestParameterDomainBoundaries) {
    auto spec = DepositDefinition();
    spec.parameters_[1].lower_ = 0.02;
    spec.parameters_[1].upper_ = 0.03;
    spec.parameters_[1].lowerInclusive_ = spec.parameters_[1].upperInclusive_ = false;
    const auto rate = CalibrationValueUnit_::Parse("Rate");
    const auto open = NewScriptCalibrationInstrument({"open", NewScriptInstrumentDefinition(spec)});
    for (double endpoint : {0.02, 0.03})
        AssertFailure([&] { static_cast<void>(BindCalibrationInstrumentParameters(open, {{{"K", endpoint, rate}}, {}})); },
                      "InvalidCalibrationParameterBinding", "/bindings/values/0/value", String_("K"));
    spec.parameters_[1].lowerInclusive_ = spec.parameters_[1].upperInclusive_ = true;
    const auto closed = NewScriptCalibrationInstrument({"closed", NewScriptInstrumentDefinition(spec)});
    for (double endpoint : {0.02, 0.03})
        ASSERT_DOUBLE_EQ(BindCalibrationInstrumentParameters(closed, {{{"K", endpoint, rate}}, {}})->Binding()[1].value_, endpoint);
    spec.parameters_[1].lower_ = spec.parameters_[1].upper_ = 0.025;
    ASSERT_DOUBLE_EQ(NewScriptInstrumentDefinition(spec)->Defaults()[1], 0.025);
    spec.parameters_[1].upperInclusive_ = false;
    AssertFailure([&] { static_cast<void>(NewScriptInstrumentDefinition(spec)); }, "InvalidScriptInstrumentDefinition", "/parameters/1",
                  String_("K"));
}

TEST(ScriptInstrumentTest, TestArchiveArrayTypesHaveLoadPaths) {
    for (const char* path : {"/definition/source/rows", "/definition/source/settings/regression_features", "/definition/parameters",
                             "/definition/measures", "/binding/values"}) {
        SCOPED_TRACE(path);
        const auto json = MutateArchive([&](auto& d) { rapidjson::Pointer(path).Get(d)->SetObject(); });
        AssertFailure([&] { static_cast<void>(LoadCalibrationInstrumentJson(json)); }, "InvalidCalibrationArchive", path);
    }
}

TEST(ScriptInstrumentTest, TestArchiveQuoteCopyRequiresBothProvenanceFields) {
    for (const char* supplied : {"quote_id", "quote_fingerprint"}) {
        const auto json = MutateArchive([&](auto& d) {
            auto& row = d["binding"]["values"][1];
            row["origin"].SetString("QuoteCopy");
            const char* value = std::string(supplied) == "quote_id" ? "Q" : "sha256:3b95b1a70005574b4d6755b6d11d567eda1dbc082e0eb56a6ca2343406b1d471";
            row[supplied].SetString(rapidjson::StringRef(value));
        });
        AssertFailure([&] { static_cast<void>(LoadCalibrationInstrumentJson(json)); }, "InvalidCalibrationArchive",
                      "/binding/values/1/quote_fingerprint", String_("K"));
    }
}

TEST(ScriptInstrumentTest, TestScalarSourceCannotBecomePriceDefinition) {
    auto spec = DepositDefinition();
    spec.source_.rows_[3].text_ = "pay = K";
    spec.source_.rows_[4].text_ = "pay = N";
    try {
        static_cast<void>(NewScriptInstrumentDefinition(spec));
        FAIL() << "a scalar program became a Price contract";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "NoSharedPriceDefinition");
        ASSERT_EQ(error.Failure().diagnostic_.field_, std::optional<String_>("/source/rows"));
        ASSERT_TRUE(error.Failure().report_);
        ASSERT_EQ(std::string(error.what()).find("NoSharedPriceDefinition:"), 0);
    }
}

TEST(ScriptInstrumentTest, TestInvalidSourceDefaultHasDefinitionContext) {
    auto spec = DepositDefinition();
    spec.source_.rows_[0].text_ = "0";
    AssertFailure([&] { static_cast<void>(NewScriptInstrumentDefinition(spec)); }, "InvalidScriptInstrumentDefinition", "/source/rows/0/text",
                  String_("N"));
}

TEST(ScriptInstrumentTest, TestLoadDefinitionFailureRetainsEnvelopeContext) {
    const auto json = MutateArchive([](auto& d) { d["definition"]["header"]["result_currency"].SetString("EUR"); });
    try {
        static_cast<void>(LoadCalibrationInstrumentJson(json));
        FAIL();
    } catch (const CalibrationError_& error) {
        const auto& d = error.Failure().diagnostic_;
        ASSERT_EQ(d.code_, "InvalidCalibrationArchive");
        ASSERT_EQ(d.stage_, CalibrationStage_::Value_::Load);
        ASSERT_EQ(d.field_, std::optional<String_>("/definition/header"));
        ASSERT_EQ(d.instrumentId_, std::optional<String_>("deposit"));
    }
}

TEST(ScriptInstrumentTest, TestSourceNumericSpellingPreservedExactly) {
    auto spec = DepositDefinition();
    spec.source_.rows_[0].text_ = " +1";
    const auto definition = NewScriptInstrumentDefinition(spec);
    const auto instrument = NewScriptCalibrationInstrument({"spellings", definition});
    ASSERT_DOUBLE_EQ(instrument->Binding()[0].value_, 1.);
    ASSERT_EQ(std::string(definition->Spec().source_.rows_[0].text_.c_str()), " +1");
    ASSERT_NE(definition->SourceIdentity(), NewScriptInstrumentDefinition(DepositDefinition())->SourceIdentity());
    ASSERT_EQ(LoadScriptInstrumentDefinitionJson(DumpScriptInstrumentDefinitionJson(definition))->SourceIdentity(), definition->SourceIdentity());
}

TEST(ScriptInstrumentTest, TestIncrementalBindingCloneAndProvenanceIndependentIdentity) {
    const auto original = Instrument();
    const auto rate = CalibrationValueUnit_::Parse("Rate"), amount = CalibrationValueUnit_::Parse("Price:USD");
    const auto coupon = BindCalibrationInstrumentParameters(original, {{{"K", 0.03, rate}}, {}});
    const auto larger = BindCalibrationInstrumentParameters(coupon, {{{"N", 2., amount}}, {}});
    ASSERT_DOUBLE_EQ(larger->Binding()[1].value_, 0.03);
    ASSERT_DOUBLE_EQ(coupon->Binding()[0].value_, 1.);
    ASSERT_DOUBLE_EQ(original->Binding()[1].value_, 0.025);
    ASSERT_EQ(larger->Binding()[2].origin_, CalibrationBindingOrigin_::Value_::SourceDefault);
    const auto empty = BindCalibrationInstrumentParameters(larger, {});
    ASSERT_NE(empty.get(), larger.get());
    ASSERT_EQ(empty->InstrumentIdentity(), larger->InstrumentIdentity());
    ASSERT_EQ(larger->Clone()->InstrumentIdentity(), larger->InstrumentIdentity());
    ASSERT_EQ(larger->Definition()->Clone()->DefinitionIdentity(), larger->Definition()->DefinitionIdentity());
    const auto same = BindCalibrationInstrumentParameters(original, {{{"K", 0.025, rate}}, {}});
    ASSERT_EQ(same->BindingIdentity(), original->BindingIdentity());
    ASSERT_EQ(same->Binding()[1].origin_, CalibrationBindingOrigin_::Value_::Literal);
}

TEST(ScriptInstrumentTest, TestInvalidBindingsLocateParameterAndField) {
    const auto original = Instrument();
    const auto rate = CalibrationValueUnit_::Parse("Rate"), amount = CalibrationValueUnit_::Parse("Price:USD");
    const double nan = std::numeric_limits<double>::quiet_NaN(), inf = std::numeric_limits<double>::infinity();
    struct Case_ {
        CalibrationParameterBindings_ update_;
        String_ field_, key_;
    };
    const Vector_<Case_> cases = {{{{{"UNKNOWN", 1., rate}}, {}}, "/bindings/values/0/key", "UNKNOWN"},
                                  {{{{"k", 0.03, rate}}, {}}, "/bindings/values/0/key", "k"},
                                  {{{{"K", 0.03, rate}, {"K", 0.04, rate}}, {}}, "/bindings/values/1/key", "K"},
                                  {{{{"ALPHA", 1., CalibrationValueUnit_::Parse("YearFraction")}}, {}}, "/bindings/values/0/key", "ALPHA"},
                                  {{{{"K", 0.03, amount}}, {}}, "/bindings/values/0/unit", "K"},
                                  {{{{"K", nan, rate}}, {}}, "/bindings/values/0/value", "K"},
                                  {{{{"N", inf, amount}}, {}}, "/bindings/values/0/value", "N"},
                                  {{{{"N", 0., amount}}, {}}, "/bindings/values/0/value", "N"},
                                  {{{{"N", -1., amount}}, {}}, "/bindings/values/0/value", "N"}};
    for (const auto& c : cases) {
        SCOPED_TRACE(c.field_);
        AssertFailure([&] { static_cast<void>(BindCalibrationInstrumentParameters(original, c.update_)); }, "InvalidCalibrationParameterBinding",
                      c.field_, c.key_);
    }
    const auto negative = BindCalibrationInstrumentParameters(original, {{{"K", -0.01, rate}}, {}});
    ASSERT_DOUBLE_EQ(negative->Binding()[1].value_, -0.01);
    const auto zero = BindCalibrationInstrumentParameters(original, {{{"K", -0.0, rate}}, {}});
    ASSERT_FALSE(std::signbit(zero->Binding()[1].value_));
    const auto positiveZero = BindCalibrationInstrumentParameters(original, {{{"K", 0., rate}}, {}});
    ASSERT_EQ(zero->BindingIdentity(), positiveZero->BindingIdentity());
}

TEST(ScriptInstrumentTest, TestTypedSlotsPrecedeHistoricalReplayAndFolding) {
    auto spec = DepositDefinition();
    spec.source_.rows_[3].text_ = "x = N*K";
    spec.source_.rows_[4].text_ = "IF K > 0.02 THEN pay PAYS x + N*K ELSE pay PAYS -N END";
    const auto original = NewScriptCalibrationInstrument({"history", NewScriptInstrumentDefinition(spec)});
    const auto rate = CalibrationValueUnit_::Parse("Rate"), amount = CalibrationValueUnit_::Parse("Price:USD");
    const auto high = BindCalibrationInstrumentParameters(original, {{{"K", 0.03, rate}, {"N", 2., amount}}, {}});
    const auto low = BindCalibrationInstrumentParameters(original, {{{"K", 0.01, rate}, {"N", 2., amount}}, {}});
    for (const auto& [instrument, expected] :
         Vector_<std::pair<Handle_<CalibrationInstrument_>, double>>{{original, 0.05}, {high, 0.12}, {low, -2.}, {original, 0.05}}) {
        auto product = instrument->Product();
        product->PartitionEvents(Date_(2026, 10, 22));
        product->PreProcess(false, false);
        ASSERT_EQ(product->PastEventDates().size(), 1);
        AAD::Scenario_<double> scenario(1);
        scenario[0].numeraire_ = 1.;
        auto evaluator = product->BuildEvaluator<double>();
        product->Evaluate(scenario, evaluator);
        ASSERT_NEAR(evaluator.VarVals()[product->PayOffIdx()], expected, 1e-12);
        const auto compiled = product->Compile();
        auto state = product->BuildEvalState<double>();
        compiled.Evaluate(scenario, state);
        ASSERT_NEAR(state.VarVals()[product->PayOffIdx()], expected, 1e-12);
    }
    ASSERT_EQ(original->Definition()->Spec().source_.rows_[4].text_, spec.source_.rows_[4].text_);
}

TEST(ScriptInstrumentTest, TestDefinitionAndInstrumentIdentityDependencies) {
    const auto base = Instrument();
    for (int change : {0, 1, 2}) {
        auto spec = DepositDefinition();
        if (change == 0)
            spec.header_.discountRoute_ = "USD.OIS.OTHER";
        if (change == 1)
            spec.source_.rows_[4].text_ += " ";
        if (change == 2)
            spec.measures_[1].minAbsAnnuity_ = 1e-10;
        const auto definition = NewScriptInstrumentDefinition(spec);
        const auto changed = NewScriptCalibrationInstrument({"deposit", definition});
        ASSERT_NE(definition->DefinitionIdentity(), base->Definition()->DefinitionIdentity());
        ASSERT_NE(changed->BindingIdentity(), base->BindingIdentity());
        ASSERT_NE(changed->InstrumentIdentity(), base->InstrumentIdentity());
        if (change != 1)
            ASSERT_EQ(definition->SourceIdentity(), base->Definition()->SourceIdentity());
    }
    const auto pillar = NewScriptCalibrationInstrument({"deposit", base->Definition(), {}, Date_(2027, 10, 21)});
    ASSERT_EQ(pillar->BindingIdentity(), base->BindingIdentity());
    ASSERT_NE(pillar->InstrumentIdentity(), base->InstrumentIdentity());
    const auto caseId = NewScriptCalibrationInstrument({"Deposit", base->Definition()});
    ASSERT_NE(caseId->InstrumentIdentity(), base->InstrumentIdentity());
}

TEST(ScriptInstrumentTest, TestQuoteCopyIsFrozenAndRoundTripsThroughBothArchives) {
    const auto rate = CalibrationValueUnit_::Parse("Rate");
    const auto quote = NewCalibrationMarketQuote("Q", 0.025, rate);
    ASSERT_EQ(std::string(quote->Fingerprint().c_str()), "sha256:3b95b1a70005574b4d6755b6d11d567eda1dbc082e0eb56a6ca2343406b1d471");
    CalibrationParameterBindings_ request{{{"K", quote->Value(), quote->Unit()}}, {{"K", quote->Id(), quote->Fingerprint()}}};
    const auto bound = BindCalibrationInstrumentParameters(Instrument(), request);
    request.values_[0].value_ = 99.;
    static_cast<void>(NewCalibrationMarketQuote("Q", 0.09, rate));
    ASSERT_DOUBLE_EQ(bound->Binding()[1].value_, 0.025);
    ASSERT_EQ(bound->Binding()[1].origin_, CalibrationBindingOrigin_::Value_::QuoteCopy);
    ASSERT_EQ(bound->Binding()[1].quoteFingerprint_, std::optional<String_>(quote->Fingerprint()));
    const auto portable = LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(bound));
    const auto archive = handle_cast<CalibrationInstrument_>(JSON::ReadString(JSON::WriteString(*bound), false));
    for (const auto& restored : Vector_<Handle_<CalibrationInstrument_>>{portable, archive}) {
        ASSERT_TRUE(restored);
        ASSERT_NE(restored.get(), bound.get());
        ASSERT_EQ(restored->InstrumentIdentity(), bound->InstrumentIdentity());
        ASSERT_EQ(DumpCalibrationInstrumentJson(restored), DumpCalibrationInstrumentJson(bound));
        ASSERT_EQ(restored->Binding()[1].quoteId_, std::optional<String_>("Q"));
    }
    const auto definition = handle_cast<ScriptInstrumentDefinition_>(JSON::ReadString(JSON::WriteString(*bound->Definition()), false));
    ASSERT_EQ(definition->DefinitionIdentity(), bound->Definition()->DefinitionIdentity());
    const auto literal = BindCalibrationInstrumentParameters(bound, {{{"K", 0.025, rate}}, {}});
    ASSERT_EQ(literal->BindingIdentity(), bound->BindingIdentity());
    ASSERT_FALSE(literal->Binding()[1].quoteId_);
    AssertFailure([&] { static_cast<void>(BindCalibrationInstrumentParameters(bound, {{}, {{"K", "Q", quote->Fingerprint()}}})); },
                  "InvalidCalibrationParameterBinding", "/bindings/quote_copies", String_("K"));
    AssertFailure([&] { static_cast<void>(BindCalibrationInstrumentParameters(bound, {{{"K", 0.03, rate}}, {{"K", "Q", "bad"}}})); },
                  "InvalidCalibrationParameterBinding", "/bindings/quote_copies/0", String_("K"));
}

TEST(ScriptInstrumentTest, TestIndependentWireSpecimenReconstructsCompleteBinding) {
    const auto loaded = LoadCalibrationInstrumentJson(DEPOSIT_SPECIMEN);
    ASSERT_EQ(loaded->Id(), "usd-deposit-1y");
    ASSERT_EQ(loaded->Pillar(), std::optional<Date_>(Date_(2027, 10, 21)));
    ASSERT_EQ(loaded->Binding().size(), 3);
    ASSERT_DOUBLE_EQ(loaded->Binding()[1].value_, 0.03);
    ASSERT_DOUBLE_EQ(loaded->Definition()->Defaults()[1], 0.025);
    ASSERT_DOUBLE_EQ(loaded->Binding()[2].value_, 1.0138888888888888);
    ASSERT_EQ(LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(loaded))->InstrumentIdentity(), loaded->InstrumentIdentity());
    const auto definition = LoadScriptInstrumentDefinitionJson(DumpScriptInstrumentDefinitionJson(loaded->Definition()));
    ASSERT_EQ(definition->SourceIdentity(), loaded->Definition()->SourceIdentity());
    ASSERT_EQ(definition->DefinitionIdentity(), loaded->Definition()->DefinitionIdentity());
}

TEST(ScriptInstrumentTest, TestBondOptionSchemaAndPositiveStrikeDomain) {
    auto spec = DepositDefinition();
    spec.source_.rows_.erase(spec.source_.rows_.begin() + 2);
    spec.source_.rows_.erase(spec.source_.rows_.begin() + 2);
    spec.source_.rows_.back().text_ = "pay PAYS N*MAX(FIX(IR[USD,DF,2028-10-21])-K,0)";
    spec.parameters_.pop_back();
    spec.parameters_[1].role_ = ScriptParameterRole_::Value_::Strike;
    spec.parameters_[1].unit_ = CalibrationValueUnit_::Parse("BondPrice");
    spec.parameters_[1].lower_ = 0.;
    spec.parameters_[1].lowerInclusive_ = false;
    spec.measures_.pop_back();
    const auto option = NewScriptCalibrationInstrument({"bond-call", NewScriptInstrumentDefinition(spec)});
    ASSERT_EQ(LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(option))->InstrumentIdentity(), option->InstrumentIdentity());
    AssertFailure(
        [&] { static_cast<void>(BindCalibrationInstrumentParameters(option, {{{"K", 0., CalibrationValueUnit_::Parse("BondPrice")}}, {}})); },
        "InvalidCalibrationParameterBinding", "/bindings/values/0/value", String_("K"));
}

TEST(ScriptInstrumentTest, TestDefinitionRejectsDuplicateKeysUnitsAndNonfiniteDefaults) {
    {
        auto spec = DepositDefinition();
        spec.source_.rows_[1].key_ = "N";
        AssertFailure([&] { static_cast<void>(NewScriptInstrumentDefinition(spec)); }, "InvalidScriptInstrumentDefinition", "/source/rows/1/key",
                      String_("N"));
    }
    {
        auto spec = DepositDefinition();
        spec.parameters_[1].key_ = "N";
        AssertFailure([&] { static_cast<void>(NewScriptInstrumentDefinition(spec)); }, "InvalidScriptInstrumentDefinition", "/parameters/1/key",
                      String_("N"));
    }
    {
        auto spec = DepositDefinition();
        spec.parameters_[1].unit_ = CalibrationValueUnit_::Parse("BondPrice");
        AssertFailure([&] { static_cast<void>(NewScriptInstrumentDefinition(spec)); }, "InvalidScriptInstrumentDefinition", "/parameters/1/unit",
                      String_("K"));
    }
    for (const String_ value : {"nan", "inf", "1e999", "text"}) {
        auto spec = DepositDefinition();
        spec.source_.rows_[1].text_ = value;
        AssertFailure([&] { static_cast<void>(NewScriptInstrumentDefinition(spec)); }, "InvalidScriptInstrumentDefinition", "/source/rows/1/text",
                      String_("K"));
    }
}

TEST(CalibrationProtocolTest, TestQuoteUnitsConventionsAndFiniteValues) {
    const auto rate = CalibrationValueUnit_::Parse("Rate");
    ASSERT_DOUBLE_EQ(NewCalibrationMarketQuote("negative", -0.01, rate)->Value(), -0.01);
    ASSERT_EQ(NewCalibrationMarketQuote("rate", 0.01, rate)->Convention().id_, CalibrationQuoteConventionId_::Value_::DecimalRate);
    ASSERT_DOUBLE_EQ(NewCalibrationMarketQuote("price", -10., CalibrationValueUnit_::Parse("Price:USD"))->Value(), -10.);
    ASSERT_EQ(CalibrationValueUnit_::Parse("FxOutright:EUR/USD").Token(), "FxOutright:EUR/USD");
    for (const String_ unit : {"rate", "Price", "Price:usd", "FxOutright:USD/USD", "FxOutright:USD/EUR/GBP"})
        AssertFailure([&] { static_cast<void>(CalibrationValueUnit_::Parse(unit)); }, "InvalidCalibrationQuote", "/unit");
    for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        AssertFailure([&] { static_cast<void>(NewCalibrationMarketQuote("Q", value, rate)); }, "InvalidCalibrationQuote", "/value");
    AssertFailure([&] { static_cast<void>(NewCalibrationMarketQuote("Q", 0., CalibrationValueUnit_::Parse("FxOutright:EUR/USD"))); },
                  "InvalidCalibrationQuote", "/value");
    AssertFailure(
        [&] {
            static_cast<void>(
                NewCalibrationMarketQuote("Q", 1., rate, CalibrationQuoteConvention_{CalibrationQuoteConventionId_::Value_::DecimalRate, 2}));
        },
        "InvalidCalibrationQuote", "/convention");
    AssertFailure([&] { static_cast<void>(NewCalibrationMarketQuote("Q", 1., CalibrationValueUnit_::Parse("BondPrice"))); },
                  "InvalidCalibrationQuote", "/unit");
}

TEST(CalibrationProtocolTest, TestObservationOrdinalsAndDistinctQuoteAxis) {
    const auto a = Instrument("I1"), b = Instrument("I2");
    const auto rate = CalibrationValueUnit_::Parse("Rate");
    const auto q = NewCalibrationMarketQuote("Q", 0.03, rate), equal = NewCalibrationMarketQuote("Q", 0.03, rate);
    const auto caseQ = NewCalibrationMarketQuote("q", 0.04, rate);
    const auto o1 = NewCalibrationObservation("o1", a, "ParRate", q, 1e-8);
    const auto o2 = NewCalibrationObservation("o2", b, "ParRate", equal, 2e-8);
    const auto o3 = NewCalibrationObservation("o3", a, "ParRate", caseQ, 3e-8);
    const auto axes = BuildCalibrationObservationAxes({o1, o2, o3});
    ASSERT_EQ(axes.quoteIndex_, Vector_<int>({0, 0, 1}));
    ASSERT_EQ(axes.quotes_.size(), 2);
    ASSERT_EQ(std::string(axes.quotes_[0]->Id().c_str()), "Q");
    ASSERT_EQ(std::string(axes.quotes_[1]->Id().c_str()), "q");
    ASSERT_EQ(axes.quoteMap_.Rows(), 3);
    ASSERT_EQ(axes.quoteMap_.Cols(), 2);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
            ASSERT_DOUBLE_EQ(axes.quoteMap_(i, j), axes.quoteIndex_[i] == j ? 1. : 0.);
    const auto reordered = BuildCalibrationObservationAxes({o3, o2, o1});
    ASSERT_EQ(reordered.quoteIndex_, Vector_<int>({0, 1, 1}));
    ASSERT_EQ(reordered.observations_[0]->Id(), "o3");
    const CalibrationReport_ report(CalibrationStage_::Value_::Input, axes);
    ASSERT_EQ(report.Axes().observations_[1]->Id(), "o2");
    AssertFailure([&] { static_cast<void>(JSON::WriteString(report)); }, "CalibrationPersistenceUnsupported", "");
}

TEST(CalibrationProtocolTest, TestObservationConflictsAndScaleLocateObjects) {
    const auto instrument = Instrument();
    const auto rate = CalibrationValueUnit_::Parse("Rate");
    const auto q = NewCalibrationMarketQuote("Q", 0.03, rate);
    const auto obs = NewCalibrationObservation("o", instrument, "ParRate", q, 1e-8);
    for (double scale : {0., -1., std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        AssertFailure([&] { static_cast<void>(NewCalibrationObservation("o", instrument, "ParRate", q, scale)); }, "InvalidCalibrationObservation",
                      "/error_scale");
    AssertFailure([&] { static_cast<void>(NewCalibrationObservation("o", instrument, "parrate", q, 1e-8)); }, "UnsupportedCalibrationMeasure",
                  "/measure_id");
    const auto wrongUnit = NewCalibrationMarketQuote("Q", 0.03, CalibrationValueUnit_::Parse("Price:EUR"));
    AssertFailure([&] { static_cast<void>(NewCalibrationObservation("o", instrument, "Price", wrongUnit, 1e-8)); }, "InvalidCalibrationObservation",
                  "/quote/unit");
    AssertFailure([&] { static_cast<void>(BuildCalibrationObservationAxes({obs, obs})); }, "InvalidCalibrationObservation", "/observations/1/id");
    const auto changedQuote = NewCalibrationMarketQuote("Q", 0.04, rate);
    const auto other = NewCalibrationObservation("other", instrument, "ParRate", changedQuote, 1e-8);
    try {
        static_cast<void>(BuildCalibrationObservationAxes({obs, other}));
        FAIL();
    } catch (const CalibrationError_& error) {
        const auto& d = error.Failure().diagnostic_;
        ASSERT_EQ(d.field_, std::optional<String_>("/observations/1/quote/id"));
        ASSERT_EQ(d.observationId_, std::optional<String_>("other"));
        ASSERT_EQ(d.instrumentId_, std::optional<String_>("deposit"));
        ASSERT_EQ(d.quoteId_, std::optional<String_>("Q"));
    }
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"K", 0.05, rate}}, {}});
    const auto changed = NewCalibrationObservation("changed", rebound, "ParRate", q, 1e-8);
    AssertFailure([&] { static_cast<void>(BuildCalibrationObservationAxes({obs, changed})); }, "InvalidCalibrationObservation",
                  "/observations/1/instrument/id");
}

TEST(ScriptInstrumentTest, TestArchiveRejectsMissingExtraMistypedAndInconsistentData) {
    const auto check = [](const auto& mutate, const String_& field) {
        const auto json = MutateArchive(mutate);
        AssertFailure([&] { static_cast<void>(LoadCalibrationInstrumentJson(json)); }, "InvalidCalibrationArchive", field);
    };
    check([](auto& d) { d["binding"]["values"][1]["value"].SetBool(true); }, "/binding/values/1/value");
    check([](auto& d) { d["binding"]["values"][1]["unit"].SetString("BondPrice"); }, "/binding/values/1/unit");
    check([](auto& d) { d["binding"]["values"][1]["value"].SetDouble(0.09); }, "/binding/values/1/value");
    check([](auto& d) { d["binding"]["values"][2]["origin"].SetString("Literal"); }, "/binding/values/2/origin");
    check([](auto& d) { d["binding"]["values"].PopBack(); }, "/binding/values");
    check([](auto& d) { d["binding"]["values"][0].Swap(d["binding"]["values"][1]); }, "/binding/values/0/key");
    check([](auto& d) { d.RemoveMember("pillar"); }, "/pillar");
    check([](auto& d) { d.AddMember("extra", true, d.GetAllocator()); }, "/extra");
    check([](auto& d) { d["definition"]["parameters"][1].AddMember("default_value", 0.04, d.GetAllocator()); },
          "/definition/parameters/1/default_value");
    check([](auto& d) { d["definition"]["parameters"][1]["type"].SetString("real"); }, "/definition/parameters/1/type");
    check([](auto& d) { d["definition"]["source"]["rows"][3]["key"].SetString("2026-02-30"); }, "/definition/source/rows/3/key");
    for (const char* version : {"language", "semantics"}) {
        const auto json = MutateArchive([&](auto& d) { d["definition"][version].SetString("unknown/9"); });
        AssertFailure([&] { static_cast<void>(LoadCalibrationInstrumentJson(json)); }, "UnsupportedCalibrationSchema",
                      "/definition/" + String_(version));
    }
    const auto json = MutateArchive([](auto& d) { d["schema"].SetString("dal.calibration-instrument/9"); });
    AssertFailure([&] { static_cast<void>(LoadCalibrationInstrumentJson(json)); }, "UnsupportedCalibrationSchema", "/schema");
}

TEST(ScriptInstrumentTest, TestArchiveRejectsDuplicatePropertiesAndInvalidText) {
    const auto original = DumpCalibrationInstrumentJson(Instrument());
    String_ duplicate = original;
    duplicate.insert(1, R"("pillar":null,)");
    AssertFailure([&] { static_cast<void>(LoadCalibrationInstrumentJson(duplicate)); }, "InvalidCalibrationArchive", "/pillar");
    const auto position = original.find(R"("binding":{)");
    ASSERT_NE(position, String_::npos);
    duplicate = original;
    duplicate.insert(position + 11, R"("values":[],)");
    AssertFailure([&] { static_cast<void>(LoadCalibrationInstrumentJson(duplicate)); }, "InvalidCalibrationArchive", "/binding/values");
    const auto nul = MutateArchive([](auto& d) { d["definition"]["source"]["rows"][3]["text"].SetString("pay\0PAYS", 8); });
    AssertFailure([&] { static_cast<void>(LoadCalibrationInstrumentJson(nul)); }, "InvalidCalibrationArchive", "/definition/source/rows/3/text");
    const String_ invalidUtf8(std::string("\xff", 1));
    AssertFailure([&] { static_cast<void>(LoadCalibrationInstrumentJson(invalidUtf8)); }, "InvalidCalibrationArchive", "");
}

TEST(CalibrationProtocolTest, TestFailurePayloadOutlivesInputsAndContainsOffendingValue) {
    std::optional<CalibrationError_> saved;
    {
        const auto instrument = Instrument();
        const auto q = NewCalibrationMarketQuote("Q", 0.03, CalibrationValueUnit_::Parse("Rate"));
        try {
            static_cast<void>(NewCalibrationObservation("bad-scale", instrument, "ParRate", q, 0.));
            FAIL();
        } catch (const CalibrationError_& error) {
            saved.emplace(error);
        }
    }
    ASSERT_TRUE(saved);
    ASSERT_EQ(saved->Failure().diagnostic_.observationId_, std::optional<String_>("bad-scale"));
    ASSERT_EQ(saved->Failure().diagnostic_.actual_, std::optional<String_>("0"));
    ASSERT_EQ(saved->Failure().report_->Errors()[0].quoteId_, std::optional<String_>("Q"));
    ASSERT_EQ(std::string(saved->what()).find("InvalidCalibrationObservation:"), 0);
}

TEST(CalibrationProtocolTest, TestRejectedTextProducesSafeOwnedDiagnostics) {
    const String_ badId(std::string("Q\0bad", 5));
    try {
        static_cast<void>(NewCalibrationMarketQuote(badId, 0.03, CalibrationValueUnit_::Parse("Rate")));
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.quoteId_->find('\0'), String_::npos);
        ASSERT_EQ(error.Failure().diagnostic_.actual_->find('\0'), String_::npos);
    }
}

TEST(ScriptInstrumentTest, TestDalArchiveRequiresPortableJsonStringWithLoadContext) {
    for (const String_ blob : {R"({"~type":"CalibrationInstrument_v1","json":true})", R"({"~type":"ScriptInstrumentDefinition_v1"})"}) {
        try {
            static_cast<void>(JSON::ReadString(blob, true));
            FAIL();
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationArchive");
            ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Load);
            ASSERT_EQ(error.Failure().diagnostic_.field_, std::optional<String_>("/json"));
        }
    }
}
