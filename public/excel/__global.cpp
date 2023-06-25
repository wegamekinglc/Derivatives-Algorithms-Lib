//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <public/excel/__platform.hpp>
#include <public/src/global.hpp>

/*IF--------------------------------------------------------------------------
public EvaluationDate_Set
    set the global evaluation date
&inputs
date is date
    the evalution date to set
&outputs
d is cell
    the set evaluation date
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EvaluationDate_Get
    get the global evaluation date
&outputs
date is cell
    the global evaluation date
-IF-------------------------------------------------------------------------*/


namespace Dal {
    namespace {
        void EvaluationDate_Set(const Date_ &date, Cell_ *d) {
            SetEvaluationDate(date);
            *d = date;
        }

        void EvaluationDate_Get(Cell_ *date) {
            *date = GetEvaluationDate();
        }
    }
#ifdef _WIN32
#include <public/auto/MG_EvaluationDate_Set_public.inc>
#include <public/auto/MG_EvaluationDate_Get_public.inc>
#endif
}
