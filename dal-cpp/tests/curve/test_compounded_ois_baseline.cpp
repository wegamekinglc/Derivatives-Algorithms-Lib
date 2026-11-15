#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/math/matrix/linearsolveaccuracy.hpp>

#include "compoundedoisfixture.hpp"

using namespace Dal;
using namespace Dal::CompoundedOisTest;

namespace {
    std::filesystem::path GoldenPath(const char* name) { return std::filesystem::path(__FILE__).parent_path() / "data" / name; }
    std::string ReadGolden(std::istream* input) {
        std::string contents{std::istreambuf_iterator<char>(*input), std::istreambuf_iterator<char>()};
        if (!contents.empty() && contents.back() == '\n') {
            contents.pop_back();
            if (!contents.empty() && contents.back() == '\r')
                contents.pop_back();
        }
        return contents;
    }
    std::string ReadGolden(const char* name) {
        std::ifstream input(GoldenPath(name), std::ios::binary);
        return ReadGolden(&input);
    }
    String_ Identities(const Handle_<PreparedCompoundedOis_>& tables, const Handle_<CalibrationInstrument_>& instrument) {
        using namespace CalibrationInternal;
        return String_(Jcs(Obj({{"calendar", Str(tables->CalendarIdentity())},
                                {"contract", Str(tables->ContractIdentity())},
                                {"preparation", Str(tables->PreparationIdentity())},
                                {"source", Str(instrument->Definition()->SourceIdentity())},
                                {"definition", Str(instrument->Definition()->DefinitionIdentity())},
                                {"binding", Str(instrument->BindingIdentity())}})));
    }
} // namespace

TEST(CompoundedOisBaselineTest, TestGoldenReaderAcceptsLfAndCrlf) {
    const std::string canonical = R"({"source":"PAY PAYS N","identity":"sha256:abc"})";
    for (const auto* ending : {"", "\n", "\r\n"}) {
        std::istringstream input(canonical + ending);
        ASSERT_EQ(ReadGolden(&input), canonical);
    }
    std::istringstream empty;
    ASSERT_TRUE(ReadGolden(&empty).empty());
}

TEST(CompoundedOisBaselineTest, TestGoldenReaderPreservesContentDifferences) {
    const std::string canonical = R"({"source":"PAY PAYS N","identity":"sha256:abc"})";
    for (const auto& different :
         {R"({"source":"PAY PAYS N+1","identity":"sha256:abc"})", R"({"source":"PAY PAYS N","identity":"sha256:abd"})",
          R"({"source": "PAY PAYS N","identity":"sha256:abc"})", "{\"source\":\"PAY PAYS N\",\r\n\"identity\":\"sha256:abc\"}"}) {
        std::istringstream input(std::string(different) + "\r\n");
        const auto actual = ReadGolden(&input);
        ASSERT_EQ(actual, different);
        ASSERT_NE(actual, canonical);
    }
    for (const auto* suffix : {"\r", " ", "\n\n", "\n\r\n"}) {
        std::istringstream input(canonical + suffix);
        ASSERT_NE(ReadGolden(&input), canonical);
    }
}

TEST(CompoundedOisBaselineTest, TestLegacyArchiveSourceAndIdentityBaseline) {
    for (const auto* name : {"compounded_ois_v1_prepared.json", "compounded_ois_v1_instrument.json", "compounded_ois_v1_identities.json"}) {
        ASSERT_TRUE(std::filesystem::is_regular_file(GoldenPath(name))) << GoldenPath(name);
        ASSERT_FALSE(ReadGolden(name).empty());
    }
    const auto tables = PrepareCompoundedOis(Contract(), Valuation());
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    const auto preparedJson = ReadGolden("compounded_ois_v1_prepared.json");
    const auto instrumentJson = ReadGolden("compounded_ois_v1_instrument.json");
    ASSERT_EQ(CalibrationInternal::Bytes(DumpPreparedCompoundedOisJson(tables)), preparedJson);
    ASSERT_EQ(CalibrationInternal::Bytes(DumpCalibrationInstrumentJson(instrument)), instrumentJson);
    ASSERT_EQ(CalibrationInternal::Bytes(Identities(tables, instrument)), ReadGolden("compounded_ois_v1_identities.json"));
    const auto loaded = LoadPreparedCompoundedOisJson(String_(preparedJson));
    ASSERT_EQ(DumpPreparedCompoundedOisJson(loaded), DumpPreparedCompoundedOisJson(tables));
    ASSERT_EQ(Identities(loaded, NewCompoundedOisScriptInstrument(loaded)), Identities(tables, instrument));
    ASSERT_EQ(DumpCalibrationInstrumentJson(LoadCalibrationInstrumentJson(String_(instrumentJson))), DumpCalibrationInstrumentJson(instrument));
}

