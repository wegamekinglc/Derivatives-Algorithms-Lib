

#pragma managed
#include "MG_Repository_Erase_public_dotnet.hpp"

TALibNet::Repository_Erase_Return_^ TALibNet::Repository_Erase_::Run
    (array<TALibNet::Object_^ >^ dn_objects)
{
    Repository_Erase_Return_^ retval = gcnew Repository_Erase_Return_;
    const char* argName = 0;
    try
    {        
        argName = "objects";
        const Vector_<Handle_<Storable_>> objects = DotNet::ToHandleBaseVector(dn_objects);
        argName = 0;
		int num_erased;
        Repository_Erase(objects, &num_erased);
        retval->num_erased_ = DotNet::CopyOut(num_erased);
        
        return retval;
    }
    catch (std::exception& e)
    {
        retval->error_ = DotNet::Error(e.what(), argName);
        return retval;
    }
}

