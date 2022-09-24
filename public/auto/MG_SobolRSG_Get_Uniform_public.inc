

extern "C" __declspec(dllexport) OPER_* xl_SobolRSG_Get_Uniform
    (const OPER_* xl_f, const OPER_* xl_num_path)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("SobolRSG_Get_Uniform");        
        argName = "f (input #1)";
        const Handle_<SobolRSG_> f = Excel::ToHandle<SobolRSG_>(_env, xl_f);
        argName = "num_path (input #2)";
        const double num_path = Excel::ToDouble(xl_num_path);
        argName = 0;
		Matrix_<double> y;
        SobolRSG_Get_Uniform(f, num_path, &y);
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

struct XlRegister_SobolRSG_Get_Uniform_
{
    XlRegister_SobolRSG_Get_Uniform_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The pseudo random sequence generator");
        argHelp.push_back("The number of random sequences");
        Excel::Register("Base", "xl_SobolRSG_Get_Uniform", "SOBOLRSG.GET.UNIFORM", "get a psobol quasi-random uniform number generated matrix (num_path x n_dim)", "QQQ", "f,num_path", argHelp, false);
    }
};
static XlRegister_SobolRSG_Get_Uniform_ The_SobolRSG_Get_Uniform_XlRegisterer;
