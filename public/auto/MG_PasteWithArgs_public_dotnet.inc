

#pragma managed
#include "MG_PasteWithArgs_public_dotnet.hpp"

TALibNet::PasteWithArgs_Return_^ TALibNet::PasteWithArgs_::Run
    (System::String^ dn_func_name)
{
    PasteWithArgs_Return_^ retval = gcnew PasteWithArgs_Return_;
    const char* argName = 0;
    try
    {        
        argName = "func_name";
        const String_ func_name = DotNet::ToString(dn_func_name);
        argName = 0;
		String_ func_with_args;
        PasteWithArgs(func_name, &func_with_args);
        retval->func_with_args_ = DotNet::CopyOut(func_with_args);
        
        return retval;
    }
    catch (std::exception& e)
    {
        retval->error_ = DotNet::Error(e.what(), argName);
        return retval;
    }
}

