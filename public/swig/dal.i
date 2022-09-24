#ifndef DAL_SWIG_I
#define DAL_SWIG_I

%module dal

%{
#pragma comment(lib, "dal.lib")
#pragma comment(lib, "dal_public.lib")
#include <dal/platform/platform.hpp>
#include <sstream>
      using namespace Dal;

%}

    %include handle.i
    %include date.i
    %include strings.i
    %include matrix.i

    %include random.i

#endif