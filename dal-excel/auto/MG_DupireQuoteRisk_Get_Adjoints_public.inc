

extern "C" __declspec(dllexport) OPER_* xl_DupireQuoteRisk_Get_Adjoints
    (const OPER_* xl_result, const OPER_* xl_contribution)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireQuoteRisk_Get_Adjoints");        
        argName = "result (input #1)";
        const Handle_<StorableDupireQuoteRisk_> result = Excel::ToHandle<StorableDupireQuoteRisk_>(_env, xl_result);
        argName = "contribution (input #2)";
        const String_ contribution = Excel::ToString(xl_contribution, true);
        argName = "contribution"; Excel::ValidateDupireTextInput(xl_contribution, "DupireQuoteRisk_Get_Adjoints; contribution");
        argName = 0;
		Matrix_<double> adjoints;
        DupireQuoteRisk_Get_Adjoints(result, contribution, &adjoints);
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

struct XlRegister_DupireQuoteRisk_Get_Adjoints_
{
    XlRegister_DupireQuoteRisk_Get_Adjoints_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed passive quote risk");
        argHelp.push_back("Blank selects total; otherwise exactly total, calibration or direct");
        Excel::Register("Base", "xl_DupireQuoteRisk_Get_Adjoints", "DUPIREQUOTERISK.GET.ADJOINTS", "Copy separated raw quote derivatives", "QQQ", "result,[contribution]", argHelp, false);
    }
};
static XlRegister_DupireQuoteRisk_Get_Adjoints_ The_DupireQuoteRisk_Get_Adjoints_XlRegisterer;
