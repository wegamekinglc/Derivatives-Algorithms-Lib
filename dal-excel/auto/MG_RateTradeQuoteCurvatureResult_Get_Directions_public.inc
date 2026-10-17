

extern "C" __declspec(dllexport) OPER_* xl_RateTradeQuoteCurvatureResult_Get_Directions
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeQuoteCurvatureResult_Get_Directions");        
        argName = "result (input #1)";
        const Handle_<StorableRateTradeQuoteCurvatureResult_> result = Excel::ToHandle<StorableRateTradeQuoteCurvatureResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> directions;
        RateTradeQuoteCurvatureResult_Get_Directions(result, &directions);
        Excel::Retval_ retval;
        retval.Load(directions);
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

struct XlRegister_RateTradeQuoteCurvatureResult_Get_Directions_
{
    XlRegister_RateTradeQuoteCurvatureResult_Get_Directions_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning native rate result");
        Excel::Register("Base", "xl_RateTradeQuoteCurvatureResult_Get_Directions", "RATETRADEQUOTECURVATURERESULT.GET.DIRECTIONS", "Copy signed full raw quote rows; empty products spill one blank cell", "QQ", "result", argHelp, false);
    }
};
static XlRegister_RateTradeQuoteCurvatureResult_Get_Directions_ The_RateTradeQuoteCurvatureResult_Get_Directions_XlRegisterer;
