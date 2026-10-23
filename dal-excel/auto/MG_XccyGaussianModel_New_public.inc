

extern "C" __declspec(dllexport) OPER_* xl_XccyGaussianModel_New
    (const OPER_* xl_name, const OPER_* xl_spec)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyGaussianModel_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "spec (input #2)";
        const Handle_<StorableXccyGaussianModelSpec_> spec = Excel::ToHandle<StorableXccyGaussianModelSpec_>(_env, xl_spec);
        argName = 0;
		Handle_<XccyGaussianModelData_> value;
        XccyGaussianModel_New(name, spec, &value);
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

struct XlRegister_XccyGaussianModel_New_
{
    XlRegister_XccyGaussianModel_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed name");
        argHelp.push_back("Typed spec");
        Excel::Register("Base", "xl_XccyGaussianModel_New", "XCCYGAUSSIANMODEL.NEW", "XccyGaussianModel New", "QQQ", "name,spec", argHelp, false);
    }
};
static XlRegister_XccyGaussianModel_New_ The_XccyGaussianModel_New_XlRegisterer;
