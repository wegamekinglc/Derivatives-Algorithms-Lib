

extern "C" __declspec(dllexport) OPER_* xl_CalibrationQuoteRisk_Get_Adjoints
    (const OPER_* xl_result, const OPER_* xl_contribution)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationQuoteRisk_Get_Adjoints");        
        argName = "result (input #1)";
        const Handle_<StorableCalibrationQuoteRisk_> result = Excel::ToHandle<StorableCalibrationQuoteRisk_>(_env, xl_result);
        argName = "contribution"; Excel::ValidateCalibrationTextInput(xl_contribution, "CalibrationQuoteRisk_Get_Adjoints; contribution");
        argName = "contribution (input #2)";
        const String_ contribution = Excel::ToString(xl_contribution, true);
        argName = 0;
		Matrix_<double> adjoints;
        CalibrationQuoteRisk_Get_Adjoints(result, contribution, &adjoints);
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

struct XlRegister_CalibrationQuoteRisk_Get_Adjoints_
{
    XlRegister_CalibrationQuoteRisk_Get_Adjoints_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed common quote risk");
        argHelp.push_back("Blank or total, calibration, direct");
        Excel::Register("Base", "xl_CalibrationQuoteRisk_Get_Adjoints", "CALIBRATIONQUOTERISK.GET.ADJOINTS", "Copy one common quote contribution without another pullback", "QQQ", "result,[contribution]", argHelp, false);
    }
};
static XlRegister_CalibrationQuoteRisk_Get_Adjoints_ The_CalibrationQuoteRisk_Get_Adjoints_XlRegisterer;
