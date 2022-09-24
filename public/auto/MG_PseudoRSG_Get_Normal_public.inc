

extern "C" __declspec(dllexport) OPER_* xl_PseudoRSG_Get_Normal
    (const OPER_* xl_f, const OPER_* xl_num_path)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PseudoRSG_Get_Normal");        
        argName = "f (input #1)";
        const Handle_<PseudoRSG_> f = Excel::ToHandle<PseudoRSG_>(_env, xl_f);
        argName = "num_path (input #2)";
        const double num_path = Excel::ToDouble(xl_num_path);
        argName = 0;
		Matrix_<double> y;
        PseudoRSG_Get_Normal(f, num_path, &y);
        Excel::Retval_ retval;
        retval.Load(y);
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

struct XlRegister_PseudoRSG_Get_Normal_
{
    XlRegister_PseudoRSG_Get_Normal_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The pseudo random sequence generator");
        argHelp.push_back("The number of random sequences");
        Excel::Register("Base", "xl_PseudoRSG_Get_Normal", "PSEUDORSG.GET.NORMAL", "get a pseudo random normal number generated matrix (num_path x n_dim)", "QQQ", "f,num_path", argHelp, false);
    }
};
static XlRegister_PseudoRSG_Get_Normal_ The_PseudoRSG_Get_Normal_XlRegisterer;
