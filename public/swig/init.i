#ifndef DAL_INIT_I
#define DAL_INIT_I

    %{
#include <dal/platform/initall.hpp>
    %}

    %init %{
        Dal::RegisterAll_::Init();
    %}

#endif