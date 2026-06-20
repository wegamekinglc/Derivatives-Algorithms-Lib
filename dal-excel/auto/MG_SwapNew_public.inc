

extern "C" __declspec(dllexport) OPER_* xl_SwapNew
    (const OPER_* xl_tradeDate, const OPER_* xl_start, const OPER_* xl_maturity, const OPER_* xl_marketRate, const OPER_* xl_fixedLeg, const OPER_* xl_floatIndex, const OPER_* xl_floatLeg)
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("SwapNew");
        argName = "tradeDate (input #1)";
        const Date_ tradeDate = Excel::ToDate(xl_tradeDate);
        argName = "start (input #2)";
        const Date_ start = Excel::ToDate(xl_start);
        argName = "maturity (input #3)";
        const Date_ maturity = Excel::ToDate(xl_maturity);
        argName = "marketRate (input #4)";
        const double marketRate = Excel::ToDouble(xl_marketRate);
        argName = "fixedLeg (input #5)";
        const Handle_<StorableRateLegConvention_> fixedLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_fixedLeg);
        argName = "floatIndex (input #6)";
        const Handle_<StorableRateIndexConvention_> floatIndex = Excel::ToHandle<StorableRateIndexConvention_>(_env, xl_floatIndex);
        argName = "floatLeg (input #7)";
        const Handle_<StorableRateLegConvention_> floatLeg = Excel::ToHandle<StorableRateLegConvention_>(_env, xl_floatLeg);
        argName = 0;
		Handle_<StorableYCInstrument_> instrument;
        SwapNew(tradeDate, start, maturity, marketRate, fixedLeg, floatIndex, floatLeg, &instrument);
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

struct XlRegister_SwapNew_
{
    XlRegister_SwapNew_()
    {
        Vector_<String_> argHelp;
        argHelp.push_back("The trade/settlement date");
        argHelp.push_back("The start date of the swap");
        argHelp.push_back("The maturity date of the swap");
        argHelp.push_back("The market quoted rate");
        argHelp.push_back("The fixed leg convention");
        argHelp.push_back("The floating index convention");
        argHelp.push_back("The floating leg convention");
        Excel::Register("Base", "xl_SwapNew", "DA.SWAP.NEW", "Create a swap instrument for curve calibration", "QQQQQQQQ!", "tradeDate,start,maturity,marketRate,fixedLeg,floatIndex,floatLeg", argHelp, false);
    }
};
static XlRegister_SwapNew_ The_SwapNew_XlRegisterer;
