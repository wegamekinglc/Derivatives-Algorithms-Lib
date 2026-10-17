

extern "C" __declspec(dllexport) OPER_* xl_BumpOverAADRequest_Get_Steps
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BumpOverAADRequest_Get_Steps");        
        argName = "request (input #1)";
        const Handle_<StorableBumpOverAADRequest_> request = Excel::ToHandle<StorableBumpOverAADRequest_>(_env, xl_request);
        argName = 0;
		Matrix_<Cell_> steps;
        BumpOverAADRequest_Get_Steps(request, &steps);
        Excel::Retval_ retval;
        retval.Load(steps);
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

struct XlRegister_BumpOverAADRequest_Get_Steps_
{
    XlRegister_BumpOverAADRequest_Get_Steps_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning passive request");
        Excel::Register("Base", "xl_BumpOverAADRequest_Get_Steps", "BUMPOVERAADREQUEST.GET.STEPS", "Copy positive finite-step sizes as a column", "QQ", "request", argHelp, false);
    }
};
static XlRegister_BumpOverAADRequest_Get_Steps_ The_BumpOverAADRequest_Get_Steps_XlRegisterer;
