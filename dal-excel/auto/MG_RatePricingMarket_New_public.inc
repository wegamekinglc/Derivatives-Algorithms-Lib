

extern "C" __declspec(dllexport) OPER_* xl_RatePricingMarket_New
    (const OPER_* xl_valuationTime, const OPER_* xl_resultCurrency, const OPER_* xl_componentKeys, const OPER_* xl_componentCurves, const OPER_* xl_fixings, const OPER_* xl_domesticBlock, const OPER_* xl_foreignBlock, const OPER_* xl_fxSpot, const OPER_* xl_collateralCurrency, const OPER_* xl_basisCurve)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RatePricingMarket_New");        
        argName = "valuationTime (input #1)";
        const Cell_ valuationTime = Excel::ToCell(xl_valuationTime);
        argName = "resultCurrency (input #2)";
        const String_ resultCurrency = Excel::ToString(xl_resultCurrency);
        argName = "componentKeys (input #3)";
        const Vector_<String_> componentKeys = Excel::ToStringVector(xl_componentKeys);
        argName = "componentCurves (input #4)";
        const Vector_<Handle_<Storable_>> componentCurves = Excel::ToHandleBaseVector(_env, xl_componentCurves);
        argName = "fixings (input #5)";
        const Handle_<StorableMarketFixingSnapshot_> fixings = Excel::ToHandle<StorableMarketFixingSnapshot_>(_env, xl_fixings, true);
        argName = "domesticBlock (input #6)";
        const Handle_<StorableCurveBlock_> domesticBlock = Excel::ToHandle<StorableCurveBlock_>(_env, xl_domesticBlock, true);
        argName = "foreignBlock (input #7)";
        const Handle_<StorableCurveBlock_> foreignBlock = Excel::ToHandle<StorableCurveBlock_>(_env, xl_foreignBlock, true);
        argName = "fxSpot (input #8)";
        const boost::optional<double> fxSpot = Excel::ToDouble(xl_fxSpot, true);
        argName = "collateralCurrency (input #9)";
        const String_ collateralCurrency = Excel::ToString(xl_collateralCurrency, true);
        argName = "basisCurve (input #10)";
        const Handle_<StorableDiscountCurve_> basisCurve = Excel::ToHandle<StorableDiscountCurve_>(_env, xl_basisCurve, true);
        argName = 0;
		Handle_<StorableRatePricingMarket_> market;
        RatePricingMarket_New(valuationTime, resultCurrency, componentKeys, componentCurves, fixings, domesticBlock, foreignBlock, fxSpot, collateralCurrency, basisCurve, &market);
        Excel::Retval_ retval;
        retval.Load(_env, market);
        return retval.ToXloper();
    }
    catch (std::exception& e)
    {
        return Excel::Error(e.what(), argName);
    }
    catch (...)
    {
        return Excel::Error("Unknown error", argName);
    }
}

struct XlRegister_RatePricingMarket_New_
{
    XlRegister_RatePricingMarket_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Valuation timestamp as an Excel date or date-time cell");
        argHelp.push_back("Reported (passthrough) currency code; never the aggregation grouping key");
        argHelp.push_back("Component keys, parallel to componentCurves");
        argHelp.push_back("Discount curve handles, parallel to componentKeys");
        argHelp.push_back("Immutable fixing snapshot");
        argHelp.push_back("XCCY domestic curve block (required for XCCY trades)");
        argHelp.push_back("XCCY foreign curve block (required for XCCY trades)");
        argHelp.push_back("Positive domestic-per-foreign FX spot");
        argHelp.push_back("XCCY collateral currency code");
        argHelp.push_back("XCCY basis curve");
        Excel::Register("Base", "xl_RatePricingMarket_New", "RATEPRICINGMARKET.NEW", "Assemble a rate pricing market from curve components and an optional XCCY market", "QQQQQQQQQQQ", "valuationTime,resultCurrency,componentKeys,componentCurves,[fixings],[domesticBlock],[foreignBlock],[fxSpot],[collateralCurrency],[basisCurve]", argHelp, false);
    }
};
static XlRegister_RatePricingMarket_New_ The_RatePricingMarket_New_XlRegisterer;
