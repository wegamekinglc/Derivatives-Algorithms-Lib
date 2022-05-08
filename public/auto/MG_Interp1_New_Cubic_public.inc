

extern "C" __declspec(dllexport) OPER_* xl_Interp1_New_Cubic
    (const OPER_* xl_name, const OPER_* xl_x, const OPER_* xl_y, const OPER_* xl_boundary_order, const OPER_* xl_boundary_value)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Interp1_New_Cubic");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "x (input #2)";
        const Vector_<double> x = Excel::ToDoubleVector(xl_x);
      REQUIRE(IsMonotonic(x), "x values must be in ascending order");
        argName = "y (input #3)";
        const Vector_<double> y = Excel::ToDoubleVector(xl_y);
      REQUIRE(y.size() == x.size(), "x and y must have the same size");
        argName = "boundary_order (input #4)";
        const Vector_<int> boundary_order = Excel::ToIntVector(xl_boundary_order, true);
      REQUIRE(boundary_order.size() <= 2, "Can only specify two boundary conditions");
      REQUIRE(boundary_order.empty() || (boundary_order.front() > 0 && boundary_order.back() > 0 && boundary_order.front() <= 3 && boundary_order.back() <= 3), "Boundary order must be in the range");
        argName = "boundary_value (input #5)";
        const Vector_<double> boundary_value = Excel::ToDoubleVector(xl_boundary_value, true);
      REQUIRE(boundary_value.size() <= 2, "Can only specify two boundary conditions");
      REQUIRE(boundary_value.empty() || !boundary_order.empty(), "Can't specify boundary value without specifying order");
        argName = 0;
		Handle_<Interp1_> f;
        Interp1_New_Cubic(name, x, y, boundary_order, boundary_value, &f);
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

struct XlRegister_Interp1_New_Cubic_
{
    XlRegister_Interp1_New_Cubic_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the object being created");
        argHelp.push_back("The x-values (abscissas)");
        argHelp.push_back("The values of f(x) at each x");
        argHelp.push_back("(0, 3] The order of the derivatives specified at the boundary (default is 3); can be a two-element vector of (left, right), or a single value for both");
        argHelp.push_back("The value of whichever derivative is specified at the boundary (default is 0.0); can be a two-element vector of (left, right), or a single number for bot");
        Excel::Register("Base", "xl_Interp1_New_Cubic", "INTERP1.NEW.CUBIC", "Create a cubic-spline interpolator", "QQQQQQ", "name,x,y,[boundary_order],[boundary_value]", argHelp, false);
    }
};
static XlRegister_Interp1_New_Cubic_ The_Interp1_New_Cubic_XlRegisterer;
