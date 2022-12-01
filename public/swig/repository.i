#ifndef DAL_REPOSITORY_I
#define DAL_REPOSITORY_I

%include "std_vector.i"

%{
#include <public/src/repository.hpp>
%}


%inline %{
    int Repository_Size() {
        return SizeRepository();
    }
%}

#endif