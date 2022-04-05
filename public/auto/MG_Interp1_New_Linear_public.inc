

extern "C" __declspec(dllexport) OPER_* xl_Interp1_New_Linear
    (const OPER_* xl_name, const OPER_* xl_x, const OPER_* xl_y)	
{
    Excel::InitializeSessionIfNeeded();
	ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Interp1_New_Linear");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "x (input #2)";
        const Vector_<double> x = Excel::ToDoubleVector(xl_x);
      REQUIRE(IsMonotonic(x), "x values must be in ascending order");
        argName = "y (input #3)";
        const Vector_<double> y = Excel::ToDoubleVector(xl_y);
      REQUIRE(y.size() == x.size(), "x and y must have the same size");
        argName = 0;
		Handle_<Interp1_> f;
        Interp1_New_Linear(name, x, y, &f);
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

struct XlRegister_Interp1_New_Linear_
{
    XlRegister_Interp1_New_Linear_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the object being created");
        argHelp.push_back("The x-values (abscissas)");
        argHelp.push_back("The values of f(x) at each x");
        Excel::Register("Bones", "xl_Interp1_New_Linear", "INTERP1.NEW.LINEAR", "Create a linear interpolator", "QQQQ", "name,x,y", argHelp, false);
    }
};
static XlRegister_Interp1_New_Linear_ The_Interp1_New_Linear_XlRegisterer;
