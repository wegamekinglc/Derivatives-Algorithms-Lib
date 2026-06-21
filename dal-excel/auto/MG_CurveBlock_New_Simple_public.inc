

extern "C" __declspec(dllexport) OPER_* xl_CurveBlock_New_Simple
    (const OPER_* xl_dc, const OPER_* xl_liborBasis)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveBlock_New_Simple");        
        argName = "dc (input #1)";
        const Handle_<StorableDiscountCurve_> dc = Excel::ToHandle<StorableDiscountCurve_>(_env, xl_dc);
        argName = "liborBasis (input #2)";
        const String_ liborBasis = Excel::ToString(xl_liborBasis, true);
        argName = 0;
		Handle_<StorableCurveBlock_> block;
        CurveBlock_New_Simple(dc, liborBasis, &block);
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

struct XlRegister_CurveBlock_New_Simple_
{
    XlRegister_CurveBlock_New_Simple_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The discount curve");
        argHelp.push_back("Libor day basis name (default \"ACT_365F\")");
        Excel::Register("Base", "xl_CurveBlock_New_Simple", "DA.CURVEBLOCKNEWSIMPLE", "Build a CurveBlock from a single discount curve", "QQQ", "dc,[liborBasis]", argHelp, false);
    }
};
static XlRegister_CurveBlock_New_Simple_ The_CurveBlock_New_Simple_XlRegisterer;
