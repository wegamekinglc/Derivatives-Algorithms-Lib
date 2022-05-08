

extern "C" __declspec(dllexport) OPER_* xl_Interp1_New_Linear_Smoothed
    (const OPER_* xl_name, const OPER_* xl_x, const OPER_* xl_y, const OPER_* xl_smoothing, const OPER_* xl_fit_weights)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Interp1_New_Linear_Smoothed");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "x (input #2)";
        const Vector_<double> x = Excel::ToDoubleVector(xl_x);
      REQUIRE(IsMonotonic(x), "x values must be in ascending order");
        argName = "y (input #3)";
        const Vector_<double> y = Excel::ToDoubleVector(xl_y);
      REQUIRE(y.size() == x.size(), "x and y must have the same size");
        argName = "smoothing (input #4)";
        const double smoothing = Excel::ToDouble(xl_smoothing);
      REQUIRE(smoothing >= 0.0, "Validation failed:  smoothing >= 0.0");
        argName = "fit_weights (input #5)";
        const Vector_<double> fit_weights = Excel::ToDoubleVector(xl_fit_weights, true);
      REQUIRE(fit_weights.empty() || fit_weights.size() == x.size(), "must have one fit_weights for each y");
        argName = 0;
		Handle_<Interp1_> f;
        Interp1_New_Linear_Smoothed(name, x, y, smoothing, fit_weights, &f);
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

struct XlRegister_Interp1_New_Linear_Smoothed_
{
    XlRegister_Interp1_New_Linear_Smoothed_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the object being created");
        argHelp.push_back("The x-values (abcissas)");
        argHelp.push_back("The values of f(x) at each x");
        argHelp.push_back("The weight to put on smoothness of the interpolating function");
        argHelp.push_back("The weight to attach to accuracy of fit for each y_i; default is 1.0 for all");
        Excel::Register("Base", "xl_Interp1_New_Linear_Smoothed", "INTERP1.NEW.LINEAR.SMOOTHED", "Create a linear interpolator", "QQQQQQ", "name,x,y,smoothing,[fit_weights]", argHelp, false);
    }
};
static XlRegister_Interp1_New_Linear_Smoothed_ The_Interp1_New_Linear_Smoothed_XlRegisterer;
