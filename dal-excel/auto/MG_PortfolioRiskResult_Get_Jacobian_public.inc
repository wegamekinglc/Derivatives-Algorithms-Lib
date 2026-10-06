

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_Jacobian
    (const OPER_* xl_result, const OPER_* xl_reported)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_Jacobian");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        xl_reported = Excel::ScriptScalarInput(xl_reported); argName = "reported"; Excel::ValidateRiskRequestBoolean(xl_reported, "PortfolioRiskResult_Get_Jacobian; reported");
        argName = "reported (input #2)";
        const bool reported = Excel::ToBool(xl_reported, true).value_or(false);
        argName = 0;
		Matrix_<Cell_> jacobian;
        PortfolioRiskResult_Get_Jacobian(result, reported, &jacobian);
        Excel::Retval_ retval;
        retval.Load(jacobian);
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

struct XlRegister_PortfolioRiskResult_Get_Jacobian_
{
    XlRegister_PortfolioRiskResult_Get_Jacobian_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted or attribution portfolio result");
        argHelp.push_back("Apply each selected input report factor once");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_Jacobian", "PORTFOLIORISKRESULT.GET.JACOBIAN", "Extract a detached global gradient or Jacobian matrix", "QQQ", "result,[reported]", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_Jacobian_ The_PortfolioRiskResult_Get_Jacobian_XlRegisterer;
