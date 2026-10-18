

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeResult_Get_Price
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeResult_Get_Price");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeResult_> result = Excel::ToHandle<StorableEarlyExercisePdeResult_>(_env, xl_result);
        argName = 0;
		double price;
        EarlyExercisePdeResult_Get_Price(result, &price);
        Excel::Retval_ retval;
        retval.Load(price);
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

struct XlRegister_EarlyExercisePdeResult_Get_Price_
{
    XlRegister_EarlyExercisePdeResult_Get_Price_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Result handle");
        Excel::Register("Base", "xl_EarlyExercisePdeResult_Get_Price", "EARLYEXERCISEPDERESULT.GET.PRICE", "Read price per unit underlying; fails if status is not SUCCESS", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeResult_Get_Price_ The_EarlyExercisePdeResult_Get_Price_XlRegisterer;
