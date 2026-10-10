//
// Created by wegam on 2026/7/19.
//

#include <gtest/gtest.h>

#include <string>

// Parse DAL's VOID enum before Windows.h defines its VOID macro.
#include <dal-excel/src/__excel_test_api.hpp>
#include <dal-excel/src/__registrationhelp.hpp>

namespace {
    std::string CaseSensitive(const Dal::String_& value) { return std::string(value.c_str()); }
} // namespace

TEST(ExcelRegistrationPortableTest, TestCaseSensitiveComparisonRejectsLowercaseExcelName) {
    ASSERT_NE(CaseSensitive(Dal::String_("rateportfolioquoterisk.spill")), "RATEPORTFOLIOQUOTERISK.SPILL");
}

TEST(ExcelRegistrationPortableTest, TestGeneratedFormatHelpPreservesExplicitMetadata) {
    const Dal::Vector_<Dal::String_> source{"Source handle"};
    for (const auto* names : {"settings,format", "request,format", "plan,format"}) {
        const auto help = Dal::Excel::RegistrationHelp(names, source);
        ASSERT_EQ(help.size(), 2);
        ASSERT_EQ(help.front(), source.front());
        ASSERT_EQ(help.back(), "Desired screen layout of outputs; blank uses the default layout");
    }
    ASSERT_EQ(source.size(), 1);
    const Dal::Vector_<Dal::String_> complete{"Source handle", "Custom format help"};
    ASSERT_EQ(Dal::Excel::RegistrationHelp("settings,format", complete), complete);
    ASSERT_EQ(Dal::Excel::RegistrationHelp("settings", source), source);
    ASSERT_EQ(Dal::Excel::RegistrationHelp("first,second,format", source), source);
    ASSERT_TRUE(Dal::Excel::RegistrationHelp("", {}).empty());
}

#ifdef _WIN32

#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <set>

using namespace Dal;

namespace {
    int DeclaredArgCount(const ExcelFuncRegistration_& reg) {
        // trailing Excel type-text modifiers (e.g. '!' for volatile) declare no argument
        String_ types = reg.argTypes_;
        while (!types.empty() && (types.back() == '!' || types.back() == '#' || types.back() == '$'))
            types.pop_back();
        return static_cast<int>(types.size()) - 1;
    }

    int NamedArgCount(const ExcelFuncRegistration_& reg) {
        if (reg.argNames_.empty())
            return 0;
        return 1 + static_cast<int>(std::count(reg.argNames_.begin(), reg.argNames_.end(), ','));
    }

    std::string UpperDotted(const String_& cName) {
        std::string retval(cName.begin(), cName.end());
        for (auto& ch : retval)
            ch = ch == '_' ? '.' : static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return retval;
    }
} // namespace

TEST(ExcelRegistrationTest, TestRegistrationTableIsPopulated) { ASSERT_GE(RegisteredFunctionsForTest().size(), 67); }

TEST(ExcelRegistrationTest, TestEuropeanPdeFunctionsRetainTypedArgumentsAndHelp) {
    const std::pair<const char*, const char*> contracts[] = {{"EuropeanPdeRiskSettings_New", "name,[settings]"},
                                                             {"EuropeanPdeRiskSettings_Get_Configuration", "settings"},
                                                             {"EuropeanPdeRiskRequest_New", "name,rate,volatility,strike,[settings]"},
                                                             {"EuropeanPdeRiskRequest_Get_Settings", "request"},
                                                             {"EuropeanPdeRiskRequest_Get_Point", "request"},
                                                             {"EuropeanPdeRiskResult_New", "name,request"},
                                                             {"EuropeanPdeRiskResult_Get_Request", "result"},
                                                             {"EuropeanPdeRiskResult_Get_Prices", "result"},
                                                             {"EuropeanPdeRiskResult_Get_Jacobian", "result"},
                                                             {"EuropeanPdeRiskResult_Get_Grid", "result"},
                                                             {"EuropeanPdeRiskResult_Get_ForwardErrors", "result"},
                                                             {"EuropeanPdeRiskResult_Get_TransposeErrors", "result"},
                                                             {"EuropeanPdeRiskResult_Get_Execution", "result"}};
    const auto registrations = RegisteredFunctionsForTest();
    for (const auto& contract : contracts) {
        const auto name = String_("xl_") + contract.first;
        const auto found = std::find_if(registrations.begin(), registrations.end(), [&](const auto& reg) { return reg.cName_ == name; });
        ASSERT_NE(found, registrations.end()) << contract.first;
        ASSERT_EQ(CaseSensitive(found->xlName_), UpperDotted(contract.first));
        ASSERT_EQ(CaseSensitive(found->argNames_), contract.second);
        ASSERT_EQ(CaseSensitive(found->argTypes_), std::string(static_cast<size_t>(NamedArgCount(*found)) + 1, 'Q'));
        ASSERT_FALSE(found->volatile_);
        ASSERT_FALSE(found->help_.empty());
        ASSERT_EQ(found->argHelpCount_, NamedArgCount(*found));
        ASSERT_LE(found->maxArgHelpLength_, 255);
    }
}

