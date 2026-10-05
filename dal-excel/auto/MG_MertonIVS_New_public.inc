

extern "C" __declspec(dllexport) OPER_* xl_MertonIVS_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MertonIVS_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "MertonIVS_New; name"); const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get(); argName = "settings"; Excel::ValidateDupireSettingsInput(xl_settings);
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, false, true);
        argName = 0;
		Handle_<StorableMertonIVS_> base;
        MertonIVS_New(name, settings, &base);
        Excel::Retval_ retval;
        retval.Load(_env, base);
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

struct XlRegister_MertonIVS_New_
{
    XlRegister_MertonIVS_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Required numeric keys: spot, vol, intensity, average_jump, jump_std");
        Excel::Register("Base", "xl_MertonIVS_New", "MERTONIVS.NEW", "Create a checked Merton implied-volatility base", "QQQ", "name,settings", argHelp, false);
    }
};
static XlRegister_MertonIVS_New_ The_MertonIVS_New_XlRegisterer;
