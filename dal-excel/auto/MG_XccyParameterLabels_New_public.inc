

extern "C" __declspec(dllexport) OPER_* xl_XccyParameterLabels_New
    (const OPER_* xl_labels)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyParameterLabels_New");        
        argName = "labels (input #1)";
        const Vector_<String_> labels = Excel::ToStringVector(xl_labels);
        argName = "labels"; Excel::ValidateXccyLabels(xl_labels);
        argName = 0;
		Handle_<StorableXccyParameterLabels_> value;
        XccyParameterLabels_New(labels, &value);
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

struct XlRegister_XccyParameterLabels_New_
{
    XlRegister_XccyParameterLabels_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed labels");
        Excel::Register("Base", "xl_XccyParameterLabels_New", "XCCYPARAMETERLABELS.NEW", "XccyParameterLabels New", "QQ", "labels", argHelp, false);
    }
};
static XlRegister_XccyParameterLabels_New_ The_XccyParameterLabels_New_XlRegisterer;
