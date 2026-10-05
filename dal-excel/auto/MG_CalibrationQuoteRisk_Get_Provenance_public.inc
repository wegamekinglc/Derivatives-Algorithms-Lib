

extern "C" __declspec(dllexport) OPER_* xl_CalibrationQuoteRisk_Get_Provenance
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationQuoteRisk_Get_Provenance");        
        argName = "result (input #1)";
        const Handle_<StorableCalibrationQuoteRisk_> result = Excel::ToHandle<StorableCalibrationQuoteRisk_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> provenance;
        CalibrationQuoteRisk_Get_Provenance(result, &provenance);
        Excel::Retval_ retval;
        retval.Load(provenance);
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

struct XlRegister_CalibrationQuoteRisk_Get_Provenance_
{
    XlRegister_CalibrationQuoteRisk_Get_Provenance_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed common quote risk");
        Excel::Register("Base", "xl_CalibrationQuoteRisk_Get_Provenance", "CALIBRATIONQUOTERISK.GET.PROVENANCE", "Project common quote result provenance", "QQ", "result", argHelp, false);
    }
};
static XlRegister_CalibrationQuoteRisk_Get_Provenance_ The_CalibrationQuoteRisk_Get_Provenance_XlRegisterer;
