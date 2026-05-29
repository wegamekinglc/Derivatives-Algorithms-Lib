

extern "C" __declspec(dllexport) OPER_* xl_EvaluationDate_Get
    ()	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EvaluationDate_Get");        
        argName = 0;
		Cell_ date;
        EvaluationDate_Get(&date);
        Excel::Retval_ retval;
        retval.Load(date);
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

struct XlRegister_EvaluationDate_Get_
{
    XlRegister_EvaluationDate_Get_()
    {
        Vector_<String_> argHelp;        
        Excel::Register("Base", "xl_EvaluationDate_Get", "EVALUATIONDATE.GET", "get the global evaluation date", "Q", "", argHelp, false);
    }
};
static XlRegister_EvaluationDate_Get_ The_EvaluationDate_Get_XlRegisterer;
