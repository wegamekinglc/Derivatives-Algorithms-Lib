

extern "C" __declspec(dllexport) OPER_* xl_CurveScriptRoutes_New
    (const OPER_* xl_routes)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveScriptRoutes_New");        
        argName = "routes (input #1)";
        const Matrix_<Cell_> routes = Excel::ToCellMatrix(xl_routes, false, true);
        const Excel::ScriptSettingsInput_ normalized_routes(xl_routes); xl_routes = normalized_routes.Get();
        argName = 0;
		Handle_<StorableCurveScriptRoutes_> value;
        CurveScriptRoutes_New(routes, &value);
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

struct XlRegister_CurveScriptRoutes_New_
{
    XlRegister_CurveScriptRoutes_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed routes");
        Excel::Register("Base", "xl_CurveScriptRoutes_New", "CURVESCRIPTROUTES.NEW", "Owning typed script calibration interface", "QQ", "routes", argHelp, false);
    }
};
static XlRegister_CurveScriptRoutes_New_ The_CurveScriptRoutes_New_XlRegisterer;
