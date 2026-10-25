

extern "C" __declspec(dllexport) OPER_* xl_CalibrationDiscountCurves_New
    (const OPER_* xl_bindings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationDiscountCurves_New");        
        argName = "bindings (input #1)";
        const Matrix_<Cell_> bindings = Excel::ToCellMatrix(xl_bindings, false, true);
        const Excel::ScriptSettingsInput_ normalized_bindings(xl_bindings); xl_bindings = normalized_bindings.Get();
        argName = 0;
		Handle_<StorableCalibrationDiscountCurves_> value;
        CalibrationDiscountCurves_New(bindings, &value);
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

struct XlRegister_CalibrationDiscountCurves_New_
{
    XlRegister_CalibrationDiscountCurves_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed bindings");
        Excel::Register("Base", "xl_CalibrationDiscountCurves_New", "CALIBRATIONDISCOUNTCURVES.NEW", "Owning typed script calibration interface", "QQ", "bindings", argHelp, false);
    }
};
static XlRegister_CalibrationDiscountCurves_New_ The_CalibrationDiscountCurves_New_XlRegisterer;
