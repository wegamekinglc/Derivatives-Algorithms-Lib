

extern "C" __declspec(dllexport) OPER_* xl_SobolRSG_New
    (const OPER_* xl_name, const OPER_* xl_i_path, const OPER_* xl_n_dim, const OPER_* xl_precise, const OPER_* xl_polish)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("SobolRSG_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "i_path (input #2)";
        const double i_path = Excel::ToDouble(xl_i_path);
        argName = "n_dim (input #3)";
        const double n_dim = Excel::ToDouble(xl_n_dim);
        argName = "precise (input #4)";
        const bool precise = Excel::ToBool(xl_precise, true).value_or(false);
        argName = "polish (input #5)";
        const bool polish = Excel::ToBool(xl_polish, true).value_or(false);
        argName = 0;
		Handle_<SobolRSG_> f;
        SobolRSG_New(name, i_path, n_dim, precise, polish, &f);
        Excel::Retval_ retval;
        retval.Load(_env, f);
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

struct XlRegister_SobolRSG_New_
{
    XlRegister_SobolRSG_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the object being created");
        argHelp.push_back("The number of path to skip");
        argHelp.push_back("The dimension of the generator");
        argHelp.push_back("Use the precise CDF when polishing normal draws");
        argHelp.push_back("Apply a Newton correction to normal draws");
        Excel::Register("Base", "xl_SobolRSG_New", "SOBOLRSG.NEW", "Create a sobol quasi-random sequence generator", "QQQQQQ", "name,i_path,n_dim,[precise],[polish]", argHelp, false);
    }
};
static XlRegister_SobolRSG_New_ The_SobolRSG_New_XlRegisterer;
