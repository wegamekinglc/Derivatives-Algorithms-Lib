

extern "C" __declspec(dllexport) OPER_* xl_CompoundedOisFixedLeg_New
    (const OPER_* xl_rows)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CompoundedOisFixedLeg_New");        
        argName = "rows (input #1)";
        const Matrix_<Cell_> rows = Excel::ToCellMatrix_orXccy(xl_rows, Excel::XccyTableInput_("CompoundedOisFixedLeg_New", "rows", 4), false, true);
        argName = 0;
		Handle_<StorableCompoundedOisFixedLeg_> value;
        CompoundedOisFixedLeg_New(rows, &value);
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

struct XlRegister_CompoundedOisFixedLeg_New_
{
    XlRegister_CompoundedOisFixedLeg_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Required named columns or exact field/value rows");
        Excel::Register("Base", "xl_CompoundedOisFixedLeg_New", "COMPOUNDEDOISFIXEDLEG.NEW", "Create immutable typed compounded OIS input", "QQ", "rows", argHelp, false);
    }
};
static XlRegister_CompoundedOisFixedLeg_New_ The_CompoundedOisFixedLeg_New_XlRegisterer;
