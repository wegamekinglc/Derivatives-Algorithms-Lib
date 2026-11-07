

extern "C" __declspec(dllexport) OPER_* xl_CalibrationStrings_New
    (const OPER_* xl_rows)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationStrings_New");        
        argName = "rows (input #1)";
        const Matrix_<Cell_> rows = Excel::ToCellMatrix_orXccy(xl_rows, Excel::XccyTableInput_("CalibrationStrings_New", "rows", 1), false, true);
        argName = 0;
		Handle_<StorableCalibrationStrings_> value;
        CalibrationStrings_New(rows, &value);
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

struct XlRegister_CalibrationStrings_New_
{
    XlRegister_CalibrationStrings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Required named columns or exact field/value rows");
        Excel::Register("Base", "xl_CalibrationStrings_New", "CALIBRATIONSTRINGS.NEW", "Create immutable typed compounded OIS input", "QQ", "rows", argHelp, false);
    }
};
static XlRegister_CalibrationStrings_New_ The_CalibrationStrings_New_XlRegisterer;
