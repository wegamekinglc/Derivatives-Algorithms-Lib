//
// Created by wegam on 2022/12/10.
//

#pragma once

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

    namespace Underdetermined {
        using Controls_ = UnderdeterminedControls_;

        class Jacobian_ {
        public:
            virtual ~Jacobian_() = default;

            [[nodiscard]] virtual int Rows() const = 0;
            [[nodiscard]] virtual int Columns() const = 0;
            virtual void DivideRows(const Vector_<>& tol) = 0;
            [[nodiscard]] virtual Vector_<> MultiplyLeft(const Vector_<>& dx) const = 0;
            [[nodiscard]] virtual Vector_<> MultiplyRight(const Vector_<>& t) const = 0;
            virtual void QForm(const Sparse::SymmetricDecomposition_& w, SquareMatrix_<>* form) const = 0;
            virtual void SecantUpdate(const Vector_<>& dx, const Vector_<>& df) = 0;
        };

        class Function_ {
            [[nodiscard]] virtual double BumpSize() const { return 1.0e-4; };
            virtual void FFast(const Vector_<>& x, Vector_<>* f) const {
                *f = F(x);
            }

        public:
            virtual ~Function_() = default;
            [[nodiscard]] virtual Vector_<> F(const Vector_<>& x) const = 0;
            [[nodiscard]] virtual Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const {
                return nullptr;
            }
            virtual void Gradient(const Vector_<>& x, const Vector_<>& f, Matrix_<>* j) const;
        };

        Vector_<> Find(const Function_& func,
                       const Vector_<>& guess,
                       const Vector_<>& tol,
                       const Sparse::SymmetricDecomposition_& w,
                       const Controls_& controls,
                       Matrix_<>* eff_j_inv = nullptr);

        Vector_<> Approximate(const Function_& func_in,
                              const Vector_<>& guess,
                              const Vector_<>& func_tol,
                              double fit_tol,
                              const Sparse::Square_& w,
                              const Controls_& controls);

    }

}
