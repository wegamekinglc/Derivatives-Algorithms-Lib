#ifndef DAL_SWIG_I
#define DAL_SWIG_I

%module dal

%{
#pragma comment(lib, "dal.lib")
#include <dal/platform/platform.hpp>
#include <sstream>
      using namespace Dal;

%}

%include strings.i
%include date.i

#endif