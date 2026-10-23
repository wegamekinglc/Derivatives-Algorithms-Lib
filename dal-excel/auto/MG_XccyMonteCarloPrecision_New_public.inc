

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloPrecision_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloPrecision_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix_orXccy(xl_settings, Excel::XccyTableInput_("XccyMonteCarloPrecision_New", "settings", 2), false, true);
        argName = 0;
		Handle_<StorableXccyMonteCarloPrecision_> value;
        XccyMonteCarloPrecision_New(settings, &value);
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

struct XlRegister_XccyMonteCarloPrecision_New_
{
    XlRegister_XccyMonteCarloPrecision_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed settings");
        Excel::Register("Base", "xl_XccyMonteCarloPrecision_New", "XCCYMONTECARLOPRECISION.NEW", "XccyMonteCarloPrecision New", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloPrecision_New_ The_XccyMonteCarloPrecision_New_XlRegisterer;
