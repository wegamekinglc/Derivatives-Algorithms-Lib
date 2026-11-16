

extern "C" __declspec(dllexport) OPER_* xl_CompoundedOisObservationConvention_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CompoundedOisObservationConvention_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix_orXccy(xl_settings, Excel::XccyTableInput_("CompoundedOisObservationConvention_New", "settings", 2), false, true);
        argName = 0;
		Handle_<StorableCompoundedOisObservationConvention_> value;
        CompoundedOisObservationConvention_New(settings, &value);
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

struct XlRegister_CompoundedOisObservationConvention_New_
{
    XlRegister_CompoundedOisObservationConvention_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact mode and optional business-day counts");
        Excel::Register("Base", "xl_CompoundedOisObservationConvention_New", "COMPOUNDEDOISOBSERVATIONCONVENTION.NEW", "Create an explicit overnight observation convention", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_CompoundedOisObservationConvention_New_ The_CompoundedOisObservationConvention_New_XlRegisterer;
