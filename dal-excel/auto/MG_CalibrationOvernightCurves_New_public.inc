

extern "C" __declspec(dllexport) OPER_* xl_CalibrationOvernightCurves_New
    (const OPER_* xl_bindings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationOvernightCurves_New");        
        argName = "bindings (input #1)";
        const Matrix_<Cell_> bindings = Excel::ToCellMatrix_orXccy(xl_bindings, Excel::XccyTableInput_("CalibrationOvernightCurves_New", "bindings", 2), false, true);
        argName = 0;
		Handle_<StorableCalibrationOvernightCurves_> value;
        CalibrationOvernightCurves_New(bindings, &value);
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

struct XlRegister_CalibrationOvernightCurves_New_
{
    XlRegister_CalibrationOvernightCurves_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Required named columns or exact field/value rows");
        Excel::Register("Base", "xl_CalibrationOvernightCurves_New", "CALIBRATIONOVERNIGHTCURVES.NEW", "Create immutable typed compounded OIS input", "QQ", "bindings", argHelp, false);
    }
};
static XlRegister_CalibrationOvernightCurves_New_ The_CalibrationOvernightCurves_New_XlRegisterer;