TEST(ExcelRegistrationTest, TestRateCurvatureFunctionsRetainTypedArgumentsAndHelp) {
    const std::pair<const char*, const char*> contracts[] = {
        {"RateCalibration_New", "name,source"},
        {"RateCalibration_Recalibrate", "name,calibration,quotes"},
        {"RateCalibration_Get_Point", "calibration"},
        {"RateCalibration_Get_Parameters", "calibration"},
        {"RateCalibration_Get_QuotePlan", "calibration"},
        {"RateTradeQuoteCurvatureSettings_New", "name,[weights],[fixings]"},
        {"RateTradeQuoteCurvatureSettings_Get_Weights", "settings"},
        {"RateTradeQuoteCurvatureSettings_Get_Fixings", "settings"},
        {"RateTradeQuoteCurvatureResult_New", "name,trades,calibration,bumps,[settings]"},
        {"RateTradeQuoteCurvatureResult_Get_Value", "result"},
        {"RateTradeQuoteCurvatureResult_Get_Currency", "result"},
        {"RateTradeQuoteCurvatureResult_Get_Point", "result"},
        {"RateTradeQuoteCurvatureResult_Get_Gradient", "result"},
        {"RateTradeQuoteCurvatureResult_Get_Directions", "result"},
        {"RateTradeQuoteCurvatureResult_Get_Steps", "result"},
        {"RateTradeQuoteCurvatureResult_Get_HessianProducts", "result"},
        {"RateTradeQuoteCurvatureResult_Get_Shape", "result"},
        {"RateTradeQuoteCurvatureResult_Get_Execution", "result"},
        {"RateTradeQuoteCurvatureResult_Get_BaseCalibration", "result"},
    };
    const auto registrations = RegisteredFunctionsForTest();
    for (const auto& contract : contracts) {
        const auto name = String_("xl_") + contract.first;
        const auto found = std::find_if(registrations.begin(), registrations.end(), [&](const auto& reg) { return reg.cName_ == name; });
        ASSERT_NE(found, registrations.end()) << contract.first;
        ASSERT_EQ(CaseSensitive(found->xlName_), UpperDotted(contract.first));
        ASSERT_EQ(CaseSensitive(found->argNames_), contract.second);
        ASSERT_EQ(CaseSensitive(found->argTypes_), std::string(static_cast<size_t>(NamedArgCount(*found)) + 1, 'Q'));
        ASSERT_FALSE(found->volatile_);
        ASSERT_FALSE(found->help_.empty());
        ASSERT_EQ(found->argHelpCount_, NamedArgCount(*found));
        ASSERT_LE(found->maxArgHelpLength_, 255);
    }
}

