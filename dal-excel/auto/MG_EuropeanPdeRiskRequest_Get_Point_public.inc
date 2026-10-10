

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskRequest_Get_Point
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskRequest_Get_Point");        
        argName = "request (input #1)";
        const Handle_<StorableEuropeanPdeRiskRequest_> request = Excel::ToHandle<StorableEuropeanPdeRiskRequest_>(_env, xl_request);
        argName = 0;
		Matrix_<Cell_> point;
        EuropeanPdeRiskRequest_Get_Point(request, &point);
        Excel::Retval_ retval;
        retval.Load(point);
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

struct XlRegister_EuropeanPdeRiskRequest_Get_Point_
{
    XlRegister_EuropeanPdeRiskRequest_Get_Point_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Passive request");
        Excel::Register("Base", "xl_EuropeanPdeRiskRequest_Get_Point", "EUROPEANPDERISKREQUEST.GET.POINT", "Copy the European PDE rate, volatility and strike", "QQ", "request", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskRequest_Get_Point_ The_EuropeanPdeRiskRequest_Get_Point_XlRegisterer;
