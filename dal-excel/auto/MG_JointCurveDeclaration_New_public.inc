

extern "C" __declspec(dllexport) OPER_* xl_JointCurveDeclaration_New
    (const OPER_* xl_name, const OPER_* xl_instruments, const OPER_* xl_knotDates, const OPER_* xl_calibrateDiscountCurve, const OPER_* xl_collateral, const OPER_* xl_tenor, const OPER_* xl_settings, const OPER_* xl_initialGuessPerNode)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JointCurveDeclaration_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "instruments (input #2)";
        const Vector_<Handle_<Storable_>> instruments = Excel::ToHandleBaseVector(_env, xl_instruments);
        argName = "knotDates (input #3)";
        const Vector_<Date_> knotDates = Excel::ToDateVector(xl_knotDates);
        argName = "calibrateDiscountCurve (input #4)";
        const bool calibrateDiscountCurve = Excel::ToBool(xl_calibrateDiscountCurve);
        argName = "collateral (input #5)";
        const String_ collateral = Excel::ToString(xl_collateral);
        argName = "tenor (input #6)";
        const String_ tenor = Excel::ToString(xl_tenor);
        argName = "settings (input #7)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
      REQUIRE(settings.Cols() == 2 || settings.Empty(), "must have two columns (key, value)");
        argName = "initialGuessPerNode (input #8)";
        const Vector_<double> initialGuessPerNode = Excel::ToDoubleVector(xl_initialGuessPerNode, true);
        argName = 0;
		Handle_<StorableJointCurveDeclaration_> declaration;
        JointCurveDeclaration_New(name, instruments, knotDates, calibrateDiscountCurve, collateral, tenor, settings, initialGuessPerNode, &declaration);
        Excel::Retval_ retval;
        retval.Load(_env, declaration);
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

struct XlRegister_JointCurveDeclaration_New_
{
    XlRegister_JointCurveDeclaration_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Display name; duplicate names are distinguished by declaration index");
        argHelp.push_back("Yield-curve calibration instrument handles");
        argHelp.push_back("Native curve knot dates");
        argHelp.push_back("True for a discount slot, false for a forward tenor slot");
        argHelp.push_back("Discount collateral slot, including the base slot for layered forwards");
        argHelp.push_back("Forward tenor such as 3M; empty for discount declarations");
        argHelp.push_back("Keys: parameterization, logDfScheme, smoothingWeight, baseLayeredOverDiscount");
        argHelp.push_back("Optional initial guess in native parameter order");
        Excel::Register("Base", "xl_JointCurveDeclaration_New", "JOINTCURVEDECLARATION.NEW", "Declare one curve in a generic same-currency joint calibration", "QQQQQQQQQ", "name,instruments,knotDates,calibrateDiscountCurve,collateral,tenor,[settings],[initialGuessPerNode]", argHelp, false);
    }
};
static XlRegister_JointCurveDeclaration_New_ The_JointCurveDeclaration_New_XlRegisterer;