TEST(ExcelRegistrationTest, TestDupireCurvatureFunctionsRetainTypedArgumentsAndHelp) {
    const std::pair<const char*, const char*> contracts[] = {
        {"BumpOverAADRequest_New", "name,[directions],[steps],[settings]"},
        {"BumpOverAADRequest_Get_Directions", "request"},
        {"BumpOverAADRequest_Get_Steps", "request"},
        {"BumpOverAADRequest_Get_Settings", "request"},
        {"BumpOverAADRequest_Get_Shape", "request"},
        {"DupireScriptCurvatureRequest_New", "name,risk,bumps"},
        {"DupireScriptCurvatureRequest_Get_Risk", "request"},
        {"DupireScriptCurvatureRequest_Get_Bumps", "request"},
        {"DupireScriptCurvaturePlan_New", "name,product,modelData,calibration,component,request"},
        {"DupireScriptCurvaturePlan_Get_BasePlan", "plan"},
        {"DupireScriptCurvaturePlan_Get_Point", "plan"},
        {"DupireScriptCurvaturePlan_Get_Directions", "plan"},
        {"DupireScriptCurvaturePlan_Get_Steps", "plan"},
        {"DupireScriptCurvaturePlan_Get_Shape", "plan"},
        {"DupireScriptCurvaturePlan_Get_Payload", "plan"},
        {"DupireScriptCurvatureResult_New", "name,plan"},
        {"DupireScriptCurvatureResult_Get_Base", "result"},
        {"DupireScriptCurvatureResult_Get_QuotePlan", "result"},
        {"DupireScriptCurvatureResult_Get_Point", "result"},
        {"DupireScriptCurvatureResult_Get_Gradient", "result"},
        {"DupireScriptCurvatureResult_Get_Directions", "result"},
        {"DupireScriptCurvatureResult_Get_Steps", "result"},
        {"DupireScriptCurvatureResult_Get_HessianProducts", "result"},
        {"DupireScriptCurvatureResult_Get_Shape", "result"},
        {"DupireScriptCurvatureResult_Get_Execution", "result"}};
    const auto registrations = RegisteredFunctionsForTest();
    for (const auto& contract : contracts) {
        const auto name = String_("xl_") + contract.first;
        const auto found = std::find_if(registrations.begin(), registrations.end(), [&](const auto& reg) { return reg.cName_ == name; });
        ASSERT_NE(found, registrations.end()) << contract.first;
        ASSERT_EQ(CaseSensitive(found->xlName_), UpperDotted(contract.first));
        ASSERT_EQ(CaseSensitive(found->argNames_), contract.second);
        ASSERT_EQ(CaseSensitive(found->argTypes_), std::string(static_cast<size_t>(NamedArgCount(*found)) + 1, 'Q'));
        ASSERT_FALSE(found->volatile_);
        ASSERT_FALSE(found->help_.empty());
        ASSERT_EQ(found->argHelpCount_, NamedArgCount(*found));
        ASSERT_LE(found->maxArgHelpLength_, 255);
    }
}

TEST(ExcelRegistrationTest, TestScriptSettingsAndLegacyContracts) {
    struct Contract_ {
        const char* name_;
        const char* arguments_;
        const char* types_;
    };
    const Contract_ contracts[] = {{"ScriptProductSettings_New", "name,[settings]", "QQQ"},
                                   {"Product_NewWithSettings", "name,dates,events,settings", "QQQQQ"},
                                   {"ScriptValuationSettings_New", "name,[settings],[fixings]", "QQQQ"},
                                   {"MonteCarloSettings_New", "name,[settings]", "QQQ"},
                                   {"MonteCarlo_ValueWithSettings", "product,modelData,n_paths,[valuation],[simulation]", "QQQQQQ"},
                                   {"Product_Describe", "product", "QQ"},
                                   {"ScriptValuation_Explain", "product,modelData,[valuation]", "QQQQ"},
                                   {"ScriptSimulation_Explain", "product,modelData,n_paths,[valuation],[simulation]", "QQQQQQ"},
                                   {"Product_New", "name,dates,events", "QQQQ"},
                                   {"MonteCarlo_Value", "product,modelData,n_paths,rsg,use_bb,enable_aad,smooth", "QQQQQQQQ"},
                                   {"MarketFixingSnapshot_New", "[indexNames],[fixingTimes],[values]", "QQQQ"}};
    const auto registrations = RegisteredFunctionsForTest();
    for (const auto& contract : contracts) {
        const auto name = String_("xl_") + contract.name_;
        const auto found = std::find_if(registrations.begin(), registrations.end(), [&](const auto& reg) { return reg.cName_ == name; });
        ASSERT_NE(found, registrations.end()) << contract.name_;
        ASSERT_EQ(CaseSensitive(found->xlName_), UpperDotted(contract.name_));
        ASSERT_EQ(CaseSensitive(found->argNames_), contract.arguments_);
        ASSERT_EQ(CaseSensitive(found->argTypes_), contract.types_);
        ASSERT_FALSE(found->volatile_);
        ASSERT_FALSE(found->help_.empty());
        ASSERT_LE(found->maxArgHelpLength_, 255);
    }
}

