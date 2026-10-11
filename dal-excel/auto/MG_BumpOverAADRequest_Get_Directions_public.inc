

extern "C" __declspec(dllexport) OPER_* xl_BumpOverAADRequest_Get_Directions
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BumpOverAADRequest_Get_Directions");        
        argName = "request (input #1)";
        const Handle_<StorableBumpOverAADRequest_> request = Excel::ToHandle<StorableBumpOverAADRequest_>(_env, xl_request);
        argName = 0;
		Matrix_<Cell_> directions;
        BumpOverAADRequest_Get_Directions(request, &directions);
        Excel::Retval_ retval;
        retval.Load(directions);
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

struct XlRegister_BumpOverAADRequest_Get_Directions_
{
    XlRegister_BumpOverAADRequest_Get_Directions_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning passive request");
        Excel::Register("Base", "xl_BumpOverAADRequest_Get_Directions", "BUMPOVERAADREQUEST.GET.DIRECTIONS", "Copy finite-step direction rows in full raw input order", "QQ", "request", argHelp, false);
    }
};
static XlRegister_BumpOverAADRequest_Get_Directions_ The_BumpOverAADRequest_Get_Directions_XlRegisterer;
