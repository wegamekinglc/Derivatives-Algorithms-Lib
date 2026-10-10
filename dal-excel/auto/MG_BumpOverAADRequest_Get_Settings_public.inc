

extern "C" __declspec(dllexport) OPER_* xl_BumpOverAADRequest_Get_Settings
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BumpOverAADRequest_Get_Settings");        
        argName = "request (input #1)";
        const Handle_<StorableBumpOverAADRequest_> request = Excel::ToHandle<StorableBumpOverAADRequest_>(_env, xl_request);
        argName = 0;
		Matrix_<Cell_> settings;
        BumpOverAADRequest_Get_Settings(request, &settings);
        Excel::Retval_ retval;
        retval.Load(settings);
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

struct XlRegister_BumpOverAADRequest_Get_Settings_
{
    XlRegister_BumpOverAADRequest_Get_Settings_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning passive request");
        Excel::Register("Base", "xl_BumpOverAADRequest_Get_Settings", "BUMPOVERAADREQUEST.GET.SETTINGS", "Copy the logical input count and optional native budgets", "QQ", "request", argHelp, false);
    }
};
static XlRegister_BumpOverAADRequest_Get_Settings_ The_BumpOverAADRequest_Get_Settings_XlRegisterer;