TEST(ExcelRegistrationTest, TestPortfolioRiskFunctionsRetainTypedArgumentsAndHelp) {
    const std::pair<const char*, const char*> contracts[]{
        {"ScriptPortfolio_New", "name,trades"},
        {"PortfolioWeightedRiskRequest_New", "name,[settings]"},
        {"PortfolioJacobianRiskRequest_New", "name,[settings]"},
        {"PortfolioMonteCarlo_ValueWithWeightedRisk", "portfolio,n_paths,[request],[valuation],[simulation]"},
        {"PortfolioMonteCarlo_ValueWithJacobianRisk", "portfolio,n_paths,[request],[valuation],[simulation]"},
        {"PortfolioRiskResult_Get_Objective", "result"},
        {"PortfolioRiskResult_Get_Values", "result"},
        {"PortfolioRiskResult_Get_Jacobian", "result,[reported]"},
        {"PortfolioRiskResult_Get_Shape", "result"},
        {"PortfolioRiskResult_Get_Outputs", "result,[complete]"},
        {"PortfolioRiskResult_Get_Inputs", "result,[complete]"},
        {"PortfolioRiskResult_Get_Execution", "result"},
        {"PortfolioRiskResult_Get_Sampling", "result"},
        {"PortfolioRiskResult_Get_Provenance", "result"},
        {"PortfolioRiskResult_Get_Trades", "result"},
        {"PortfolioRiskResult_Get_TradeProvenance", "result,trade"},
        {"PortfolioRiskResult_Get_History", "result,trade"},
        {"PortfolioRiskResult_Get_Product", "result,trade"},
        {"PortfolioRiskResult_Get_ModelSnapshot", "result,trade"}};
    const auto registrations = RegisteredFunctionsForTest();
    for (const auto& contract : contracts) {
        const auto name = String_("xl_") + contract.first;
        const auto found = std::find_if(registrations.begin(), registrations.end(), [&](const auto& reg) { return reg.cName_ == name; });
        ASSERT_NE(found, registrations.end()) << contract.first;
        ASSERT_EQ(CaseSensitive(found->xlName_), UpperDotted(contract.first));
        ASSERT_EQ(CaseSensitive(found->argNames_), contract.second);
        ASSERT_EQ(DeclaredArgCount(*found), NamedArgCount(*found));
        ASSERT_FALSE(found->volatile_);
        ASSERT_FALSE(found->help_.empty());
        ASSERT_LE(found->maxArgHelpLength_, 255);
    }
}

