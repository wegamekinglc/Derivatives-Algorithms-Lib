

extern "C" __declspec(dllexport) OPER_* xl_VHWCurveData_New
    (const OPER_* xl_name, const OPER_* xl_evaluationDate, const OPER_* xl_currency, const OPER_* xl_nodeDates, const OPER_* xl_discountLogDF, const OPER_* xl_projectionTenors, const OPER_* xl_projectionLogDF)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("VHWCurveData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "evaluationDate (input #2)";
        const Date_ evaluationDate = Excel::ToDate(xl_evaluationDate);
        argName = "currency (input #3)";
        const String_ currency = Excel::ToString(xl_currency);
        argName = "nodeDates (input #4)";
        const Vector_<Date_> nodeDates = Excel::ToDateVector(xl_nodeDates);
        argName = "discountLogDF (input #5)";
        const Vector_<double> discountLogDF = Excel::ToDoubleVector(xl_discountLogDF);
        argName = "projectionTenors (input #6)";
        const Vector_<String_> projectionTenors = Excel::ToStringVector(xl_projectionTenors);
        argName = "projectionLogDF (input #7)";
        const Matrix_<double> projectionLogDF = Excel::ToDoubleMatrix(xl_projectionLogDF);
        argName = 0;
		Handle_<VHWCurveData_> curve;
        VHWCurveData_New(name, evaluationDate, currency, nodeDates, discountLogDF, projectionTenors, projectionLogDF, &curve);
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

struct XlRegister_VHWCurveData_New_
{
    XlRegister_VHWCurveData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Curve snapshot name");
        argHelp.push_back("Valuation date and first node");
        argHelp.push_back("Model currency");
        argHelp.push_back("Strictly increasing node dates beginning at the valuation date");
        argHelp.push_back("OIS log discount factors beginning at zero");
        argHelp.push_back("Optional projection tenors");
        argHelp.push_back("One log discount factor row per projection tenor");
        Excel::Register("Base", "xl_VHWCurveData_New", "VHWCURVEDATA.NEW", "Dated initial discount and projection curve nodes for a VHW model", "QQQQQQQQ", "name,evaluationDate,currency,nodeDates,discountLogDF,projectionTenors,projectionLogDF", argHelp, false);
    }
};
static XlRegister_VHWCurveData_New_ The_VHWCurveData_New_XlRegisterer;
