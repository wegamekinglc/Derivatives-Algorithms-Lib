

extern "C" __declspec(dllexport) OPER_* xl_Interp2_Get
    (const OPER_* xl_f, const OPER_* xl_x, const OPER_* xl_y)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Interp2_Get");        
        argName = "f (input #1)";
        const Handle_<Interp2_> f = Excel::ToHandle<Interp2_>(_env, xl_f);
        argName = "x (input #2)";
        const Vector_<double> x = Excel::ToDoubleVector(xl_x);
        argName = "y (input #3)";
        const Vector_<double> y = Excel::ToDoubleVector(xl_y);
        argName = 0;
		Matrix_<double> z;
        Interp2_Get(f, x, y, &z);
        Excel::Retval_ retval;
        retval.Load(z);
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

struct XlRegister_Interp2_Get_
{
    XlRegister_Interp2_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The interpolant function");
        argHelp.push_back("The x-values (abcissas)");
        argHelp.push_back("The y-values (abcissas)");
        Excel::Register("Base", "xl_Interp2_Get", "INTERP2.GET", "Interpolate 2D a value at specified abcissas", "QQQQ", "f,x,y", argHelp, false);
    }
};
static XlRegister_Interp2_Get_ The_Interp2_Get_XlRegisterer;