TEST(ExcelRegistrationTest, TestQuoteRiskFunctionsRetainLongNamesAndHelpMetadata) {
    const auto registrations = RegisteredFunctionsForTest();
    const Vector_<String_> cNames = {
        "xl_SingleCurveQuoteRiskProvenance_New",
        "xl_JointXccyQuoteRiskProvenance_New",
        "xl_JointMultiCurveQuoteRiskProvenance_New",
        "xl_JointCurveDeclaration_New",
        "xl_JointMultiCurveCalibrationSpec_New",
        "xl_Calibrate_JointMultiCurve",
        "xl_JointMultiCurveCalibrationResult_Get_Curve",
        "xl_JointMultiCurveCalibrationResult_Get",
        "xl_StagedXccyBasisQuoteRiskProvenance_New",
        "xl_RateQuoteRiskProvenance_New",
        "xl_RatePortfolioQuoteRisk_Spill",
        "xl_CalibrationPullback_New",
        "xl_CalibrationParameterAdjoints_New",
        "xl_CalibrationDirectQuoteAdjoints_New",
        "xl_CalibrationQuoteRisk_New",
        "xl_CalibrationPullback_Get_Source",
        "xl_CalibrationPullback_Get_Provenance",
        "xl_CalibrationParameterAdjoints_Get_Calibration",
        "xl_CalibrationParameterAdjoints_Get_Adjoints",
        "xl_CalibrationDirectQuoteAdjoints_Get_Calibration",
        "xl_CalibrationDirectQuoteAdjoints_Get_Adjoints",
        "xl_CalibrationQuoteRisk_Get_Calibration",
        "xl_CalibrationQuoteRisk_Get_Adjoints",
        "xl_CalibrationQuoteRisk_Get_Provenance",
    };
    const String_ prefix("result,calibrationId,parameterBlockKeys,componentKeys,market");
    for (const auto& cName : cNames) {
        const auto found =
            std::find_if(registrations.begin(), registrations.end(), [&](const auto& registration) { return registration.cName_ == cName; });
        ASSERT_NE(found, registrations.end()) << cName.c_str();
        ASSERT_EQ(CaseSensitive(found->xlName_), UpperDotted(cName.substr(3)));
        ASSERT_FALSE(found->help_.empty());
        ASSERT_EQ(DeclaredArgCount(*found), found->argHelpCount_);
        ASSERT_LE(found->maxArgHelpLength_, 255);
        if (cName.find("QuoteRiskProvenance_New") != String_::npos) {
            ASSERT_EQ(found->argNames_.substr(0, prefix.size()), prefix);
            ASSERT_EQ(found->argNames_, prefix + ",[retainCalibrationRecord]");
            ASSERT_EQ(CaseSensitive(found->argTypes_), "QQQQQQQ");
        }
    }
    const auto staged = std::find_if(registrations.begin(), registrations.end(),
                                     [](const auto& registration) { return registration.cName_ == "xl_StagedXccyBasisQuoteRiskProvenance_New"; });
    ASSERT_NE(staged, registrations.end());
    ASSERT_EQ(staged->argNames_.substr(0, prefix.size()), prefix);
    ASSERT_EQ(staged->argNames_, prefix + ",[retainCalibrationRecord]");
}

