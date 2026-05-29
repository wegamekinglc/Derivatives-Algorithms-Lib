

extern "C" __declspec(dllexport) OPER_* xl_PseudoRSG_New
    (const OPER_* xl_name, const OPER_* xl_seed, const OPER_* xl_n_dim)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PseudoRSG_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "seed (input #2)";
        const double seed = Excel::ToDouble(xl_seed);
        argName = "n_dim (input #3)";
        const double n_dim = Excel::ToDouble(xl_n_dim);
        argName = 0;
		Handle_<PseudoRSG_> f;
        PseudoRSG_New(name, seed, n_dim, &f);
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

struct XlRegister_PseudoRSG_New_
{
    XlRegister_PseudoRSG_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the object being created");
        argHelp.push_back("The seed for the generator");
        argHelp.push_back("The dimension of the generator");
        Excel::Register("Base", "xl_PseudoRSG_New", "PSEUDORSG.NEW", "Create a pseudo random sequence generator", "QQQQ", "name,seed,n_dim", argHelp, false);
    }
};
static XlRegister_PseudoRSG_New_ The_PseudoRSG_New_XlRegisterer;
