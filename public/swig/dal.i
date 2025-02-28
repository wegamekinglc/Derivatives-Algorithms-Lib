#ifndef DAL_SWIG_I
#define DAL_SWIG_I

%feature("autodoc", "2");

%module dal

%{

#if defined(_MSC_VER)
#pragma comment(lib, "dal.lib")
#pragma comment(lib, "dal_public.lib")
#endif

#include <dal/platform/platform.hpp>
#include <sstream>
      using namespace Dal;

%}

%include "std_string.i"
%include "std_vector.i"

%template(DoubleVector) std::vector<double>;

    %include init.i
    %include handle.i
    %include date.i
    %include cell.i
    %include global.i
    %include strings.i
    %include matrix.i
    %include script.i
    %include models.i
    %include value.i
    %include random.i

#endif