TEST(ExcelRegistrationTest, TestCalibrationAndAutomaticRequestContracts) {
    struct Contract_ {
        const char* name_;
        const char* arguments_;
        const char* types_;
    };
    const Contract_ contracts[] = {{"CalibrationRiskRequest_New", "name,[settings]", "QQQ"},
                                   {"CalibrationRiskRequest_Get_Settings", "request", "QQ"},
                                   {"CalibrationRiskPlan_New", "name,calibration,[request]", "QQQQ"},
                                   {"CalibrationRiskPlan_Get_Calibration", "plan", "QQ"},
                                   {"CalibrationRiskPlan_Get_Inputs", "plan,[complete]", "QQQ"},
                                   {"CalibrationRiskPlan_Get_Shape", "plan", "QQ"},
                                   {"CalibrationRiskResult_New", "name,plan,parameters,[direct]", "QQQQQ"},
                                   {"CalibrationRiskResult_Get_Plan", "result", "QQ"},
                                   {"CalibrationRiskResult_Get_QuoteRisk", "result", "QQ"},
                                   {"CalibrationRiskResult_Get_Jacobian", "result,[projection]", "QQQ"},
                                   {"DupireScriptRiskSettings_New", "name,n_paths,[valuation],[simulation]", "QQQQQ"},
                                   {"DupireScriptRiskSettings_Get_Configuration", "settings,format", "QQQ"},
                                   {"DupireScriptRiskRequest_New", "name,settings,[quotes],[bindings],[direct]", "QQQQQQ"},
                                   {"DupireScriptRiskRequest_Get_Configuration", "request,format", "QQQ"},
                                   {"DupireScriptRiskRequest_Get_Direct", "request", "QQ"},
                                   {"DupireScriptRiskPlan_New", "name,product,modelData,calibration,component,request", "QQQQQQQ"},
                                   {"DupireScriptRiskPlan_Get_QuotePlan", "plan", "QQ"},
                                   {"DupireScriptRiskPlan_Get_Inputs", "plan,[complete]", "QQQ"},
                                   {"DupireScriptRiskPlan_Get_Configuration", "plan,format", "QQQ"},
                                   {"DupireScriptRiskPlan_Get_Provenance", "plan", "QQ"},
                                   {"DupireScriptRiskResult_New", "name,plan", "QQQ"},
                                   {"DupireScriptRiskResult_Get_Valuation", "result", "QQ"},
                                   {"DupireScriptRiskResult_Get_QuoteRisk", "result", "QQ"},
                                   {"DupireScriptRiskResult_Get_Provenance", "result", "QQ"}};
    const auto registrations = RegisteredFunctionsForTest();
    for (const auto& contract : contracts) {
        const auto name = String_("xl_") + contract.name_;
        const auto found = std::find_if(registrations.begin(), registrations.end(), [&](const auto& reg) { return reg.cName_ == name; });
        ASSERT_NE(found, registrations.end()) << contract.name_;
        ASSERT_EQ(CaseSensitive(found->xlName_), UpperDotted(contract.name_));
        ASSERT_EQ(CaseSensitive(found->argNames_), contract.arguments_);
        ASSERT_EQ(CaseSensitive(found->argTypes_), contract.types_);
        ASSERT_FALSE(found->volatile_);
        ASSERT_LE(found->maxArgHelpLength_, 255);
    }
}

TEST(ExcelRegistrationTest, TestEveryRegisteredFunctionResolvesToAnEntryPoint) {
    const HMODULE dll = ::GetModuleHandleA("dal_excel.xll");
    ASSERT_TRUE(dll != nullptr);
    for (const auto& reg : RegisteredFunctionsForTest())
        ASSERT_TRUE(::GetProcAddress(dll, reg.cName_.c_str()) != nullptr) << reg.cName_.c_str();
}

TEST(ExcelRegistrationTest, TestNoDuplicateExcelNames) {
    std::set<std::string> seen;
    for (const auto& reg : RegisteredFunctionsForTest()) {
        const std::string name(reg.xlName_.c_str());
        ASSERT_TRUE(seen.insert(name).second) << "duplicate Excel name: " << name;
    }
}

TEST(ExcelRegistrationTest, TestNoDuplicateCNames) {
    std::set<std::string> seen;
    for (const auto& reg : RegisteredFunctionsForTest()) {
        const std::string name(reg.cName_.c_str());
        ASSERT_TRUE(seen.insert(name).second) << "duplicate C entry point: " << name;
    }
}

TEST(ExcelRegistrationTest, TestArgumentMetadataIsConsistent) {
    for (const auto& reg : RegisteredFunctionsForTest()) {
        const int declared = DeclaredArgCount(reg);
        ASSERT_EQ(declared, NamedArgCount(reg)) << reg.cName_.c_str();
        ASSERT_EQ(declared, reg.argHelpCount_) << reg.cName_.c_str();
    }
}

TEST(ExcelRegistrationTest, TestCNamesFollowWrapperConvention) {
    for (const auto& reg : RegisteredFunctionsForTest()) {
        ASSERT_EQ(reg.cName_.substr(0, 3), String_("xl_")) << reg.cName_.c_str();
        ASSERT_FALSE(reg.xlName_.empty());
    }
}

TEST(ExcelRegistrationTest, TestExcelNameMatchesCNameConvention) {
    for (const auto& reg : RegisteredFunctionsForTest()) {
        const std::string expected = UpperDotted(reg.cName_.substr(3));
        const std::string actual(reg.xlName_.c_str());
        ASSERT_TRUE(actual == expected || actual == "DA." + expected) << reg.cName_.c_str() << " -> " << actual;
    }
}

#endif
