

extern "C" __declspec(dllexport) OPER_* xl_GSRModelData_New
    (const OPER_* xl_name, const OPER_* xl_curve, const OPER_* xl_vol)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRModelData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "curve (input #2)";
        const Handle_<GSRCurveData_> curve = Excel::ToHandle<GSRCurveData_>(_env, xl_curve);
        argName = "vol (input #3)";
        const Handle_<GSRVolData_> vol = Excel::ToHandle<GSRVolData_>(_env, xl_vol);
        argName = 0;
		Handle_<ModelData_> model;
        GSRModelData_New(name, curve, vol, &model);
        Excel::Retval_ retval;
        retval.Load(_env, model);
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

struct XlRegister_GSRModelData_New_
{
    XlRegister_GSRModelData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Model name");
        argHelp.push_back("Initial curve snapshot");
        argHelp.push_back("Piecewise-constant g and H");
        Excel::Register("Base", "xl_GSRModelData_New", "GSRMODELDATA.NEW", "Compose a one-factor GSR rate model", "QQQQ", "name,curve,vol", argHelp, false);
    }
};
static XlRegister_GSRModelData_New_ The_GSRModelData_New_XlRegisterer;
