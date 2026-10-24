

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloBasketPartial_Get_Json
    (const OPER_* xl_partial)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloBasketPartial_Get_Json");        
        argName = "partial (input #1)";
        const Handle_<StorableXccyMonteCarloBasketPartial_> partial = Excel::ToHandle<StorableXccyMonteCarloBasketPartial_>(_env, xl_partial);
        argName = 0;
		Vector_<String_> json;
        XccyMonteCarloBasketPartial_Get_Json(partial, &json);
        Excel::Retval_ retval;
        retval.Load(json);
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

struct XlRegister_XccyMonteCarloBasketPartial_Get_Json_
{
    XlRegister_XccyMonteCarloBasketPartial_Get_Json_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owned partial independent of outcome");
        Excel::Register("Base", "xl_XccyMonteCarloBasketPartial_Get_Json", "XCCYMONTECARLOBASKETPARTIAL.GET.JSON", "Export owned failed progress", "QQ", "partial", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloBasketPartial_Get_Json_ The_XccyMonteCarloBasketPartial_Get_Json_XlRegisterer;
