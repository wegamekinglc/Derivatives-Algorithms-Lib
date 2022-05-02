

extern "C" __declspec(dllexport) OPER_* xl_Interp2_New_Linear
    (const OPER_* xl_name, const OPER_* xl_x, const OPER_* xl_y, const OPER_* xl_z)	
{
    Excel::InitializeSessionIfNeeded();
	ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Interp2_New_Linear");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "x (input #2)";
        const Vector_<double> x = Excel::ToDoubleVector(xl_x);
      REQUIRE(IsMonotonic(x), "x values must be in ascending order");
        argName = "y (input #3)";
        const Vector_<double> y = Excel::ToDoubleVector(xl_y);
      REQUIRE(IsMonotonic(y), "y values must be in ascending order");
        argName = "z (input #4)";
        const Matrix_<double> z = Excel::ToDoubleMatrix(xl_z);
      REQUIRE(z.Rows() == x.size(), "z rows and x must have the same size");
      REQUIRE(z.Cols() == y.size(), "x columns and y must have the same size");
        argName = 0;
		Handle_<Interp2_> f;
        Interp2_New_Linear(name, x, y, z, &f);
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

struct XlRegister_Interp2_New_Linear_
{
    XlRegister_Interp2_New_Linear_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the object being created");
        argHelp.push_back("The x-values (abscissas)");
        argHelp.push_back("The y-values (abscissas)");
        argHelp.push_back("The values of f(x, y) at each x, y");
        Excel::Register("Bones", "xl_Interp2_New_Linear", "INTERP2.NEW.LINEAR", "Create a linear 2D interpolator", "QQQQQ", "name,x,y,z", argHelp, false);
    }
};
static XlRegister_Interp2_New_Linear_ The_Interp2_New_Linear_XlRegisterer;
