#ifndef DAL_HANDLE_I
#define DAL_HANDLE_I

    %{
#include <dal/platform/platform.hpp>
    %}

    template <class T_> class Handle_ {
    };

    %template(Storable_) Handle_<Storable_>;

#endif