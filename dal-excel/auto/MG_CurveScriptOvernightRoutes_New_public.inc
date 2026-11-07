

extern "C" __declspec(dllexport) OPER_* xl_CurveScriptOvernightRoutes_New
    (const OPER_* xl_routes)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveScriptOvernightRoutes_New");        
        argName = "routes (input #1)";
        const Matrix_<Cell_> routes = Excel::ToCellMatrix_orXccy(xl_routes, Excel::XccyTableInput_("CurveScriptOvernightRoutes_New", "routes", 6), false, true);
        argName = 0;
		Handle_<StorableCurveScriptRoutes_> value;
        CurveScriptOvernightRoutes_New(routes, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_CurveScriptOvernightRoutes_New_
{
    XlRegister_CurveScriptOvernightRoutes_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Required named columns or exact field/value rows");
        Excel::Register("Base", "xl_CurveScriptOvernightRoutes_New", "CURVESCRIPTOVERNIGHTROUTES.NEW", "Create immutable typed compounded OIS input", "QQ", "routes", argHelp, false);
    }
};
static XlRegister_CurveScriptOvernightRoutes_New_ The_CurveScriptOvernightRoutes_New_XlRegisterer;
