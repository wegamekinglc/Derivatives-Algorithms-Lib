

extern "C" __declspec(dllexport) OPER_* xl_CurveBlock_New
    (const OPER_* xl_name, const OPER_* xl_ccy, const OPER_* xl_discounts, const OPER_* xl_forwards, const OPER_* xl_liborBasis)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveBlock_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "ccy (input #2)";
        const String_ ccy = Excel::ToString(xl_ccy);
        argName = "discounts (input #3)";
        const Handle_<StorableCalibrationDiscountCurves_> discounts = Excel::ToHandle<StorableCalibrationDiscountCurves_>(_env, xl_discounts);
        argName = "forwards (input #4)";
        const Handle_<StorableCalibrationForwardCurves_> forwards = Excel::ToHandle<StorableCalibrationForwardCurves_>(_env, xl_forwards);
        argName = "liborBasis (input #5)";
        const String_ liborBasis = Excel::ToString(xl_liborBasis, true);
        argName = 0;
		Handle_<StorableCurveBlock_> block;
        CurveBlock_New(name, ccy, discounts, forwards, liborBasis, &block);
        Excel::Retval_ retval;
        retval.Load(_env, block);
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

struct XlRegister_CurveBlock_New_
{
    XlRegister_CurveBlock_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the curve block");
        argHelp.push_back("Currency code (e.g. \"USD\")");
        argHelp.push_back("Collateral-keyed discount curve bindings");
        argHelp.push_back("Tenor-keyed projection curve bindings");
        argHelp.push_back("Libor day basis name (default \"ACT_365F\")");
        Excel::Register("Base", "xl_CurveBlock_New", "CURVEBLOCK.NEW", "Build a curve block with explicit discount and projection curves", "QQQQQQ", "name,ccy,discounts,forwards,[liborBasis]", argHelp, false);
    }
};
static XlRegister_CurveBlock_New_ The_CurveBlock_New_XlRegisterer;
