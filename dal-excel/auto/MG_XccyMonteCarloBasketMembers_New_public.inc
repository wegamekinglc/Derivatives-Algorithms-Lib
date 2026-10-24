

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloBasketMembers_New
    (const OPER_* xl_members)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloBasketMembers_New");        
        argName = "members (input #1)";
        const Matrix_<Cell_> members = Excel::ToCellMatrix_orXccy(xl_members, Excel::XccyTableInput_("XccyMonteCarloBasketMembers_New", "members", 3), false, true);
        argName = 0;
		Handle_<StorableXccyMonteCarloBasketMembers_> value;
        XccyMonteCarloBasketMembers_New(members, &value);
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

struct XlRegister_XccyMonteCarloBasketMembers_New_
{
    XlRegister_XccyMonteCarloBasketMembers_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Complete member_id, instrument, measure_id columns");
        Excel::Register("Base", "xl_XccyMonteCarloBasketMembers_New", "XCCYMONTECARLOBASKETMEMBERS.NEW", "Own an ordered XCCY basket member table", "QQ", "members", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloBasketMembers_New_ The_XccyMonteCarloBasketMembers_New_XlRegisterer;
