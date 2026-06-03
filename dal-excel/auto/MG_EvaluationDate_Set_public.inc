

extern "C" __declspec(dllexport) OPER_* xl_EvaluationDate_Set
    (const OPER_* xl_date)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EvaluationDate_Set");        
        argName = "date (input #1)";
        const Date_ date = Excel::ToDate(xl_date);
        argName = 0;
		Cell_ d;
        EvaluationDate_Set(date, &d);
        Excel::Retval_ retval;
        retval.Load(d);
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

struct XlRegister_EvaluationDate_Set_
{
    XlRegister_EvaluationDate_Set_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("the evalution date to set");
        Excel::Register("Base", "xl_EvaluationDate_Set", "EVALUATIONDATE.SET", "set the global evaluation date", "QQ", "date", argHelp, false);
    }
};
static XlRegister_EvaluationDate_Set_ The_EvaluationDate_Set_XlRegisterer;
