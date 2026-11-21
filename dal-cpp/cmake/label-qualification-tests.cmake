# Keep full-scale numerical and capacity cases available to unfiltered CTest runs.
foreach(test IN ITEMS
        DepositScriptNumericalTest.TestFrozenThreeWayPriceRateAndDerivativeMatrix
        EarlyExercisePdeRiskQualificationTest.TestFrozenN1SmoothEuropeanAllRisksAgainstIndependentAnalytic
        EarlyExercisePdeRiskQualificationTest.TestFrozenN5bShortExpiryRequiresAvailableGamma
        EarlyExercisePdeRiskQualificationTest.TestFrozenN7FixedSpacingDomainRefinementRetainsAllRisks
        GSRMarketCalibrationTest.TestMultiExpiryTenorStrikeFitAndHeldOutVolatilities
        GSRRateOptionNumericalTest.TestFrozenThreeWayMatrix
        JointQuoteRiskTest.TestFullRecalibrationOracleAtRequiredWidthsModesLayoutsAndLayers
        LsmcCheckpointTest.TestHistoricalProgramCapacityIsBudgetedAndRejectionRecovers
        ScriptExerciseLSMCTest.TestAadRiskMatchesCentralDifferences
        ScriptTest.TestBlackScholes
        ScriptTest.TestBlackScholesAAD)
    list(FIND dal_cpp_tests_TESTS "${test}" test_index)
    if(NOT test_index EQUAL -1)
        set_tests_properties("${test}" PROPERTIES LABELS slow_qualification)
    endif()
endforeach()
