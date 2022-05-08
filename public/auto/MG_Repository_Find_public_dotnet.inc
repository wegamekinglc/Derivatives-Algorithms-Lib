

#pragma managed
#include "MG_Repository_Find_public_dotnet.hpp"

TALibNet::Repository_Find_Return_^ TALibNet::Repository_Find_::Run
    (System::String^ dn_match)
{
    Repository_Find_Return_^ retval = gcnew Repository_Find_Return_;
    const char* argName = 0;
    try
    {        
        argName = "match";
        const String_ match = DotNet::ToString(dn_match);
        argName = 0;
		Vector_<Handle_<Storable_>> objects;
        Repository_Find(match, &objects);
        retval->objects_ = DotNet::CopyOut(objects);
        
        return retval;
    }
    catch (std::exception& e)
    {
        retval->error_ = DotNet::Error(e.what(), argName);
        return retval;
    }
}

