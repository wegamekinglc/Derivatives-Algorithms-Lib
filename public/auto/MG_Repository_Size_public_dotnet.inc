

#pragma managed
#include "MG_Repository_Size_public_dotnet.hpp"

TALibNet::Repository_Size_Return_^ TALibNet::Repository_Size_::Run
    ()
{
    Repository_Size_Return_^ retval = gcnew Repository_Size_Return_;
    const char* argName = 0;
    try
    {        
        argName = 0;
		int size;
        Repository_Size(&size);
        retval->size_ = DotNet::CopyOut(size);
        
        return retval;
    }
    catch (std::exception& e)
    {
        retval->error_ = DotNet::Error(e.what(), argName);
        return retval;
    }
}

