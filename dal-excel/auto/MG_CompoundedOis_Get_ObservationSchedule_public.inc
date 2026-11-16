

extern "C" __declspec(dllexport) OPER_* xl_CompoundedOis_Get_ObservationSchedule
    (const OPER_* xl_prepared)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CompoundedOis_Get_ObservationSchedule");        
        argName = "prepared (input #1)";
        const Handle_<StorablePreparedCompoundedOis_> prepared = Excel::ToHandle<StorablePreparedCompoundedOis_>(_env, xl_prepared);
        argName = 0;
		Matrix_<Cell_> value;
        CompoundedOis_Get_ObservationSchedule(prepared, &value);
        Excel::Retval_ retval;
        retval.Load(value);
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

struct XlRegister_CompoundedOis_Get_ObservationSchedule_
{
    XlRegister_CompoundedOis_Get_ObservationSchedule_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning prepared OIS tables");
        Excel::Register("Base", "xl_CompoundedOis_Get_ObservationSchedule", "COMPOUNDEDOIS.GET.OBSERVATIONSCHEDULE", "Inspect the frozen overnight observation schedule", "QQ", "prepared", argHelp, false);
    }
};
static XlRegister_CompoundedOis_Get_ObservationSchedule_ The_CompoundedOis_Get_ObservationSchedule_XlRegisterer;
