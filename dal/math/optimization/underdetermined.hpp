//
// Created by wegam on 2022/12/10.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/dictionary.hpp>

/*IF--------------------------------------------------------------------------
settings UnderdeterminedControls
	controls for underdetermined search
&members
maxEvaluations is integer
	Give up after this many point evaluations
maxRestarts is integer
	Give up after this many gradient calculations
maxBacktrackTries is integer default 5
	Iteration limit during backtracking linesearch
restartTolerance is number default 0.4
	Restart when k_min is above this limit
backtrackTolerance is number default 0.1
   Don't start backtracking until k_min exceeds this
maxBacktrack is number default 0.8
   Never backtrack more than this fraction of a step
&conditions
maxEvaluations_ > 0
maxRestarts_ > 0
restartTolerance_ >= 0.0 && restartTolerance_ <= 1.0
maxBacktrack_ > backtrackTolerance_ && maxBacktrack_ < 1.0
-IF-------------------------------------------------------------------------*/


namespace Dal {
#include <dal/auto/MG_UnderdeterminedControls_object.hpp>

    namespace Sparse {
        class Square_;
        class SymmetricDecomposition_;
    }

}
