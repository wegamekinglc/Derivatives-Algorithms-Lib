

extern "C" __declspec(dllexport) OPER_* xl_MultiFactorGSRModelData_New
    (const OPER_* xl_name, const OPER_* xl_curve, const OPER_* xl_vol)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MultiFactorGSRModelData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "curve (input #2)";
        const Handle_<GSRCurveData_> curve = Excel::ToHandle<GSRCurveData_>(_env, xl_curve);
        argName = "vol (input #3)";
        const Handle_<MultiFactorGSRVolData_> vol = Excel::ToHandle<MultiFactorGSRVolData_>(_env, xl_vol);
        argName = 0;
		Handle_<ModelData_> model;
        MultiFactorGSRModelData_New(name, curve, vol, &model);
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

struct XlRegister_MultiFactorGSRModelData_New_
{
    XlRegister_MultiFactorGSRModelData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Model name");
        argHelp.push_back("Initial curve snapshot");
        argHelp.push_back("Named g and H factor inputs");
        Excel::Register("Base", "xl_MultiFactorGSRModelData_New", "MULTIFACTORGSRMODELDATA.NEW", "Compose a multi-factor Gaussian short rate model", "QQQQ", "name,curve,vol", argHelp, false);
    }
};
static XlRegister_MultiFactorGSRModelData_New_ The_MultiFactorGSRModelData_New_XlRegisterer;
