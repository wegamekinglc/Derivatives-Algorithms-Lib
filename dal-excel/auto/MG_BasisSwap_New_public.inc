

extern "C" __declspec(dllexport) OPER_* xl_BasisSwap_New
    (const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_marketRate, const OPER_* xl_spreadIndex, const OPER_* xl_spreadLeg, const OPER_* xl_refIndex, const OPER_* xl_refLeg)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BasisSwap_New");        
        argName = "tradeDate (input #1)";
        const Date_ tradeDate = Excel::ToDate(xl_tradeDate);
        argName = "start (input #2)";
        const Date_ start = Excel::ToDate(xl_start);
        argName = "maturity (input #3)";
        const Date_ maturity = Excel::ToDate(xl_maturity);
        argName = "marketRate (input #4)";
        const double marketRate = Excel::ToDouble(xl_marketRate);
        argName = "spreadIndex (input #5)";
        const Handle_<StorableRateIndexConvention_> spreadIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_spreadIndex);
        argName = "spreadLeg (input #6)";
        const Handle_<StorableRateLegConvention_> spreadLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_spreadLeg);
        argName = "refIndex (input #7)";
        const Handle_<StorableRateIndexConvention_> refIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_refIndex);
        argName = "refLeg (input #8)";
        const Handle_<StorableRateLegConvention_> refLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_refLeg);
        argName = 0;
		Handle_<StorableYCInstrument_> instrument;
        BasisSwap_New(tradeDate, start, maturity, marketRate, spreadIndex, spreadLeg, refIndex, refLeg, &instrument);
        Excel::Retval_ retval;
        retval.Load(_env, instrument);
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

struct XlRegister_BasisSwap_New_
{
    XlRegister_BasisSwap_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The trade/settlement date");
        argHelp.push_back("The start date of the swap");
        argHelp.push_back("The maturity date of the swap");
        argHelp.push_back("The market quoted spread");
        argHelp.push_back("The spread index convention");
        argHelp.push_back("The spread leg convention");
        argHelp.push_back("The reference index convention");
        argHelp.push_back("The reference leg convention");
        Excel::Register("Base", "xl_BasisSwap_New", "DA.BASISSWAPNEW", "Create a basis swap instrument for curve calibration", "QQQQQQQQQ", "tradeDate,start,maturity,marketRate,spreadIndex,spreadLeg,refIndex,refLeg", argHelp, false);
    }
};
static XlRegister_BasisSwap_New_ The_BasisSwap_New_XlRegisterer;
