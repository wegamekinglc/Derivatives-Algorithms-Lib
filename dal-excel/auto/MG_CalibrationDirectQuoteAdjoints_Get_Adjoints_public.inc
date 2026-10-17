

extern "C" __declspec(dllexport) OPER_* xl_CalibrationDirectQuoteAdjoints_Get_Adjoints
    (const OPER_* xl_direct)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationDirectQuoteAdjoints_Get_Adjoints");        
        argName = "direct (input #1)";
        const Handle_<StorableCalibrationDirectQuoteAdjoints_> direct = Excel::ToHandle<StorableCalibrationDirectQuoteAdjoints_>(_env, xl_direct);
        argName = 0;
		Matrix_<double> adjoints;
        CalibrationDirectQuoteAdjoints_Get_Adjoints(direct, &adjoints);
        Excel::Retval_ retval;
        retval.Load(adjoints);
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

struct XlRegister_CalibrationDirectQuoteAdjoints_Get_Adjoints_
{
    XlRegister_CalibrationDirectQuoteAdjoints_Get_Adjoints_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owned direct quote seeds");
        Excel::Register("Base", "xl_CalibrationDirectQuoteAdjoints_Get_Adjoints", "CALIBRATIONDIRECTQUOTEADJOINTS.GET.ADJOINTS", "Copy canonical direct quote seeds", "QQ", "direct", argHelp, false);
    }
};
static XlRegister_CalibrationDirectQuoteAdjoints_Get_Adjoints_ The_CalibrationDirectQuoteAdjoints_Get_Adjoints_XlRegisterer;
