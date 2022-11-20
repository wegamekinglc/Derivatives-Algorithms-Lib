#ifndef DAL_GLOBAL_I
#define DAL_GLOBAL_I

%include date.i
%{
#include <public/src/global.hpp>
%}

%inline %{
    Date_ EvaluationDate_Get() {
        return GetEvaluationDate();
    }

    void EvaluationDate_Set(const Date_& d) {
        SetEvaluationDate(d);
    }
%}

#endif