TEST(CompoundedOisBaselineTest, TestThreeSquareTargetRolesFitIndependentQuotesAndRetainSnapshotRejection) {
    for (const auto* role : {"Overnight", "Discount", "Shared"}) {
        SCOPED_TRACE(role);
        const auto spec = Calibration(role);
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        const auto fit = CalibrateCurveInstruments(spec, options);
        ASSERT_TRUE(fit.fitWithinTolerance_);
        ASSERT_TRUE(fit.evidence_);
        ASSERT_EQ(fit.parameters_.size(), 2u);
        ASSERT_EQ(fit.axes_.quotes_.size(), 2u);
        ASSERT_EQ(fit.freeParameters_.size(), 2u);
        for (int i = 0; i < 2; ++i) {
            ASSERT_NEAR(fit.parameters_[i], Parameters()[i], 1e-8);
            ASSERT_LE(std::abs(fit.rawResiduals_[i]), 1e-12);
        }
        const auto evaluated = PreparedCurveInstruments_(spec, options).Evaluate(fit.parameters_);
        SquareMatrix_<> jacobian(2, 0.);
        Matrix_<> rhs(2, 2, 0.);
        for (int i = 0; i < 2; ++i) {
            rhs(i, i) = 1.;
            for (int j = 0; j < 2; ++j)
                jacobian(i, j) = evaluated.quoteJacobian_(i, j);
        }
        const CheckedLinearSolve_ checked(jacobian, rhs, {1e-12, 1e-12});
        ASSERT_GE(checked.Diagnostics().reciprocalConditionInfinity_, 1e-10);
        auto request = Request(spec);
        const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[1]);
        const auto expected = Oracle(String_(role) == "Discount" ? BackgroundParameters() : Parameters(),
                                     String_(role) == "Overnight" ? BackgroundParameters() : Parameters());
        for (const auto method : {CalibrationDerivative_("AAD"), CalibrationDerivative_("FiniteDifference")}) {
            request.derivativeMethod_ = method;
            const auto result = EvaluateCurveScriptInstrument(instrument, request);
            ASSERT_EQ(result.values_.size(), 2u);
            ASSERT_NEAR(result.values_[0], expected[0], 1e-10);
            ASSERT_NEAR(result.values_[1], expected[1], 1e-10);
        }
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-12, 1e-12}, 1e-10, 1e-12});
        ASSERT_FALSE(snapshot.RiskAvailable());
        ASSERT_FALSE(snapshot.MappingAvailable());
        ASSERT_EQ(snapshot.Diagnostics().size(), 1u);
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedCapability");
        ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/observations");
    }
}

TEST(CompoundedOisBaselineTest, TestPerPvRejectsOisAgainstAvailableDepositSnapshot) {
    auto spec = Calibration("Discount");
    spec.observations_ = {spec.observations_[0]};
    spec.geometry_.knotDates_ = {Date_(2024, 3, 1), Date_(2024, 3, 20)};
    spec.solver_.initialGuessPerNode_ = {-.0015};
    const auto fit = CalibrateCurveInstruments(spec);
    ASSERT_TRUE(fit.fitWithinTolerance_);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-12}, 1e-10, 1e-12});
    ASSERT_TRUE(snapshot.RiskAvailable());
    ASSERT_TRUE(snapshot.MappingAvailable());
    const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
    const auto result = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
    ASSERT_FALSE(result.Available());
    ASSERT_EQ(result.Diagnostics().size(), 1u);
    ASSERT_EQ(result.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedCapability");
    ASSERT_EQ(result.Diagnostics()[0].field_, "/instrument");
}

TEST(CompoundedOisBaselineTest, TestV2RuleRequiresAnExplicitConvention) {
    auto spec = Contract();
    spec.ruleId_ = "dal.compounded-ois-cashflows/2";
    try {
        PrepareCompoundedOis(spec, Valuation());
        FAIL() << "Unimplemented rule accepted";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "UnsupportedCompoundedOisRule");
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/observation_convention");
    }
}
