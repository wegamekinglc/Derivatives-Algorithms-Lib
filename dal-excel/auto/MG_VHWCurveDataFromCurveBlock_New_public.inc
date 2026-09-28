

extern "C" __declspec(dllexport) OPER_* xl_VHWCurveDataFromCurveBlock_New
    (const OPER_* xl_name, const OPER_* xl_block, const OPER_* xl_evaluationDate, const OPER_* xl_nodeDates, const OPER_* xl_projectionTenors)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("VHWCurveDataFromCurveBlock_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "block (input #2)";
        const Handle_<StorableCurveBlock_> block = Excel::ToHandle<StorableCurveBlock_>(_env, xl_block);
        argName = "evaluationDate (input #3)";
        const Date_ evaluationDate = Excel::ToDate(xl_evaluationDate);
        argName = "nodeDates (input #4)";
        const Vector_<Date_> nodeDates = Excel::ToDateVector(xl_nodeDates);
        argName = "projectionTenors (input #5)";
        const Vector_<String_> projectionTenors = Excel::ToStringVector(xl_projectionTenors);
        argName = 0;
		Handle_<VHWCurveData_> curve;
        VHWCurveDataFromCurveBlock_New(name, block, evaluationDate, nodeDates, projectionTenors, &curve);
        Excel::Retval_ retval;
        retval.Load(_env, curve);
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

struct XlRegister_VHWCurveDataFromCurveBlock_New_
{
    XlRegister_VHWCurveDataFromCurveBlock_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Curve snapshot name");
        argHelp.push_back("Source yield curve block");
        argHelp.push_back("Valuation date and first node");
        argHelp.push_back("Snapshot node dates");
        argHelp.push_back("Projection tenors to snapshot");
        Excel::Register("Base", "xl_VHWCurveDataFromCurveBlock_New", "VHWCURVEDATAFROMCURVEBLOCK.NEW", "Snapshot a curve block on specified dates for a VHW model", "QQQQQQ", "name,block,evaluationDate,nodeDates,projectionTenors", argHelp, false);
    }
};
static XlRegister_VHWCurveDataFromCurveBlock_New_ The_VHWCurveDataFromCurveBlock_New_XlRegisterer;
