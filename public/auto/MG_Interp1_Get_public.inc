

extern "C" __declspec(dllexport) OPER_* xl_Interp1_Get
    (const OPER_* xl_f, const OPER_* xl_x)	
{
    Excel::InitializeSessionIfNeeded();
	ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Interp1_Get");        
        argName = "f (input #1)";
        const Handle_<Interp1_> f = Excel::ToHandle<Interp1_>(_env, xl_f);
        argName = "x (input #2)";
        const Vector_<double> x = Excel::ToDoubleVector(xl_x);
        argName = 0;
		Vector_<double> y;
        Interp1_Get(f, x, &y);
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

struct XlRegister_Interp1_Get_
{
    XlRegister_Interp1_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The interpolant function");
        argHelp.push_back("The x-values (abcissas)");
        Excel::Register("Bones", "xl_Interp1_Get", "INTERP1.GET", "Interpolate a value at specified abcissas", "QQQ", "f,x", argHelp, false);
    }
};
static XlRegister_Interp1_Get_ The_Interp1_Get_XlRegisterer;
