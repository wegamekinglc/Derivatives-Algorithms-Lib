/* adept_source.h - Source code for the Adept library

  Copyright (C) 2012-2015 The University of Reading
  Copyright (C) 2015-     European Centre for Medium-Range Weather Forecasts

  Licensed under the Apache License, Version 2.0 (the "License"); you
  may not use this file except in compliance with the License.  You
  may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
  implied.  See the License for the specific language governing
  permissions and limitations under the License.


  This file was created automatically by script ./create_adept_source_header 
  on Sun 9 Jan 16:21:44 GMT 2022

  It contains a concatenation of the source files from the Adept
  library. The idea is that a program may #include this file in one of
  its source files (typically the one containing the main function),
  and then the Adept library will be built into the executable without
  the need to link to an external library. All other source files
  should just #include <adept.h> or <adept_arrays.h>. The ability to
  use Adept in this way makes it easier to distribute an Adept package
  that is usable on non-Unix platforms that are unable to use the
  autoconf configure script to build external libraries.

  If HAVE_BLAS is defined below then matrix multiplication will be
  enabled; the BLAS library should be provided at the link stage
  although no header file is required.  If HAVE_LAPACK is defined
  below then linear algebra routines will be enabled (matrix inverse
  and solving linear systems of equations); again, the LAPACK library
  should be provided at the link stage although no header file is
  required.

*/

/* Feel free to delete this warning: */
#ifdef _MSC_FULL_VER 
#pragma message("warning: the adept_source.h header file has not been edited so BLAS matrix multiplication and LAPACK linear-algebra support have been disabled")
#else
#warning "The adept_source.h header file has not been edited so BLAS matrix multiplication and LAPACK linear-algebra support have been disabled"
#endif

/* Uncomment this if you are linking to the BLAS library (header file
   not required) to enable matrix multiplication */
//#define HAVE_BLAS 1

/* Uncomment this if you are linking to the LAPACK library (header
   file not required) */
//#define HAVE_LAPACK 1

/* Uncomment this if you have the cblas.h header from OpenBLAS */
//#define HAVE_OPENBLAS_CBLAS_HEADER

/*

  The individual source files now follow.

*/

#ifndef AdeptSource_H
#define AdeptSource_H 1




// =================================================================
// Contents of config_platform_independent.h
// =================================================================

/* config_platform_independent.h.  Generated from config_platform_independent.h.in by configure.  */
/* config_platform_independent.h.in. */

/* Name of package */
#define PACKAGE "adept"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "r.j.hogan@ecmwf.int"

/* Define to the full name of this package. */
#define PACKAGE_NAME "adept"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "adept 2.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "adept"

/* Define to the home page for this package. */
#define PACKAGE_URL "http://www.met.reading.ac.uk/clouds/adept/"

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.1"

/* Version number of package */
#define VERSION "2.1"



// =================================================================
// Contents of cpplapack.h
// =================================================================

/* cpplapack.h -- C++ interface to LAPACK

    Copyright (C) 2015-2016 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.
*/

#ifndef AdeptCppLapack_H
#define AdeptCppLapack_H 1                       

#include <vector>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LAPACK

extern "C" {
  // External LAPACK Fortran functions
  void sgetrf_(const int* m, const int* n, float*  a, const int* lda, int* ipiv, int* info);
  void dgetrf_(const int* m, const int* n, double* a, const int* lda, int* ipiv, int* info);
  void sgetri_(const int* n, float* a, const int* lda, const int* ipiv, 
	       float* work, const int* lwork, int* info);
  void dgetri_(const int* n, double* a, const int* lda, const int* ipiv, 
	       double* work, const int* lwork, int* info);
  void ssytrf_(const char* uplo, const int* n, float* a, const int* lda, int* ipiv,
	       float* work, const int* lwork, int* info);
  void dsytrf_(const char* uplo, const int* n, double* a, const int* lda, int* ipiv,
	       double* work, const int* lwork, int* info);
  void ssytri_(const char* uplo, const int* n, float* a, const int* lda, 
	       const int* ipiv, float* work, int* info);
  void dsytri_(const char* uplo, const int* n, double* a, const int* lda, 
	       const int* ipiv, double* work, int* info);
  void ssysv_(const char* uplo, const int* n, const int* nrhs, float* a, const int* lda, 
	      int* ipiv, float* b, const int* ldb, float* work, const int* lwork, int* info);
  void dsysv_(const char* uplo, const int* n, const int* nrhs, double* a, const int* lda, 
	      int* ipiv, double* b, const int* ldb, double* work, const int* lwork, int* info);
  void sgesv_(const int* n, const int* nrhs, float* a, const int* lda, 
	      int* ipiv, float* b, const int* ldb, int* info);
  void dgesv_(const int* n, const int* nrhs, double* a, const int* lda, 
	      int* ipiv, double* b, const int* ldb, int* info);
}

namespace adept {

  // Overloaded functions provide both single &
  // double precision versions, and prevents the huge lapacke.h having
  // to be included in all user code
  namespace internal {
    typedef int lapack_int;
    // Factorize a general matrix
    inline
    int cpplapack_getrf(int n, float* a,  int lda, int* ipiv) {
      int info;
      sgetrf_(&n, &n, a, &lda, ipiv, &info);
      return info;
    }
    inline
    int cpplapack_getrf(int n, double* a, int lda, int* ipiv) {
      int info;
      dgetrf_(&n, &n, a, &lda, ipiv, &info);
      return info;
    }

    // Invert a general matrix
    inline
    int cpplapack_getri(int n, float* a,  int lda, const int* ipiv) {
      int info;
      float work_query;
      int lwork = -1;
      // Find out how much work memory required
      sgetri_(&n, a, &lda, ipiv, &work_query, &lwork, &info);
      lwork = static_cast<int>(work_query);
      std::vector<float> work(static_cast<size_t>(lwork));
      // Do full calculation
      sgetri_(&n, a, &lda, ipiv, &work[0], &lwork, &info);
      return info;
    }
    inline
    int cpplapack_getri(int n, double* a,  int lda, const int* ipiv) {
      int info;
      double work_query;
      int lwork = -1;
      // Find out how much work memory required
      dgetri_(&n, a, &lda, ipiv, &work_query, &lwork, &info);
      lwork = static_cast<int>(work_query);
      std::vector<double> work(static_cast<size_t>(lwork));
      // Do full calculation
      dgetri_(&n, a, &lda, ipiv, &work[0], &lwork, &info);
      return info;
    }

    // Factorize a symmetric matrix
    inline
    int cpplapack_sytrf(char uplo, int n, float* a, int lda, int* ipiv) {
      int info;
      float work_query;
      int lwork = -1;
      // Find out how much work memory required
      ssytrf_(&uplo, &n, a, &lda, ipiv, &work_query, &lwork, &info);
      lwork = static_cast<int>(work_query);
      std::vector<float> work(static_cast<size_t>(lwork));
      // Do full calculation
      ssytrf_(&uplo, &n, a, &lda, ipiv, &work[0], &lwork, &info);
      return info;
    }
    inline
    int cpplapack_sytrf(char uplo, int n, double* a, int lda, int* ipiv) {
      int info;
      double work_query;
      int lwork = -1;
      // Find out how much work memory required
      dsytrf_(&uplo, &n, a, &lda, ipiv, &work_query, &lwork, &info);
      lwork = static_cast<int>(work_query);
      std::vector<double> work(static_cast<size_t>(lwork));
      // Do full calculation
      dsytrf_(&uplo, &n, a, &lda, ipiv, &work[0], &lwork, &info);
      return info;
    }

    // Invert a symmetric matrix
    inline
    int cpplapack_sytri(char uplo, int n, float* a, int lda, const int* ipiv) {
      int info;
      std::vector<float> work(n);
      ssytri_(&uplo, &n, a, &lda, ipiv, &work[0], &info);
      return info;
    }
    inline
    int cpplapack_sytri(char uplo, int n, double* a, int lda, const int* ipiv) {
      int info;
      std::vector<double> work(n);
      dsytri_(&uplo, &n, a, &lda, ipiv, &work[0], &info);
      return info;
    }

    // Solve system of linear equations with general matrix
    inline
    int cpplapack_gesv(int n, int nrhs, float* a, int lda,
		       int* ipiv, float* b, int ldb) {
      int info;
      sgesv_(&n, &nrhs, a, &lda, ipiv, b, &lda, &info);
      return info;
    }
    inline
    int cpplapack_gesv(int n, int nrhs, double* a, int lda,
		       int* ipiv, double* b, int ldb) {
      int info;
      dgesv_(&n, &nrhs, a, &lda, ipiv, b, &lda, &info);
      return info;
    }

    // Solve system of linear equations with symmetric matrix
    inline
    int cpplapack_sysv(char uplo, int n, int nrhs, float* a, int lda, int* ipiv,
		       float* b, int ldb) {
      int info;
      float work_query;
      int lwork = -1;
      // Find out how much work memory required
      ssysv_(&uplo, &n, &nrhs, a, &lda, ipiv, b, &ldb, &work_query, &lwork, &info);
      lwork = static_cast<int>(work_query);
      std::vector<float> work(static_cast<size_t>(lwork));
      // Do full calculation
      ssysv_(&uplo, &n, &nrhs, a, &lda, ipiv, b, &ldb, &work[0], &lwork, &info);
      return info;
    }
    inline
    int cpplapack_sysv(char uplo, int n, int nrhs, double* a, int lda, int* ipiv,
		       double* b, int ldb) {
      int info;
      double work_query;
      int lwork = -1;
      // Find out how much work memory required
      dsysv_(&uplo, &n, &nrhs, a, &lda, ipiv, b, &ldb, &work_query, &lwork, &info);
      lwork = static_cast<int>(work_query);
      std::vector<double> work(static_cast<size_t>(lwork));
      // Do full calculation
      dsysv_(&uplo, &n, &nrhs, a, &lda, ipiv, b, &ldb, &work[0], &lwork, &info);
      return info;
    }

  }
}

#endif

#endif


// =================================================================
// Contents of Array.cpp
// =================================================================

/* Array.cpp -- Functions and global variables controlling array behaviour

    Copyright (C) 2015-2016 European Centre for Medium-Range Weather Forecasts

    Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.
*/


#include <adept/Array.h>

namespace adept {
  namespace internal {
    bool array_row_major_order = true;
    //    bool array_print_curly_brackets = true;

    // Variables describing how arrays are written to a stream
    ArrayPrintStyle array_print_style = PRINT_STYLE_CURLY;
    std::string vector_separator = ", ";
    std::string vector_print_before = "{";
    std::string vector_print_after = "}";
    std::string array_opening_bracket = "{";
    std::string array_closing_bracket = "}";
    std::string array_contiguous_separator = ", ";
    std::string array_non_contiguous_separator = ",\n";
    std::string array_print_before = "\n{";
    std::string array_print_after = "}";
    std::string array_print_empty_before = "(empty rank-";
    std::string array_print_empty_after = " array)";
    bool array_print_indent = true;
    bool array_print_empty_rank = true;
  }

  void set_array_print_style(ArrayPrintStyle ps) {
    using namespace internal;
    switch (ps) {
    case PRINT_STYLE_PLAIN:
       vector_separator = " ";
       vector_print_before = "";
       vector_print_after = "";
       array_opening_bracket = "";
       array_closing_bracket = "";
       array_contiguous_separator = " ";
       array_non_contiguous_separator = "\n";
       array_print_before = "";
       array_print_after = "";
       array_print_empty_before = "(empty rank-";
       array_print_empty_after = " array)";
       array_print_indent = false;
       array_print_empty_rank = true;
       break;
    case PRINT_STYLE_CSV:
       vector_separator = ", ";
       vector_print_before = "";
       vector_print_after = "";
       array_opening_bracket = "";
       array_closing_bracket = "";
       array_contiguous_separator = ", ";
       array_non_contiguous_separator = "\n";
       array_print_before = "";
       array_print_after = "";
       array_print_empty_before = "empty";
       array_print_empty_after = "";
       array_print_indent = false;
       array_print_empty_rank = false;
       break;
    case PRINT_STYLE_MATLAB:
       vector_separator = " ";
       vector_print_before = "[";
       vector_print_after = "]";
       array_opening_bracket = "";
       array_closing_bracket = "";
       array_contiguous_separator = " ";
       array_non_contiguous_separator = ";\n";
       array_print_before = "[";
       array_print_after = "]";
       array_print_empty_before = "[";
       array_print_empty_after = "]";
       array_print_indent = true;
       array_print_empty_rank = false;
       break;
    case PRINT_STYLE_CURLY:
       vector_separator = ", ";
       vector_print_before = "{";
       vector_print_after = "}";
       array_opening_bracket = "{";
       array_closing_bracket = "}";
       array_contiguous_separator = ", ";
       array_non_contiguous_separator = ",\n";
       array_print_before = "\n{";
       array_print_after = "}";
       array_print_empty_before = "(empty rank-";
       array_print_empty_after = " array)";
       array_print_indent = true;
       array_print_empty_rank = true;
       break;
    default:
      throw invalid_operation("Array print style not understood");
    }
    array_print_style = ps;
  }

}


// =================================================================
// Contents of Minimizer.cpp
// =================================================================

/* Minimizer.h -- class for minimizing the cost function of an optimizable object

    Copyright (C) 2020 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/

#include <cctype>

#include <adept/Minimizer.h>
#include <adept/exception.h>

namespace adept {

  // List of the names of available minimizer algorithms
  static const char* minimizer_algorithm_names_[]
    = {"L-BFGS",
       "Conjugate-Gradient",
       "Conjugate-Gradient-FR",
       "Levenberg",
       "Levenberg-Marquardt"};

  // Lower-case versions of the list above
  static const char* minimizer_algorithm_lower_names_[]
    = {"l-bfgs",
       "conjugate-gradient",
       "conjugate-gradient-fr",
       "levenberg",
       "levenberg-marquardt"};

  // Convert to lower case, and convert spaces and underscores to
  // hyphens. This function is used to do a case-insensitive
  // string-based selection of the minimizer algorithm to use.
  static void to_lower_in_place(std::string& str) {
    for (std::string::size_type istr = 0; istr < str.size(); ++istr) {
      str[istr] = std::tolower(str[istr]);
      if (str[istr] == ' ' || str[istr] == '_') {
	str[istr] = '-';
      }
    }
  }

  // Return a C string describing the minimizer status
  const char*
  minimizer_status_string(MinimizerStatus status)
  {
    switch (status) {
    case MINIMIZER_STATUS_SUCCESS:
      return "Converged";
      break;
    case MINIMIZER_STATUS_EMPTY_STATE:
      return "Empty state vector, no minimization performed";
      break;
    case MINIMIZER_STATUS_MAX_ITERATIONS_REACHED:
      return "Maximum iterations reached";
      break;
    case MINIMIZER_STATUS_FAILED_TO_CONVERGE:
      return "Failed to converge";
      break;
    case MINIMIZER_STATUS_DIRECTION_UPHILL:
      return "Search direction points uphill";
      break;
    case MINIMIZER_STATUS_BOUND_REACHED:
      return "Bound reached"; // Should not be returned from a minimize function
      break;
    case MINIMIZER_STATUS_INVALID_COST_FUNCTION:
      return "Non-finite cost function";
      break;
    case MINIMIZER_STATUS_INVALID_GRADIENT:
      return "Non-finite gradient";
      break;
    case MINIMIZER_STATUS_INVALID_BOUNDS:
      return "Invalid bounds for bounded minimization";
      break;
    case MINIMIZER_STATUS_NOT_YET_CONVERGED:
      return "Minimization still in progress";
      break;
    default:
      return "Status unrecognized";
    }
  }

  // Case-insensitive setting of the miminization algorithm given its
  // name
  void
  Minimizer::set_algorithm(const std::string& algo) {
    std::string algo_lower = algo;
    to_lower_in_place(algo_lower);

    std::cout << "Checking \"" << algo_lower << "\"\n";

    for (int ialgo = 0;
	 ialgo < static_cast<int>(MINIMIZER_ALGORITHM_NUMBER_AVAILABLE);
	 ++ialgo) {
      if (algo_lower == minimizer_algorithm_lower_names_[ialgo]) {
	set_algorithm(static_cast<MinimizerAlgorithm>(ialgo));
	return;
      }
    }
    throw optimization_exception("Algorithm name not understood");
  }

  std::string
  Minimizer::algorithm_name() {
    int ialgo = static_cast<MinimizerAlgorithm>(algorithm_);
    if (ialgo >= 0 && ialgo < MINIMIZER_ALGORITHM_NUMBER_AVAILABLE) {
      return minimizer_algorithm_names_[ialgo];
    }
    else {
      return "Unknown";
    }
  }

  // Unconstrained minimization
  MinimizerStatus
  Minimizer::minimize(Optimizable& optimizable, Vector x)
  {
    if (minimizer_algorithm_order(algorithm_) > 1
	&& !optimizable.provides_derivative(2)) {
      throw optimization_exception("2nd-order minimization algorithm requires optimizable that can provide 2nd derivatives");
    }
    else if (algorithm_ == MINIMIZER_ALGORITHM_LIMITED_MEMORY_BFGS) {
      return minimize_limited_memory_bfgs(optimizable, x);
    }
    else if (algorithm_ == MINIMIZER_ALGORITHM_CONJUGATE_GRADIENT) {
      return minimize_conjugate_gradient(optimizable, x);
    }
    else if (algorithm_ == MINIMIZER_ALGORITHM_CONJUGATE_GRADIENT_FR) {
      return minimize_conjugate_gradient(optimizable, x, true);
    }
    else if (algorithm_ == MINIMIZER_ALGORITHM_LEVENBERG) {
      return minimize_levenberg_marquardt(optimizable, x, true);
    }
    else if (algorithm_ == MINIMIZER_ALGORITHM_LEVENBERG_MARQUARDT) {
      return minimize_levenberg_marquardt(optimizable, x, false);
    }
    else {
      throw optimization_exception("Minimization algorithm not recognized");
    }
  }

  // Constrained minimization
  MinimizerStatus
  Minimizer::minimize(Optimizable& optimizable, Vector x,
		      const Vector& x_lower, const Vector& x_upper)
  {
    if (minimizer_algorithm_order(algorithm_) > 1
	&& !optimizable.provides_derivative(2)) {
      throw optimization_exception("2nd-order minimization algorithm requires optimizable that can provide 2nd derivatives");
    }
    if (algorithm_ == MINIMIZER_ALGORITHM_LIMITED_MEMORY_BFGS) {
      return minimize_limited_memory_bfgs_bounded(optimizable, x,
						  x_lower, x_upper);
    }
    else if (algorithm_ == MINIMIZER_ALGORITHM_CONJUGATE_GRADIENT) {
      return minimize_conjugate_gradient_bounded(optimizable, x,
						 x_lower, x_upper);
    }
    else if (algorithm_ == MINIMIZER_ALGORITHM_CONJUGATE_GRADIENT_FR) {
      return minimize_conjugate_gradient_bounded(optimizable, x,
						 x_lower, x_upper, true);
    }
    if (algorithm_ == MINIMIZER_ALGORITHM_LEVENBERG) {
      return minimize_levenberg_marquardt_bounded(optimizable, x,
						  x_lower, x_upper, true);
    }
    if (algorithm_ == MINIMIZER_ALGORITHM_LEVENBERG_MARQUARDT) {
      return minimize_levenberg_marquardt_bounded(optimizable, x,
						  x_lower, x_upper, false);
    }
    else {
      throw optimization_exception("Constrained minimization algorithm not recognized");
    }
  }

};


// =================================================================
// Contents of Stack.cpp
// =================================================================

/* Stack.cpp -- Stack for storing automatic differentiation information

     Copyright (C) 2012-2014 University of Reading
    Copyright (C) 2015 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/


#include <iostream>
#include <cstring> // For memcpy


#ifdef _OPENMP
#include <omp.h>
#endif

#include <adept/Stack.h>


namespace adept {

  using namespace internal;

  // Global pointers to the current thread, the second of which is
  // thread safe. The first is only used if ADEPT_STACK_THREAD_UNSAFE
  // is defined.
  ADEPT_THREAD_LOCAL Stack* _stack_current_thread = 0;
  Stack* _stack_current_thread_unsafe = 0;

  // MEMBER FUNCTIONS OF THE STACK CLASS

  // Destructor: frees dynamically allocated memory (if any)
  Stack::~Stack() {
    // If this is the currently active stack then set to NULL as
    // "this" is shortly to become invalid
    if (is_thread_unsafe_) {
      if (_stack_current_thread_unsafe == this) {
	_stack_current_thread_unsafe = 0; 
      }
    }
    else if (_stack_current_thread == this) {
      _stack_current_thread = 0; 
    }
#ifndef ADEPT_STACK_STORAGE_STL
    if (gradient_) {
      delete[] gradient_;
    }
#endif
  }
  
  // Make this stack "active" by copying its "this" pointer to a
  // global variable; this makes it the stack that aReal objects
  // subsequently interact with when being created and participating
  // in mathematical expressions
  void
  Stack::activate()
  {
    // Check that we don't already have an active stack in this thread
    if ((is_thread_unsafe_ && _stack_current_thread_unsafe 
	 && _stack_current_thread_unsafe != this)
	|| ((!is_thread_unsafe_) && _stack_current_thread
	    && _stack_current_thread != this)) {
      throw(stack_already_active());
    }
    else {
      if (!is_thread_unsafe_) {
	_stack_current_thread = this;
      }
      else {
	_stack_current_thread_unsafe = this;
      }
    }    
  }

  
  // Set the maximum number of threads to be used in Jacobian
  // calculations, if possible. A value of 1 indicates that OpenMP
  // will not be used, while a value of 0 indicates that the number
  // will match the number of available processors. Returns the
  // maximum that will be used, which will be 1 if the Adept library
  // was compiled without OpenMP support. Note that a value of 1 will
  // disable the use of OpenMP with Adept, so Adept will then use no
  // OpenMP directives or function calls. Note that if in your program
  // you use OpenMP with each thread performing automatic
  // differentiaion with its own independent Adept stack, then
  // typically only one OpenMP thread is available for each Jacobian
  // calculation, regardless of whether you call this function.
  int
  Stack::set_max_jacobian_threads(int n)
  {
#ifdef _OPENMP
    if (have_openmp_) {
      if (n == 1) {
	openmp_manually_disabled_ = true;
	return 1;
      }
      else if (n < 1) {
	openmp_manually_disabled_ = false;
	omp_set_num_threads(omp_get_num_procs());
	return omp_get_max_threads();
      }
      else {
	openmp_manually_disabled_ = false;
	omp_set_num_threads(n);
	return omp_get_max_threads();
      }
    }
#endif
    return 1;
  }


  // Return maximum number of OpenMP threads to be used in Jacobian
  // calculation
  int 
  Stack::max_jacobian_threads() const
  {
#ifdef _OPENMP
    if (have_openmp_) {
      if (openmp_manually_disabled_) {
	return 1;
      }
      else {
	return omp_get_max_threads();
      }
    }
#endif
    return 1;
  }


  // Perform to adjoint computation (reverse mode). It is assumed that
  // some gradients have been assigned already, otherwise the function
  // returns with an error.
  void
  Stack::compute_adjoint()
  {
    if (gradients_are_initialized()) {
      // Loop backwards through the derivative statements
      for (uIndex ist = n_statements_-1; ist > 0; ist--) {
	const Statement& statement = statement_[ist];
	// We copy the RHS gradient (LHS in the original derivative
	// statement but swapped in the adjoint equivalent) to "a" in
	// case it appears on the LHS in any of the following statements
	Real a = gradient_[statement.index];
	gradient_[statement.index] = 0.0;
	// By only looping if a is non-zero we gain a significant speed-up
	if (a != 0.0) {
	  // Loop over operations
	  for (uIndex i = statement_[ist-1].end_plus_one;
	       i < statement.end_plus_one; i++) {
	    gradient_[index_[i]] += multiplier_[i]*a;
	  }
	}
      }
    }  
    else {
      throw(gradients_not_initialized());
    }  
  }


  // Perform tangent linear computation (forward mode). It is assumed
  // that some gradients have been assigned already, otherwise the
  // function returns with an error.
  void
  Stack::compute_tangent_linear()
  {
    if (gradients_are_initialized()) {
      // Loop forward through the statements
      for (uIndex ist = 1; ist < n_statements_; ist++) {
	const Statement& statement = statement_[ist];
	// We copy the LHS to "a" in case it appears on the RHS in any
	// of the following statements
	Real a = 0.0;
	for (uIndex i = statement_[ist-1].end_plus_one;
	     i < statement.end_plus_one; i++) {
	  a += multiplier_[i]*gradient_[index_[i]];
	}
	gradient_[statement.index] = a;
      }
    }
    else {
      throw(gradients_not_initialized());
    }
  }



  // Register n gradients
  uIndex
  Stack::do_register_gradients(const uIndex& n) {
    n_gradients_registered_ += n;
    if (!gap_list_.empty()) {
      uIndex return_val;
      // Insert in a gap, if there is one big enough
      for (GapListIterator it = gap_list_.begin();
	   it != gap_list_.end(); it++) {
	uIndex len = it->end + 1 - it->start;
	if (len > n) {
	  // Gap a bit larger than needed: reduce its size
	  return_val = it->start;
	  it->start += n;
	  return return_val;
	}
	else if (len == n) {
	  // Gap exactly the size needed: fill it and remove from list
	  return_val = it->start;
	  if (most_recent_gap_ == it) {
	    gap_list_.erase(it);
	    most_recent_gap_ = gap_list_.end();
	  }
	  else {
	    gap_list_.erase(it);
	  }
	  return return_val;
	}
      }
    }
    // No suitable gap found; instead add to end of gradient vector
    i_gradient_ += n;
    if (i_gradient_ > max_gradient_) {
      max_gradient_ = i_gradient_;
    }
    return i_gradient_ - n;
  }
  

  // If an aReal object is deleted, its gradient_index is
  // unregistered from the stack.  If this is at the top of the stack
  // then this is easy and is done inline; this is the usual case
  // since C++ trys to deallocate automatic objects in the reverse
  // order to that in which they were allocated.  If it is not at the
  // top of the stack then a non-inline function is called to ensure
  // that the gap list is adjusted correctly.
  void
  Stack::unregister_gradient_not_top(const uIndex& gradient_index)
  {
    enum {
      ADDED_AT_BASE,
      ADDED_AT_TOP,
      NEW_GAP,
      NOT_FOUND
    } status = NOT_FOUND;
    // First try to find if the unregistered element is at the
    // start or end of an existing gap
    if (!gap_list_.empty() && most_recent_gap_ != gap_list_.end()) {
      // We have a "most recent" gap - check whether the gradient
      // to be unregistered is here
      Gap& current_gap = *most_recent_gap_;
      if (gradient_index == current_gap.start - 1) {
	current_gap.start--;
	status = ADDED_AT_BASE;
      }
      else if (gradient_index == current_gap.end + 1) {
	current_gap.end++;
	status = ADDED_AT_TOP;
      }
      // Should we check for erroneous removal from middle of gap?
    }
    if (status == NOT_FOUND) {
      // Search other gaps
      for (GapListIterator it = gap_list_.begin();
	   it != gap_list_.end(); it++) {
	if (gradient_index <= it->end + 1) {
	  // Gradient to unregister is either within the gap
	  // referenced by iterator "it", or it is between "it"
	  // and the previous gap in the list
	  if (gradient_index == it->start - 1) {
	    status = ADDED_AT_BASE;
	    it->start--;
	    most_recent_gap_ = it;
	  }
	  else if (gradient_index == it->end + 1) {
	    status = ADDED_AT_TOP;
	    it->end++;
	    most_recent_gap_ = it;
	  }
	  else {
	    // Insert a new gap of width 1; note that list::insert
	    // inserts *before* the specified location
	    most_recent_gap_
	      = gap_list_.insert(it, Gap(gradient_index));
	    status = NEW_GAP;
	  }
	  break;
	}
      }
      if (status == NOT_FOUND) {
	gap_list_.push_back(Gap(gradient_index));
	most_recent_gap_ = gap_list_.end();
	most_recent_gap_--;
      }
    }
    // Finally check if gaps have merged
    if (status == ADDED_AT_BASE
	&& most_recent_gap_ != gap_list_.begin()) {
      // Check whether the gap has merged with the next one
      GapListIterator it = most_recent_gap_;
      it--;
      if (it->end == most_recent_gap_->start - 1) {
	// Merge two gaps
	most_recent_gap_->start = it->start;
	gap_list_.erase(it);
      }
    }
    else if (status == ADDED_AT_TOP) {
      GapListIterator it = most_recent_gap_;
      it++;
      if (it != gap_list_.end()
	  && it->start == most_recent_gap_->end + 1) {
	// Merge two gaps
	most_recent_gap_->end = it->end;
	gap_list_.erase(it);
      }
    }
  }	


  // Unregister n gradients starting at gradient_index
  void
  Stack::unregister_gradients(const uIndex& gradient_index,
			      const uIndex& n)
  {
    n_gradients_registered_ -= n;
    if (gradient_index+n == i_gradient_) {
      // Gradient to be unregistered is at the top of the stack
      i_gradient_ -= n;
      if (!gap_list_.empty()) {
	Gap& last_gap = gap_list_.back();
	if (i_gradient_ == last_gap.end+1) {
	  // We have unregistered the elements between the "gap" of
	  // unregistered element and the top of the stack, so can set
	  // the variables indicating the presence of the gap to zero
	  i_gradient_ = last_gap.start;
	  GapListIterator it = gap_list_.end();
	  it--;
	  if (most_recent_gap_ == it) {
	    most_recent_gap_ = gap_list_.end();
	  }
	  gap_list_.pop_back();
	}
      }
    }
    else { // Gradients to be unregistered not at top of stack.
      enum {
	ADDED_AT_BASE,
	ADDED_AT_TOP,
	NEW_GAP,
	NOT_FOUND
      } status = NOT_FOUND;
      // First try to find if the unregistered element is at the start
      // or end of an existing gap
      if (!gap_list_.empty() && most_recent_gap_ != gap_list_.end()) {
	// We have a "most recent" gap - check whether the gradient
	// to be unregistered is here
	Gap& current_gap = *most_recent_gap_;
	if (gradient_index == current_gap.start - n) {
	  current_gap.start -= n;
	  status = ADDED_AT_BASE;
	}
	else if (gradient_index == current_gap.end + 1) {
	  current_gap.end += n;
	  status = ADDED_AT_TOP;
	}
	/*
	else if (gradient_index > current_gap.start - n
		 && gradient_index < current_gap.end + 1) {
	  std::cout << "** Attempt to find " << gradient_index << " in gaps ";
	  print_gaps();
	  std::cout << "\n";
	  throw invalid_operation("Gap list corruption");
	}
	*/
	// Should we check for erroneous removal from middle of gap?
      }
      if (status == NOT_FOUND) {
	// Search other gaps
	for (GapListIterator it = gap_list_.begin();
	     it != gap_list_.end(); it++) {
	  if (gradient_index <= it->end + 1) {
	    // Gradient to unregister is either within the gap
	    // referenced by iterator "it", or it is between "it" and
	    // the previous gap in the list
	    if (gradient_index == it->start - n) {
	      status = ADDED_AT_BASE;
	      it->start -= n;
	      most_recent_gap_ = it;
	    }
	    else if (gradient_index == it->end + 1) {
	      status = ADDED_AT_TOP;
	      it->end += n;
	      most_recent_gap_ = it;
	    }
	    /*
	    else if (gradient_index > it->start - n) {
	      std::cout << "*** Attempt to find " << gradient_index << " in gaps ";
	      print_gaps();
	      std::cout << "\n";
	      throw invalid_operation("Gap list corruption");
	    }
	    */
	    else {
	      // Insert a new gap; note that list::insert inserts
	      // *before* the specified location
	      most_recent_gap_
		= gap_list_.insert(it, Gap(gradient_index,
					   gradient_index+n-1));
	      status = NEW_GAP;
	    }
	    break;
	  }
	}
	if (status == NOT_FOUND) {
	  gap_list_.push_back(Gap(gradient_index,
				  gradient_index+n-1));
	  most_recent_gap_ = gap_list_.end();
	  most_recent_gap_--;
	}
      }
      // Finally check if gaps have merged
      if (status == ADDED_AT_BASE
	  && most_recent_gap_ != gap_list_.begin()) {
	// Check whether the gap has merged with the next one
	GapListIterator it = most_recent_gap_;
	it--;
	if (it->end == most_recent_gap_->start - 1) {
	  // Merge two gaps
	  most_recent_gap_->start = it->start;
	  gap_list_.erase(it);
	}
      }
      else if (status == ADDED_AT_TOP) {
	GapListIterator it = most_recent_gap_;

	it++;
	if (it != gap_list_.end()
	    && it->start == most_recent_gap_->end + 1) {
	  // Merge two gaps
	  most_recent_gap_->end = it->end;
	  gap_list_.erase(it);
	}
      }
    }
  }
  
  
  // Print each derivative statement to the specified stream (standard
  // output if omitted)
  void
  Stack::print_statements(std::ostream& os) const
  {
    for (uIndex ist = 1; ist < n_statements_; ist++) {
      const Statement& statement = statement_[ist];
      os << ist
		<< ": d[" << statement.index
		<< "] = ";
      
      if (statement_[ist-1].end_plus_one == statement_[ist].end_plus_one) {
	os << "0\n";
      }
      else {    
	for (uIndex i = statement_[ist-1].end_plus_one;
	     i < statement.end_plus_one; i++) {
	  os << " + " << multiplier_[i] << "*d[" << index_[i] << "]";
	}
	os << "\n";
      }
    }
  }
  
  // Print the current gradient list to the specified stream (standard
  // output if omitted)
  bool
  Stack::print_gradients(std::ostream& os) const
  {
    if (gradients_are_initialized()) {
      for (uIndex i = 0; i < max_gradient_; i++) {
	if (i%10 == 0) {
	  if (i != 0) {
	    os << "\n";
	  }
	  os << i << ":";
	}
	os << " " << gradient_[i];
      }
      os << "\n";
      return true;
    }
    else {
      os << "No gradients initialized\n";
      return false;
    }
  }

  // Print the list of gaps in the gradient list to the specified
  // stream (standard output if omitted)
  void
  Stack::print_gaps(std::ostream& os) const
  {
    for (std::list<Gap>::const_iterator it = gap_list_.begin();
	 it != gap_list_.end(); it++) {
      os << it->start << "-" << it->end << " ";
    }
  }


#ifndef ADEPT_STACK_STORAGE_STL
  // Initialize the vector of gradients ready for the adjoint
  // calculation
  void
  Stack::initialize_gradients()
  {
    if (max_gradient_ > 0) {
      if (n_allocated_gradients_ < max_gradient_) {
	if (gradient_) {
	  delete[] gradient_;
	}
	gradient_ = new Real[max_gradient_];
	n_allocated_gradients_ = max_gradient_;
      }
      for (uIndex i = 0; i < max_gradient_; i++) {
	gradient_[i] = 0.0;
      }
    }
    gradients_initialized_ = true;
  }
#else
  void
  Stack::initialize_gradients()
  {
    gradient_.resize(max_gradient_+10, 0.0);
      gradients_initialized_ = true;
  }
#endif

  // Report information about the stack to the specified stream, or
  // standard output if omitted; note that this is synonymous with
  // sending the Stack object to a stream using the "<<" operator.
  void
  Stack::print_status(std::ostream& os) const
  {
    os << "Automatic Differentiation Stack (address " << this << "):\n";
    if ((!is_thread_unsafe_) && _stack_current_thread == this) {
      os << "   Currently attached - thread safe\n";
    }
    else if (is_thread_unsafe_ && _stack_current_thread_unsafe == this) {
      os << "   Currently attached - thread unsafe\n";
    }
    else {
      os << "   Currently detached\n";
    }
    os << "   Recording status:\n";
    if (is_recording_) {
      os << "      Recording is ON\n";  
    }
    else {
      os << "      Recording is PAUSED\n";
    }
    // Account for the null statement at the start by subtracting one
    os << "      " << n_statements()-1 << " statements (" 
       << n_allocated_statements() << " allocated)";
    os << " and " << n_operations() << " operations (" 
       << n_allocated_operations() << " allocated)\n";
    os << "      " << n_gradients_registered() << " gradients currently registered ";
    os << "and a total of " << max_gradients() << " needed (current index "
       << i_gradient() << ")\n";
    if (gap_list_.empty()) {
      os << "      Gradient list has no gaps\n";
    }
    else {
      os << "      Gradient list has " << gap_list_.size() << " gaps (";
      print_gaps(os);
      os << ")\n";
    }
    os << "   Computation status:\n";
    if (gradients_are_initialized()) {
      os << "      " << max_gradients() << " gradients assigned (" 
	 << n_allocated_gradients() << " allocated)\n";
    }
    else {
      os << "      0 gradients assigned (" << n_allocated_gradients()
	 << " allocated)\n";
    }
    os << "      Jacobian size: " << n_dependents() << "x" << n_independents() << "\n";
    if (n_dependents() <= 10 && n_independents() <= 10) {
      os << "      Independent indices:";
      for (std::size_t i = 0; i < independent_index_.size(); ++i) {
	os << " " << independent_index_[i];
      }
      os << "\n      Dependent indices:  ";
      for (std::size_t i = 0; i < dependent_index_.size(); ++i) {
	os << " " << dependent_index_[i];
      }
      os << "\n";
    }

#ifdef _OPENMP
    if (have_openmp_) {
      if (openmp_manually_disabled_) {
	os << "      Parallel Jacobian calculation manually disabled\n";
      }
      else {
	os << "      Parallel Jacobian calculation can use up to "
	   << omp_get_max_threads() << " threads\n";
	os << "      Each thread treats " << ADEPT_MULTIPASS_SIZE 
	   << " (in)dependent variables\n";
      }
    }
    else {
#endif
      os << "      Parallel Jacobian calculation not available\n";
#ifdef _OPENMP
    }
#endif
  }
} // End namespace adept



// =================================================================
// Contents of StackStorageOrig.cpp
// =================================================================

/* StackStorageOrig.cpp -- Original storage of stacks using STL containers

    Copyright (C) 2014-2015 University of Reading

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

   The Stack class inherits from a class providing the storage (and
   interface to the storage) for the derivative statements that are
   accumulated during the execution of an algorithm.  The derivative
   statements are held in two stacks described by Hogan (2014): the
   "statement stack" and the "operation stack".

   This file provides one of the original storage engine, which used
   std::vector to hold the two stacks. Note that these stacks are
   contiguous in memory, which is not ideal for very large algorithms.

*/

#include <cstring>

#include <adept/StackStorageOrig.h>

namespace adept {
  namespace internal {

    StackStorageOrig::~StackStorageOrig() {
      if (statement_) {
	delete[] statement_;
      }
      if (multiplier_) {
	delete[] multiplier_;
      }
      if (index_) {
	delete[] index_;
      }
    }


    // Double the size of the operation stack, or grow it even more if
    // the requested minimum number of extra entries (min) is greater
    // than this would allow
    void
    StackStorageOrig::grow_operation_stack(uIndex min)
    {
      uIndex new_size = 2*n_allocated_operations_;
      if (min > 0 && new_size < n_allocated_operations_+min) {
	new_size += min;
      }
      Real* new_multiplier = new Real[new_size];
      uIndex* new_index = new uIndex[new_size];
      
      std::memcpy(new_multiplier, multiplier_, n_operations_*sizeof(Real));
      std::memcpy(new_index, index_, n_operations_*sizeof(uIndex));
      
      delete[] multiplier_;
      delete[] index_;
      
      multiplier_ = new_multiplier;
      index_ = new_index;
      
      n_allocated_operations_ = new_size;
    }
    
    // ... likewise for the statement stack
    void
    StackStorageOrig::grow_statement_stack(uIndex min)
    {
      uIndex new_size = 2*n_allocated_statements_;
      if (min > 0 && new_size < n_allocated_statements_+min) {
	new_size += min;
      }
      Statement* new_statement = new Statement[new_size];
      std::memcpy(new_statement, statement_,
		  n_statements_*sizeof(Statement));
      delete[] statement_;
      
      statement_ = new_statement;
      
      n_allocated_statements_ = new_size;
    }

  }
}


// =================================================================
// Contents of Storage.cpp
// =================================================================

/* Storage.cpp -- Global variables recording use of Storage objects

    Copyright (C) 2015 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/

#include <adept/Storage.h>

namespace adept {
  namespace internal {
    Index n_storage_objects_created_;
    Index n_storage_objects_deleted_;
  }
}


// =================================================================
// Contents of cppblas.cpp
// =================================================================

/* cppblas.cpp -- C++ interface to BLAS functions

    Copyright (C) 2015-2016 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

   This file provides a C++ interface to selected Level-2 and -3 BLAS
   functions in which the precision of the arguments (float versus
   double) is inferred via overloading

*/

#include <adept/exception.h>
#include <adept/cppblas.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_BLAS

extern "C" {
  void sgemm_(const char* TransA, const char* TransB, const int* M,
	      const int* N, const int* K, const float* alpha,
	      const float* A, const int* lda, const float* B, const int* ldb,
	      const float* beta, const float* C, const int* ldc);
  void dgemm_(const char* TransA, const char* TransB, const int* M,
	      const int* N, const int* K, const double* alpha,
	      const double* A, const int* lda, const double* B, const int* ldb,
	      const double* beta, const double* C, const int* ldc);
  void sgemv_(const char* TransA, const int* M, const int* N, const float* alpha,
	      const float* A, const int* lda, const float* X, const int* incX,
	      const float* beta, const float* Y, const int* incY);
  void dgemv_(const char* TransA, const int* M, const int* N, const double* alpha,
	      const double* A, const int* lda, const double* X, const int* incX,
	      const double* beta, const double* Y, const int* incY);
  void ssymm_(const char* side, const char* uplo, const int* M, const int* N,
	      const float* alpha, const float* A, const int* lda, const float* B,
	      const int* ldb, const float* beta, float* C, const int* ldc);
  void dsymm_(const char* side, const char* uplo, const int* M, const int* N,
	      const double* alpha, const double* A, const int* lda, const double* B,
	      const int* ldb, const double* beta, double* C, const int* ldc);
  void ssymv_(const char* uplo, const int* N, const float* alpha, const float* A, 
	      const int* lda, const float* X, const int* incX, const float* beta, 
	      const float* Y, const int* incY);
  void dsymv_(const char* uplo, const int* N, const double* alpha, const double* A, 
	      const int* lda, const double* X, const int* incX, const double* beta, 
	      const double* Y, const int* incY);
  void sgbmv_(const char* TransA, const int* M, const int* N, const int* kl, 
	      const int* ku, const float* alpha, const float* A, const int* lda,
	      const float* X, const int* incX, const float* beta, 
	      const float* Y, const int* incY);
  void dgbmv_(const char* TransA, const int* M, const int* N, const int* kl, 
	      const int* ku, const double* alpha, const double* A, const int* lda,
	      const double* X, const int* incX, const double* beta, 
	      const double* Y, const int* incY);
}

namespace adept {

  namespace internal {
    
    // Matrix-matrix multiplication for general dense matrices
#define ADEPT_DEFINE_GEMM(T, FUNC, FUNC_COMPLEX)		\
    void cppblas_gemm(BLAS_ORDER Order,				\
		      BLAS_TRANSPOSE TransA,			\
		      BLAS_TRANSPOSE TransB,			\
		      int M, int N,				\
		      int K, T alpha, const T *A,		\
		      int lda, const T *B, int ldb,		\
		      T beta, T *C, int ldc) {			\
      if (Order == BlasColMajor) {				\
        FUNC(&TransA, &TransB, &M, &N, &K, &alpha, A, &lda,	\
	     B, &ldb, &beta, C, &ldc);				\
      }								\
      else {							\
        FUNC(&TransB, &TransA, &N, &M, &K, &alpha, B, &ldb,	\
	     A, &lda, &beta, C, &ldc);				\
      }								\
    }
    ADEPT_DEFINE_GEMM(double, dgemm_, zgemm_)
    ADEPT_DEFINE_GEMM(float,  sgemm_, cgemm_)
#undef ADEPT_DEFINE_GEMM
    
    // Matrix-vector multiplication for a general dense matrix
#define ADEPT_DEFINE_GEMV(T, FUNC, FUNC_COMPLEX)		\
    void cppblas_gemv(const BLAS_ORDER Order,			\
		      const BLAS_TRANSPOSE TransA,		\
		      const int M, const int N,			\
		      const T alpha, const T *A, const int lda,	\
		      const T *X, const int incX, const T beta,	\
		      T *Y, const int incY) {			\
      if (Order == BlasColMajor) {				\
        FUNC(&TransA, &M, &N, &alpha, A, &lda, X, &incX, 	\
	     &beta, Y, &incY);					\
      }								\
      else {							\
        BLAS_TRANSPOSE TransNew					\
	  = TransA == BlasTrans ? BlasNoTrans : BlasTrans;	\
        FUNC(&TransNew, &N, &M, &alpha, A, &lda, X, &incX, 	\
	     &beta, Y, &incY);					\
      }								\
    }
    ADEPT_DEFINE_GEMV(double, dgemv_, zgemv_)
    ADEPT_DEFINE_GEMV(float,  sgemv_, cgemv_)
#undef ADEPT_DEFINE_GEMV
    
    // Matrix-matrix multiplication where matrix A is symmetric
    // FIX! CHECK ROW MAJOR VERSION IS RIGHT			
#define ADEPT_DEFINE_SYMM(T, FUNC, FUNC_COMPLEX)			\
    void cppblas_symm(const BLAS_ORDER Order,				\
		      const BLAS_SIDE Side,				\
		      const BLAS_UPLO Uplo,				\
		      const int M, const int N,				\
		      const T alpha, const T *A, const int lda,		\
		      const T *B, const int ldb, const T beta,		\
		      T *C, const int ldc) {				\
      if (Order == BlasColMajor) {					\
        FUNC(&Side, &Uplo, &M, &N, &alpha, A, &lda,			\
	     B, &ldb, &beta, C, &ldc);					\
      }									\
      else {								\
	BLAS_SIDE SideNew = Side == BlasLeft  ? BlasRight : BlasLeft;	\
	BLAS_UPLO UploNew = Uplo == BlasUpper ? BlasLower : BlasUpper;  \
        FUNC(&SideNew, &UploNew, &N, &M, &alpha, A, &lda,		\
	     B, &ldb, &beta, C, &ldc);					\
      }									\
    }
    ADEPT_DEFINE_SYMM(double, dsymm_, zsymm_)
    ADEPT_DEFINE_SYMM(float,  ssymm_, csymm_)
#undef ADEPT_DEFINE_SYMM
    
    // Matrix-vector multiplication where the matrix is symmetric
#define ADEPT_DEFINE_SYMV(T, FUNC, FUNC_COMPLEX)			\
    void cppblas_symv(const BLAS_ORDER Order,				\
		      const BLAS_UPLO Uplo,				\
		      const int N, const T alpha, const T *A,		\
		      const int lda, const T *X, const int incX,	\
		      const T beta, T *Y, const int incY) {		\
      if (Order == BlasColMajor) {					\
        FUNC(&Uplo, &N, &alpha, A, &lda, X, &incX, &beta, Y, &incY);	\
      }									\
      else {								\
        BLAS_UPLO UploNew = Uplo == BlasUpper ? BlasLower : BlasUpper;  \
        FUNC(&UploNew, &N, &alpha, A, &lda, X, &incX, &beta, Y, &incY);	\
      }									\
    }
    ADEPT_DEFINE_SYMV(double, dsymv_, zsymv_)
    ADEPT_DEFINE_SYMV(float,  ssymv_, csymv_)
#undef ADEPT_DEFINE_SYMV
    
    // Matrix-vector multiplication for a general band matrix
#define ADEPT_DEFINE_GBMV(T, FUNC, FUNC_COMPLEX)		\
    void cppblas_gbmv(const BLAS_ORDER Order,			\
		      const BLAS_TRANSPOSE TransA,		\
		      const int M, const int N,			\
		      const int KL, const int KU, const T alpha,\
		      const T *A, const int lda, const T *X,	\
		      const int incX, const T beta, T *Y,	\
		      const int incY) {				\
      if (Order == BlasColMajor) {				\
        FUNC(&TransA, &M, &N, &KL, &KU, &alpha, A, &lda,	\
	     X, &incX, &beta, Y, &incY);			\
      }								\
      else {							\
	BLAS_TRANSPOSE TransNew					\
	  = TransA == BlasTrans ? BlasNoTrans : BlasTrans;	\
	FUNC(&TransNew, &N, &M, &KU, &KL, &alpha, A, &lda,	\
	     X, &incX, &beta, Y, &incY);			\
      }								\
    }
    ADEPT_DEFINE_GBMV(double, dgbmv_, zgbmv_)
    ADEPT_DEFINE_GBMV(float,  sgbmv_, cgbmv_)
#undef ADEPT_DEFINE_GBMV
  
  } // End namespace internal
  
} // End namespace adept
  

#else // Don't have BLAS


namespace adept {

  namespace internal {
    
    // Matrix-matrix multiplication for general dense matrices
#define ADEPT_DEFINE_GEMM(T, FUNC, FUNC_COMPLEX)		\
    void cppblas_gemm(BLAS_ORDER Order,				\
		      BLAS_TRANSPOSE TransA,			\
		      BLAS_TRANSPOSE TransB,			\
		      int M, int N,				\
		      int K, T alpha, const T *A,		\
		      int lda, const T *B, int ldb,		\
		      T beta, T *C, int ldc) {			\
      throw feature_not_available("Cannot perform matrix-matrix multiplication because compiled without BLAS"); \
    }
    ADEPT_DEFINE_GEMM(double, dgemm_, zgemm_)
    ADEPT_DEFINE_GEMM(float,  sgemm_, cgemm_)
#undef ADEPT_DEFINE_GEMM
    
    // Matrix-vector multiplication for a general dense matrix
#define ADEPT_DEFINE_GEMV(T, FUNC, FUNC_COMPLEX)		\
    void cppblas_gemv(const BLAS_ORDER Order,			\
		      const BLAS_TRANSPOSE TransA,		\
		      const int M, const int N,			\
		      const T alpha, const T *A, const int lda,	\
		      const T *X, const int incX, const T beta,	\
		      T *Y, const int incY) {			\
      throw feature_not_available("Cannot perform matrix-vector multiplication because compiled without BLAS"); \
    }
    ADEPT_DEFINE_GEMV(double, dgemv_, zgemv_)
    ADEPT_DEFINE_GEMV(float,  sgemv_, cgemv_)
#undef ADEPT_DEFINE_GEMV
    
    // Matrix-matrix multiplication where matrix A is symmetric
    // FIX! CHECK ROW MAJOR VERSION IS RIGHT			
#define ADEPT_DEFINE_SYMM(T, FUNC, FUNC_COMPLEX)			\
    void cppblas_symm(const BLAS_ORDER Order,				\
		      const BLAS_SIDE Side,				\
		      const BLAS_UPLO Uplo,				\
		      const int M, const int N,				\
		      const T alpha, const T *A, const int lda,		\
		      const T *B, const int ldb, const T beta,		\
		      T *C, const int ldc) {				\
      throw feature_not_available("Cannot perform symmetric matrix-matrix multiplication because compiled without BLAS"); \
    }
    ADEPT_DEFINE_SYMM(double, dsymm_, zsymm_)
    ADEPT_DEFINE_SYMM(float,  ssymm_, csymm_)
#undef ADEPT_DEFINE_SYMM
    
    // Matrix-vector multiplication where the matrix is symmetric
#define ADEPT_DEFINE_SYMV(T, FUNC, FUNC_COMPLEX)			\
    void cppblas_symv(const BLAS_ORDER Order,				\
		      const BLAS_UPLO Uplo,				\
		      const int N, const T alpha, const T *A,		\
		      const int lda, const T *X, const int incX,	\
		      const T beta, T *Y, const int incY) {		\
      throw feature_not_available("Cannot perform symmetric matrix-vector multiplication because compiled without BLAS"); \
    }
    ADEPT_DEFINE_SYMV(double, dsymv_, zsymv_)
    ADEPT_DEFINE_SYMV(float,  ssymv_, csymv_)
#undef ADEPT_DEFINE_SYMV
    
    // Matrix-vector multiplication for a general band matrix
#define ADEPT_DEFINE_GBMV(T, FUNC, FUNC_COMPLEX)		\
    void cppblas_gbmv(const BLAS_ORDER Order,			\
		      const BLAS_TRANSPOSE TransA,		\
		      const int M, const int N,			\
		      const int KL, const int KU, const T alpha,\
		      const T *A, const int lda, const T *X,	\
		      const int incX, const T beta, T *Y,	\
		      const int incY) {				\
      throw feature_not_available("Cannot perform band matrix-vector multiplication because compiled without BLAS"); \
    }
    ADEPT_DEFINE_GBMV(double, dgbmv_, zgbmv_)
    ADEPT_DEFINE_GBMV(float,  sgbmv_, cgbmv_)
#undef ADEPT_DEFINE_GBMV

  }
}

#endif


// =================================================================
// Contents of index.cpp
// =================================================================

/* index.cpp -- Definitions of "end" and "__" for array indexing

    Copyright (C) 2015 European Centre for Medium-Range Weather Forecasts

    Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.
*/

#include <adept/RangeIndex.h>

namespace adept {

  ::adept::internal::EndIndex end;
  ::adept::internal::AllIndex __;

}


// =================================================================
// Contents of inv.cpp
// =================================================================

/* inv.cpp -- Invert matrices

    Copyright (C) 2015-2016 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.
*/
                             
#include <vector>

#include <adept/Array.h>
#include <adept/SpecialMatrix.h>

#ifndef AdeptSource_H
#include "cpplapack.h"
#endif

#ifdef HAVE_LAPACK

namespace adept {

  using namespace internal;
  
  // -------------------------------------------------------------------
  // Invert general square matrix A
  // -------------------------------------------------------------------
  template <typename Type>
  Array<2,Type,false> 
  inv(const Array<2,Type,false>& A) {
    using internal::cpplapack_getrf;
    using internal::cpplapack_getri;

    if (A.dimension(0) != A.dimension(1)) {
      throw invalid_operation("Only square matrices can be inverted"
			      ADEPT_EXCEPTION_LOCATION);
    }

    Array<2,Type,false> A_;

    // LAPACKE is more efficient with column-major input
    A_.resize_column_major(A.dimensions());
    A_ = A;

    std::vector<lapack_int> ipiv(A_.dimension(0));

    //    lapack_int status = LAPACKE_dgetrf(LAPACK_COL_MAJOR, A_.dimension(0), A_.dimension(1),
    //				       A_.data(), A_.offset(1), &ipiv[0]);

    lapack_int status = cpplapack_getrf(A_.dimension(0),
					A_.data(), A_.offset(1), &ipiv[0]);
    if (status != 0) {
      std::stringstream s;
      s << "Failed to factorize matrix: LAPACK ?getrf returned code " << status;
      throw(matrix_ill_conditioned(s.str() ADEPT_EXCEPTION_LOCATION));
    }

    //    status = LAPACKE_dgetri(LAPACK_COL_MAJOR, A_.dimension(0),
    //			    A_.data(), A_.offset(1), &ipiv[0]);
    status = cpplapack_getri(A_.dimension(0),
			     A_.data(), A_.offset(1), &ipiv[0]);

    if (status != 0) {
      std::stringstream s;
      s << "Failed to invert matrix: LAPACK ?getri returned code " << status;
      throw(matrix_ill_conditioned(s.str() ADEPT_EXCEPTION_LOCATION));
    }
    return A_;
  }



  // -------------------------------------------------------------------
  // Invert symmetric matrix A
  // -------------------------------------------------------------------
  template <typename Type, SymmMatrixOrientation Orient>
  SpecialMatrix<Type,SymmEngine<Orient>,false> 
  inv(const SpecialMatrix<Type,SymmEngine<Orient>,false>& A) {
    using internal::cpplapack_sytrf;
    using internal::cpplapack_sytri;

    SpecialMatrix<Type,SymmEngine<Orient>,false> A_;

    A_.resize(A.dimension());
    A_ = A;

    // Treat symmetric matrix as column-major
    char uplo;
    if (Orient == ROW_LOWER_COL_UPPER) {
      uplo = 'U';
    }
    else {
      uplo = 'L';
    }

    std::vector<lapack_int> ipiv(A_.dimension(0));

    //    lapack_int status = LAPACKE_dsytrf(LAPACK_COL_MAJOR, uplo, A_.dimension(),
    //				       A_.data(), A_.offset(), &ipiv[0]);
    lapack_int status = cpplapack_sytrf(uplo, A_.dimension(),
					A_.data(), A_.offset(), &ipiv[0]);
    if (status != 0) {
      std::stringstream s;
      s << "Failed to factorize symmetric matrix: LAPACK ?sytrf returned code " << status;
      throw(matrix_ill_conditioned(s.str() ADEPT_EXCEPTION_LOCATION));
    }

    //    status = LAPACKE_dsytri(LAPACK_COL_MAJOR, uplo, A_.dimension(),
    //			    A_.data(), A_.offset(), &ipiv[0]);
    status = cpplapack_sytri(uplo, A_.dimension(),
			     A_.data(), A_.offset(), &ipiv[0]);
    if (status != 0) {
      std::stringstream s;
      s << "Failed to invert symmetric matrix: LAPACK ?sytri returned code " << status;
      throw(matrix_ill_conditioned(s.str() ADEPT_EXCEPTION_LOCATION));
    }
    return A_;
  }

}

#else // LAPACK not available
    
namespace adept {

  using namespace internal;

  // -------------------------------------------------------------------
  // Invert general square matrix A
  // -------------------------------------------------------------------
  template <typename Type>
  Array<2,Type,false> 
  inv(const Array<2,Type,false>& A) {
    throw feature_not_available("Cannot invert matrix because compiled without LAPACK");
  }

  // -------------------------------------------------------------------
  // Invert symmetric matrix A
  // -------------------------------------------------------------------
  template <typename Type, SymmMatrixOrientation Orient>
  SpecialMatrix<Type,SymmEngine<Orient>,false> 
  inv(const SpecialMatrix<Type,SymmEngine<Orient>,false>& A) {
    throw feature_not_available("Cannot invert matrix because compiled without LAPACK");
  }
  
}

#endif

namespace adept {
  // -------------------------------------------------------------------
  // Explicit instantiations
  // -------------------------------------------------------------------
#define ADEPT_EXPLICIT_INV(TYPE)					\
  template Array<2,TYPE,false>						\
  inv(const Array<2,TYPE,false>& A);					\
  template SpecialMatrix<TYPE,SymmEngine<ROW_LOWER_COL_UPPER>,false>	\
  inv(const SpecialMatrix<TYPE,SymmEngine<ROW_LOWER_COL_UPPER>,false>&); \
  template SpecialMatrix<TYPE,SymmEngine<ROW_UPPER_COL_LOWER>,false>	\
  inv(const SpecialMatrix<TYPE,SymmEngine<ROW_UPPER_COL_LOWER>,false>&)

  ADEPT_EXPLICIT_INV(float);
  ADEPT_EXPLICIT_INV(double);

#undef ADEPT_EXPLICIT_INV
  
}




// =================================================================
// Contents of jacobian.cpp
// =================================================================

/* jacobian.cpp -- Computation of Jacobian matrix

    Copyright (C) 2012-2014 University of Reading
    Copyright (C) 2015-2020 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/

#ifdef _OPENMP
#include <omp.h>
#endif

#include <adept_arrays.h>

namespace adept {

  namespace internal {
    static const int MULTIPASS_SIZE = ADEPT_REAL_PACKET_SIZE == 1 ? ADEPT_MULTIPASS_SIZE : ADEPT_REAL_PACKET_SIZE;
  }

  using namespace internal;

  template <typename T>
  T _check_long_double() {
    // The user may have requested Real to be of type "long double" by
    // specifying ADEPT_REAL_TYPE_SIZE=16. If the present system can
    // only support double then sizeof(long double) will be 8, but
    // Adept will not be emitting the best code for this, so it is
    // probably better to fail forcing the user to specify
    // ADEPT_REAL_TYPE_SIZE=8.
    ADEPT_STATIC_ASSERT(ADEPT_REAL_TYPE_SIZE != 16 || ADEPT_REAL_TYPE_SIZE == sizeof(Real),
			COMPILER_DOES_NOT_SUPPORT_16_BYTE_LONG_DOUBLE);
    return 1;
  }

#if ADEPT_REAL_PACKET_SIZE > 1
  void
  Stack::jacobian_forward_kernel(Real* __restrict gradient_multipass_b) const
  {

    // Loop forward through the derivative statements
    for (uIndex ist = 1; ist < n_statements_; ist++) {
      const Statement& statement = statement_[ist];
      // We copy the LHS to "a" in case it appears on the RHS in any
      // of the following statements
      Packet<Real> a; // Zeroed automatically
      // Loop through operations
      for (uIndex iop = statement_[ist-1].end_plus_one;
	   iop < statement.end_plus_one; iop++) {
	Packet<Real> g(gradient_multipass_b+index_[iop]*MULTIPASS_SIZE);
	Packet<Real> m(multiplier_[iop]);
	a += m * g;
      }
      // Copy the results
      a.put(gradient_multipass_b+statement.index*MULTIPASS_SIZE);
    } // End of loop over statements
  }    
#else
  void
  Stack::jacobian_forward_kernel(Real* __restrict gradient_multipass_b) const
  {

    // Loop forward through the derivative statements
    for (uIndex ist = 1; ist < n_statements_; ist++) {
      const Statement& statement = statement_[ist];
      // We copy the LHS to "a" in case it appears on the RHS in any
      // of the following statements
      Block<MULTIPASS_SIZE,Real> a; // Zeroed automatically
      // Loop through operations
      for (uIndex iop = statement_[ist-1].end_plus_one;
	   iop < statement.end_plus_one; iop++) {
	for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	  a[i] += multiplier_[iop]*gradient_multipass_b[index_[iop]*MULTIPASS_SIZE+i];
	}
      }
      // Copy the results
      for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	gradient_multipass_b[statement.index*MULTIPASS_SIZE+i] = a[i];
      }
    } // End of loop over statements
  }    
#endif

  void
  Stack::jacobian_forward_kernel_extra(Real* __restrict gradient_multipass_b,
				       uIndex n_extra) const
  {

    // Loop forward through the derivative statements
    for (uIndex ist = 1; ist < n_statements_; ist++) {
      const Statement& statement = statement_[ist];
      // We copy the LHS to "a" in case it appears on the RHS in any
      // of the following statements
      Block<MULTIPASS_SIZE,Real> a; // Zeroed automatically
      // Loop through operations
      for (uIndex iop = statement_[ist-1].end_plus_one;
	   iop < statement.end_plus_one; iop++) {
	for (uIndex i = 0; i < n_extra; i++) {
	  a[i] += multiplier_[iop]*gradient_multipass_b[index_[iop]*MULTIPASS_SIZE+i];
	}
      }
      // Copy the results
      for (uIndex i = 0; i < n_extra; i++) {
	gradient_multipass_b[statement.index*MULTIPASS_SIZE+i] = a[i];
      }
    } // End of loop over statements
  }    



  // Compute the Jacobian matrix, parallelized using OpenMP. Normally
  // the user would call the jacobian or jacobian_forward functions,
  // and the OpenMP version would only be called if OpenMP is
  // available and the Jacobian matrix is large enough for
  // parallelization to be worthwhile.  Note that jacobian_out must be
  // allocated to be at least of size m*n, where m is the number of
  // dependent variables and n is the number of independents. The
  // independents and dependents must have already been identified
  // with the functions "independent" and "dependent", otherwise this
  // function will fail with FAILURE_XXDEPENDENT_NOT_IDENTIFIED. The
  // offsets in memory of the two dimensions are provided by
  // dep_offset and indep_offset. This is implemented using a forward
  // pass, appropriate for m>=n.
  void
  Stack::jacobian_forward_openmp(Real* jacobian_out,
				 Index dep_offset, Index indep_offset) const
  {

    // Number of blocks to cycle through, including a possible last
    // block containing fewer than MULTIPASS_SIZE variables
    int n_block = (n_independent() + MULTIPASS_SIZE - 1)
      / MULTIPASS_SIZE;
    uIndex n_extra = n_independent() % MULTIPASS_SIZE;
    
#pragma omp parallel
    {
      //      std::vector<Block<MULTIPASS_SIZE,Real> > 
      //	gradient_multipass_b(max_gradient_);
      uIndex gradient_multipass_size = max_gradient_*MULTIPASS_SIZE;
      Real* __restrict gradient_multipass_b 
	= alloc_aligned<Real>(gradient_multipass_size);
      
#pragma omp for schedule(static)
      for (int iblock = 0; iblock < n_block; iblock++) {
	// Set the index to the dependent variables for this block
	uIndex i_independent =  MULTIPASS_SIZE * iblock;
	
	uIndex block_size = MULTIPASS_SIZE;
	// If this is the last iteration and the number of extra
	// elements is non-zero, then set the block size to the number
	// of extra elements. If the number of extra elements is zero,
	// then the number of independent variables is exactly divisible
	// by MULTIPASS_SIZE, so the last iteration will be the
	// same as all the rest.
	if (iblock == n_block-1 && n_extra > 0) {
	  block_size = n_extra;
	}
	
	// Set the initial gradients all to zero
	for (uIndex i = 0; i < gradient_multipass_size; i++) {
	  gradient_multipass_b[i] = 0.0;
	}
	// Each seed vector has one non-zero entry of 1.0
	for (uIndex i = 0; i < block_size; i++) {
	  gradient_multipass_b[independent_index_[i_independent+i]*MULTIPASS_SIZE+i] = 1.0;
	}

	jacobian_forward_kernel(gradient_multipass_b);

	// Copy the gradients corresponding to the dependent variables
	// into the Jacobian matrix
	if (indep_offset == 1) {
	  for (uIndex idep = 0; idep < n_dependent(); idep++) {
	    for (uIndex i = 0; i < block_size; i++) {
	      jacobian_out[idep*dep_offset+i_independent+i]
		= gradient_multipass_b[dependent_index_[idep]*MULTIPASS_SIZE+i];
	    }
	  }
	}
	else {
	  for (uIndex idep = 0; idep < n_dependent(); idep++) {
	    for (uIndex i = 0; i < block_size; i++) {
	      jacobian_out[(i_independent+i)*indep_offset+idep*dep_offset]
		= gradient_multipass_b[dependent_index_[idep]*MULTIPASS_SIZE+i];
	    }
	  }
	}
      } // End of loop over blocks
      free_aligned(gradient_multipass_b);
    } // End of parallel section
  } // End of jacobian function


  // Compute the Jacobian matrix; note that jacobian_out must be
  // allocated to be of size m*n, where m is the number of dependent
  // variables and n is the number of independents. The independents
  // and dependents must have already been identified with the
  // functions "independent" and "dependent", otherwise this function
  // will fail with FAILURE_XXDEPENDENT_NOT_IDENTIFIED. This is
  // implemented using a forward pass, appropriate for m>=n.
  void
  Stack::jacobian_forward(Real* jacobian_out,
			  Index dep_offset, Index indep_offset) const
  {
    if (independent_index_.empty() || dependent_index_.empty()) {
      throw(dependents_or_independents_not_identified());
    }

    // If either of the offsets are zero, set them to the size of the
    // other dimension, which assumes that the full Jacobian matrix is
    // contiguous in memory.
    if (dep_offset <= 0) {
      dep_offset = n_independent();
    }
    if (indep_offset <= 0) {
      indep_offset = n_dependent();
    }

#ifdef _OPENMP
    if (have_openmp_ 
	&& !openmp_manually_disabled_
	&& n_independent() > MULTIPASS_SIZE
	&& omp_get_max_threads() > 1) {
      // Call the parallel version
      jacobian_forward_openmp(jacobian_out, dep_offset, indep_offset);
      return;
    }
#endif

    // For optimization reasons, we process a block of
    // MULTIPASS_SIZE columns of the Jacobian at once; calculate
    // how many blocks are needed and how many extras will remain
    uIndex n_block = n_independent() / MULTIPASS_SIZE;
    uIndex n_extra = n_independent() % MULTIPASS_SIZE;

    ///gradient_multipass_.resize(max_gradient_);
    uIndex gradient_multipass_size = max_gradient_*MULTIPASS_SIZE;
    Real* __restrict gradient_multipass_b 
      = alloc_aligned<Real>(gradient_multipass_size);

    // Loop over blocks of MULTIPASS_SIZE columns
    for (uIndex iblock = 0; iblock < n_block; iblock++) {
      // Set the index to the dependent variables for this block
      uIndex i_independent =  MULTIPASS_SIZE * iblock;

      // Set the initial gradients all to zero
      ///zero_gradient_multipass();
      for (uIndex i = 0; i < gradient_multipass_size; i++) {
	gradient_multipass_b[i] = 0.0;
      }

      // Each seed vector has one non-zero entry of 1.0
      for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	gradient_multipass_b[independent_index_[i_independent+i]*MULTIPASS_SIZE+i] = 1.0;
      }

      jacobian_forward_kernel(gradient_multipass_b);

      // Copy the gradients corresponding to the dependent variables
      // into the Jacobian matrix
      if (indep_offset == 1) {
	for (uIndex idep = 0; idep < n_dependent(); idep++) {
	  for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	    jacobian_out[idep*dep_offset+i_independent+i]
	      = gradient_multipass_b[dependent_index_[idep]*MULTIPASS_SIZE+i];
	  }
	}
      }
      else {
	for (uIndex idep = 0; idep < n_dependent(); idep++) {
	  for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	    jacobian_out[(i_independent+i)*indep_offset+idep*dep_offset] 
	      = gradient_multipass_b[dependent_index_[idep]*MULTIPASS_SIZE+i];
	  }
	}
      }
    } // End of loop over blocks
    
    // Now do the same but for the remaining few columns in the matrix
    if (n_extra > 0) {
      uIndex i_independent =  MULTIPASS_SIZE * n_block;
      ///zero_gradient_multipass();
      for (uIndex i = 0; i < gradient_multipass_size; i++) {
	gradient_multipass_b[i] = 0.0;
      }

      for (uIndex i = 0; i < n_extra; i++) {
	gradient_multipass_b[independent_index_[i_independent+i]*MULTIPASS_SIZE+i] = 1.0;
      }

      jacobian_forward_kernel_extra(gradient_multipass_b, n_extra);

      if (indep_offset == 1) {
	for (uIndex idep = 0; idep < n_dependent(); idep++) {
	  for (uIndex i = 0; i < n_extra; i++) {
	    jacobian_out[idep*dep_offset+i_independent+i]
	      = gradient_multipass_b[dependent_index_[idep]*MULTIPASS_SIZE+i];
	  }
	}
      }
      else {
	for (uIndex idep = 0; idep < n_dependent(); idep++) {
	  for (uIndex i = 0; i < n_extra; i++) {
	    jacobian_out[(i_independent+i)*indep_offset+idep*dep_offset] 
	      = gradient_multipass_b[dependent_index_[idep]*MULTIPASS_SIZE+i];
	  }
	}
      }
    }

    free_aligned(gradient_multipass_b);
  }


  // Compute the Jacobian matrix, parallelized using OpenMP.  Normally
  // the user would call the jacobian or jacobian_reverse functions,
  // and the OpenMP version would only be called if OpenMP is
  // available and the Jacobian matrix is large enough for
  // parallelization to be worthwhile.  Note that jacobian_out must be
  // allocated to be at least of size m*n, where m is the number of
  // dependent variables and n is the number of independents. The
  // independents and dependents must have already been identified
  // with the functions "independent" and "dependent", otherwise this
  // function will fail with FAILURE_XXDEPENDENT_NOT_IDENTIFIED. The
  // offsets in memory of the two dimensions are provided by
  // dep_offset and indep_offset.  This is implemented using a reverse
  // pass, appropriate for m<n.
  void
  Stack::jacobian_reverse_openmp(Real* jacobian_out,
				 Index dep_offset, Index indep_offset) const
  {

    // Number of blocks to cycle through, including a possible last
    // block containing fewer than MULTIPASS_SIZE variables
    int n_block = (n_dependent() + MULTIPASS_SIZE - 1)
      / MULTIPASS_SIZE;
    uIndex n_extra = n_dependent() % MULTIPASS_SIZE;
    
    // Inside the OpenMP loop, the "this" pointer may be NULL if the
    // adept::Stack pointer is declared as thread-local and if the
    // OpenMP memory model uses thread-local storage for private
    // data. If this is the case then local pointers to or copies of
    // the following members of the adept::Stack object may need to be
    // made: dependent_index_ n_statements_ statement_ multiplier_
    // index_ independent_index_ n_dependent() n_independent().
    // Limited testing implies this is OK though.

#pragma omp parallel
    {
      std::vector<Block<MULTIPASS_SIZE,Real> > 
	gradient_multipass_b(max_gradient_);
      
#pragma omp for schedule(static)
      for (int iblock = 0; iblock < n_block; iblock++) {
	// Set the index to the dependent variables for this block
	uIndex i_dependent =  MULTIPASS_SIZE * iblock;
	
	uIndex block_size = MULTIPASS_SIZE;
	// If this is the last iteration and the number of extra
	// elements is non-zero, then set the block size to the number
	// of extra elements. If the number of extra elements is zero,
	// then the number of independent variables is exactly divisible
	// by MULTIPASS_SIZE, so the last iteration will be the
	// same as all the rest.
	if (iblock == n_block-1 && n_extra > 0) {
	  block_size = n_extra;
	}

	// Set the initial gradients all to zero
	for (std::size_t i = 0; i < gradient_multipass_b.size(); i++) {
	  gradient_multipass_b[i].zero();
	}
	// Each seed vector has one non-zero entry of 1.0
	for (uIndex i = 0; i < block_size; i++) {
	  gradient_multipass_b[dependent_index_[i_dependent+i]][i] = 1.0;
	}

	// Loop backward through the derivative statements
	for (uIndex ist = n_statements_-1; ist > 0; ist--) {
	  const Statement& statement = statement_[ist];
	  // We copy the RHS to "a" in case it appears on the LHS in any
	  // of the following statements
	  Real a[MULTIPASS_SIZE];
#if MULTIPASS_SIZE > MULTIPASS_SIZE_ZERO_CHECK
	  // For large blocks, we only process the ones where a[i] is
	  // non-zero
	  uIndex i_non_zero[MULTIPASS_SIZE];
#endif
	  uIndex n_non_zero = 0;
	  for (uIndex i = 0; i < block_size; i++) {
	    a[i] = gradient_multipass_b[statement.index][i];
	    gradient_multipass_b[statement.index][i] = 0.0;
	    if (a[i] != 0.0) {
#if MULTIPASS_SIZE > MULTIPASS_SIZE_ZERO_CHECK
	      i_non_zero[n_non_zero++] = i;
#else
	      n_non_zero = 1;
#endif
	    }
	  }

	  // Only do anything for this statement if any of the a values
	  // are non-zero
	  if (n_non_zero) {
	    // Loop through the operations
	    for (uIndex iop = statement_[ist-1].end_plus_one;
		 iop < statement.end_plus_one; iop++) {
	      // Try to minimize pointer dereferencing by making local
	      // copies
	      Real multiplier = multiplier_[iop];
	      Real* __restrict gradient_multipass 
		= &(gradient_multipass_b[index_[iop]][0]);
#if MULTIPASS_SIZE > MULTIPASS_SIZE_ZERO_CHECK
	      // For large blocks, loop over only the indices
	      // corresponding to non-zero a
	      for (uIndex i = 0; i < n_non_zero; i++) {
		gradient_multipass[i_non_zero[i]] += multiplier*a[i_non_zero[i]];
	      }
#else
	      // For small blocks, do all indices
	      for (uIndex i = 0; i < block_size; i++) {
	      //	      for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
		gradient_multipass[i] += multiplier*a[i];
	      }
#endif
	    }
	  }
	} // End of loop over statement
	// Copy the gradients corresponding to the independent
	// variables into the Jacobian matrix
	if (dep_offset == 1) {
	  for (uIndex iindep = 0; iindep < n_independent(); iindep++) {
	    for (uIndex i = 0; i < block_size; i++) {
	      jacobian_out[iindep*indep_offset+i_dependent+i] 
		= gradient_multipass_b[independent_index_[iindep]][i];
	    }
	  }
	}
	else {
	  for (uIndex iindep = 0; iindep < n_independent(); iindep++) {
	    for (uIndex i = 0; i < block_size; i++) {
	      jacobian_out[iindep*indep_offset+(i_dependent+i)*dep_offset] 
		= gradient_multipass_b[independent_index_[iindep]][i];
	    }
	  }
	}
      } // End of loop over blocks
    } // end #pragma omp parallel
  } // end jacobian_reverse_openmp


  // Compute the Jacobian matrix; note that jacobian_out must be
  // allocated to be of size m*n, where m is the number of dependent
  // variables and n is the number of independents. The independents
  // and dependents must have already been identified with the
  // functions "independent" and "dependent", otherwise this function
  // will fail with FAILURE_XXDEPENDENT_NOT_IDENTIFIED.  This is
  // implemented using a reverse pass, appropriate for m<n.
  void
  Stack::jacobian_reverse(Real* jacobian_out,
			  Index dep_offset, Index indep_offset) const
  {
    if (independent_index_.empty() || dependent_index_.empty()) {
      throw(dependents_or_independents_not_identified());
    }

    // If either of the offsets are zero, set them to the size of the
    // other dimension, which assumes that the full Jacobian matrix is
    // contiguous in memory.
    if (dep_offset <= 0) {
      dep_offset = n_independent();
    }
    if (indep_offset <= 0) {
      indep_offset = n_dependent();
    }

#ifdef _OPENMP
    if (have_openmp_ 
	&& !openmp_manually_disabled_
	&& n_dependent() > MULTIPASS_SIZE
	&& omp_get_max_threads() > 1) {
      // Call the parallel version
      jacobian_reverse_openmp(jacobian_out,
			      dep_offset, indep_offset);
      return;
    }
#endif

    //    gradient_multipass_.resize(max_gradient_);
    std::vector<Block<MULTIPASS_SIZE,Real> > 
      gradient_multipass_b(max_gradient_);

    // For optimization reasons, we process a block of
    // MULTIPASS_SIZE rows of the Jacobian at once; calculate
    // how many blocks are needed and how many extras will remain
    uIndex n_block = n_dependent() / MULTIPASS_SIZE;
    uIndex n_extra = n_dependent() % MULTIPASS_SIZE;
    uIndex i_dependent = 0; // uIndex of first row in the block we are
			    // currently computing
    // Loop over the of MULTIPASS_SIZE rows
    for (uIndex iblock = 0; iblock < n_block; iblock++) {
      // Set the initial gradients all to zero
      //      zero_gradient_multipass();
      for (std::size_t i = 0; i < gradient_multipass_b.size(); i++) {
	gradient_multipass_b[i].zero();
      }

      // Each seed vector has one non-zero entry of 1.0
      for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	gradient_multipass_b[dependent_index_[i_dependent+i]][i] = 1.0;
      }
      // Loop backward through the derivative statements
      for (uIndex ist = n_statements_-1; ist > 0; ist--) {
	const Statement& statement = statement_[ist];
	// We copy the RHS to "a" in case it appears on the LHS in any
	// of the following statements
	Real a[MULTIPASS_SIZE];
#if MULTIPASS_SIZE > MULTIPASS_SIZE_ZERO_CHECK
	// For large blocks, we only process the ones where a[i] is
	// non-zero
	uIndex i_non_zero[MULTIPASS_SIZE];
#endif
	uIndex n_non_zero = 0;
	for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	  a[i] = gradient_multipass_b[statement.index][i];
	  gradient_multipass_b[statement.index][i] = 0.0;
	  if (a[i] != 0.0) {
#if MULTIPASS_SIZE > MULTIPASS_SIZE_ZERO_CHECK
	    i_non_zero[n_non_zero++] = i;
#else
	    n_non_zero = 1;
#endif
	  }
	}
	// Only do anything for this statement if any of the a values
	// are non-zero
	if (n_non_zero) {
	  // Loop through the operations
	  for (uIndex iop = statement_[ist-1].end_plus_one;
	       iop < statement.end_plus_one; iop++) {
	    // Try to minimize pointer dereferencing by making local
	    // copies
	    Real multiplier = multiplier_[iop];
	    Real* __restrict gradient_multipass 
	      = &(gradient_multipass_b[index_[iop]][0]);
#if MULTIPASS_SIZE > MULTIPASS_SIZE_ZERO_CHECK
	    // For large blocks, loop over only the indices
	    // corresponding to non-zero a
	    for (uIndex i = 0; i < n_non_zero; i++) {
	      gradient_multipass[i_non_zero[i]] += multiplier*a[i_non_zero[i]];
	    }
#else
	    // For small blocks, do all indices
	    for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	      gradient_multipass[i] += multiplier*a[i];
	    }
#endif
	  }
	}
      } // End of loop over statement
      // Copy the gradients corresponding to the independent variables
      // into the Jacobian matrix
      if (dep_offset == 1) {
	for (uIndex iindep = 0; iindep < n_independent(); iindep++) {
	  for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	    jacobian_out[iindep*indep_offset+i_dependent+i] 
	      = gradient_multipass_b[independent_index_[iindep]][i];
	  }
	}
      }
      else {
	for (uIndex iindep = 0; iindep < n_independent(); iindep++) {
	  for (uIndex i = 0; i < MULTIPASS_SIZE; i++) {
	    jacobian_out[iindep*indep_offset+(i_dependent+i)*dep_offset] 
	      = gradient_multipass_b[independent_index_[iindep]][i];
	  }
	}
      }
      i_dependent += MULTIPASS_SIZE;
    } // End of loop over blocks
    
    // Now do the same but for the remaining few rows in the matrix
    if (n_extra > 0) {
      for (std::size_t i = 0; i < gradient_multipass_b.size(); i++) {
	gradient_multipass_b[i].zero();
      }
      //      zero_gradient_multipass();
      for (uIndex i = 0; i < n_extra; i++) {
	gradient_multipass_b[dependent_index_[i_dependent+i]][i] = 1.0;
      }
      for (uIndex ist = n_statements_-1; ist > 0; ist--) {
	const Statement& statement = statement_[ist];
	Real a[MULTIPASS_SIZE];
#if MULTIPASS_SIZE > MULTIPASS_SIZE_ZERO_CHECK
	uIndex i_non_zero[MULTIPASS_SIZE];
#endif
	uIndex n_non_zero = 0;
	for (uIndex i = 0; i < n_extra; i++) {
	  a[i] = gradient_multipass_b[statement.index][i];
	  gradient_multipass_b[statement.index][i] = 0.0;
	  if (a[i] != 0.0) {
#if MULTIPASS_SIZE > MULTIPASS_SIZE_ZERO_CHECK
	    i_non_zero[n_non_zero++] = i;
#else
	    n_non_zero = 1;
#endif
	  }
	}
	if (n_non_zero) {
	  for (uIndex iop = statement_[ist-1].end_plus_one;
	       iop < statement.end_plus_one; iop++) {
	    Real multiplier = multiplier_[iop];
	    Real* __restrict gradient_multipass 
	      = &(gradient_multipass_b[index_[iop]][0]);
#if MULTIPASS_SIZE > MULTIPASS_SIZE_ZERO_CHECK
	    for (uIndex i = 0; i < n_non_zero; i++) {
	      gradient_multipass[i_non_zero[i]] += multiplier*a[i_non_zero[i]];
	    }
#else
	    for (uIndex i = 0; i < n_extra; i++) {
	      gradient_multipass[i] += multiplier*a[i];
	    }
#endif
	  }
	}
      }
      if (dep_offset == 1) {
	for (uIndex iindep = 0; iindep < n_independent(); iindep++) {
	  for (uIndex i = 0; i < n_extra; i++) {
	    jacobian_out[iindep*indep_offset+i_dependent+i] 
	      = gradient_multipass_b[independent_index_[iindep]][i];
	  }
	}
      }
      else {
	for (uIndex iindep = 0; iindep < n_independent(); iindep++) {
	  for (uIndex i = 0; i < n_extra; i++) {
	    jacobian_out[iindep*indep_offset+(i_dependent+i)*dep_offset] 
	      = gradient_multipass_b[independent_index_[iindep]][i];
	  }
	}
      }
    }
  }
  
  // Return the Jacobian matrix in the matrix "jac", using the forward
  // or reverse method depending which would be faster
  void Stack::jacobian(Array<2,Real,false> jac) const {
    if (jac.dimension(0) != n_dependent()
	|| jac.dimension(1) != n_independent()) {
      throw size_mismatch("Jacobian matrix has wrong size");
    }
    if (n_independent() <= n_dependent()) {
      jacobian_forward(jac.data(), jac.offset(0), jac.offset(1));
    }
    else {
      jacobian_reverse(jac.data(), jac.offset(0), jac.offset(1));
    }
  }

  // Return the Jacobian matrix in the matrix "jac", explicitly
  // specifying whether to use the forward or reverse method
  void Stack::jacobian_forward(Array<2,Real,false> jac) const {
    if (jac.dimension(0) != n_dependent()
	|| jac.dimension(1) != n_independent()) {
      throw size_mismatch("Jacobian matrix has wrong size");
    }
    jacobian_forward(jac.data(), jac.offset(0), jac.offset(1));
  }

  void Stack::jacobian_reverse(Array<2,Real,false> jac) const {
    if (jac.dimension(0) != n_dependent()
	|| jac.dimension(1) != n_independent()) {
      throw size_mismatch("Jacobian matrix has wrong size");
    }
    jacobian_reverse(jac.data(), jac.offset(0), jac.offset(1));
  }

  // Return the Jacobian matrix using the forward or reverse method
  // depending which would be faster
  Array<2,Real,false> Stack::jacobian() const {
    Array<2,Real,false> jac(n_dependent(), n_independent());
    if (n_independent() <= n_dependent()) {
      jacobian_forward(jac.data(), jac.offset(0), jac.offset(1));
    }
    else {
      jacobian_reverse(jac.data(), jac.offset(0), jac.offset(1));
    }
    return jac;
  }

  // Return the Jacobian matrix, explicitly specifying whether to use
  // the forward or reverse method
  Array<2,Real,false> Stack::jacobian_forward() const {
    Array<2,Real,false> jac(n_dependent(), n_independent());
    jacobian_forward(jac.data(), jac.offset(0), jac.offset(1));
    return jac;
  }

  Array<2,Real,false> Stack::jacobian_reverse() const {
    Array<2,Real,false> jac(n_dependent(), n_independent());
    jacobian_reverse(jac.data(), jac.offset(0), jac.offset(1));
    return jac;
  }

} // End namespace adept


// =================================================================
// Contents of line_search.cpp
// =================================================================

/* line_search.cpp -- Approximate minimization of function along a line

    Copyright (C) 2020 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/

#include <limits>
#include <cmath>
#include <adept/Minimizer.h>

namespace adept {

  // Compute the cost function "cf" and gradient vector "gradient",
  // along with the scalar gradient "grad" in the search direction
  // "direction" (normalized with "dir_scaling"), from the state
  // vector "x" plus a step "step_size" in the search direction. If
  // the resulting cost function and gradient satisfy the Wolfe
  // conditions for sufficient convergence, copy the new state vector
  // to "x" and the step size to "final_step_size", and return
  // MINIMIZER_STATUS_SUCCESS.  Otherwise, return
  // MINIMIZER_STATUS_NOT_YET_CONVERGED.  Error conditions
  // MINIMIZER_STATUS_INVALID_COST_FUNCTION and
  // MINIMIZER_STATUS_INVALID_GRADIENT are also possible.
  MinimizerStatus
  Minimizer::line_search_gradient_check(
	Optimizable& optimizable, // Object defining function to be minimized
	Vector x, // Initial and returned state vector
	const Vector& direction, // Un-normalized search direction
	Vector test_x, // Test state vector (working memory)
	Real& final_step_size, // Returned step size if converged
	Vector gradient, // Gradient vector
	int& state_up_to_date, // Is state up-to-date?
	Real step_size, // Candidate step size
	Real grad0, // Gradient in direction at start of line search
	Real dir_scaling, // Scaling of direction vector
	Real& cf, // Returned cost function
	Real& grad, // Returned gradient in direction
	Real curvature_coeff) // Factor by which gradient should reduce (0-1)
  {
    test_x = x + (step_size * dir_scaling) * direction;
    cf = optimizable.calc_cost_function_gradient(test_x, gradient);
    ++n_samples_;
    state_up_to_date = -1;

    // Check cost function and gradient are finite
    if (!std::isfinite(cf)) {
      return MINIMIZER_STATUS_INVALID_COST_FUNCTION;
    }
    else if (any(!isfinite(gradient))) {
      return MINIMIZER_STATUS_INVALID_GRADIENT;
    }

    // Calculate gradient in search direction
    grad = dot_product(direction, gradient) * dir_scaling;

    // Check Wolfe conditions
    if (cf <= cost_function_ + armijo_coeff_*step_size*grad0 // Armijo condition
	&& std::fabs(grad) <= -curvature_coeff*grad0) { // Curvature condition
      x = test_x;
      final_step_size = step_size;
      cost_function_ = cf;
      state_up_to_date = 1;
      return MINIMIZER_STATUS_SUCCESS;
    }
    else {
      return MINIMIZER_STATUS_NOT_YET_CONVERGED;
    }
  }

  // Perform line search starting at state vector "x" with gradient
  // vector "gradient", and initial step "step_size" in un-normalized
  // direction "direction". Successful minimization of the function
  // (according to Wolfe conditions) will lead to
  // MINIMIZER_STATUS_SUCCESS being returned, the new state stored in
  // "x", and if state_up_to_date >= 1 then the gradient stored in
  // "gradient". Other possible return values are
  // MINIMIZER_STATUS_FAILED_TO_CONVERGE and
  // MINIMIZER_STATUS_DIRECTION_UPHILL if the initial direction points
  // uphill, or MINIMIZER_STATUS_INVALID_COST_FUNCTION,
  // MINIMIZER_STATUS_INVALID_GRADIENT or
  // MINIMIZER_STATUS_BOUND_REACHED. First the minimum is bracketed,
  // then a cubic polynomial is fitted to the values and gradients of
  // the function at the two points in order to select the next test
  // point.
  MinimizerStatus
  Minimizer::line_search(
	 Optimizable& optimizable,  // Object defining function to be minimized
	 Vector x, // Initial and returned state vector
	 const Vector& direction, // Un-normalized search direction
	 Vector test_x, // Test state vector (working memory)
	 Real& step_size, // Initial and final step size
	 Vector gradient, // Initial and possibly final gradient
	 int& state_up_to_date, // 1 if gradient up-to-date, -1 otherwise
	 Real curvature_coeff, // Factor by which gradient should reduce (0-1)
	 Real bound_step_size) // Maximum step until bound is reached (-1 for no bound)
  {
    Real dir_scaling = 1.0 / norm2(direction);

    // Numerical suffixes to variables indicate different locations
    // along the line:
    // 0 = initial point of line search, constant within this function
    // 1 = point at which gradient has been calculated (initially the same as 0)
    // 2 = test point
    // 3 = test point

    // Step sizes
    const Real ss0 = 0.0;
    Real ss1 = ss0;
    Real ss2 = step_size;
    Real ss3;

    // Gradients in search direction
    Real grad0 = dot_product(direction, gradient) * dir_scaling;
    Real grad1 = grad0;
    Real grad2, grad3;

    // Cost function values
    Real cf0 = cost_function_;
    Real cf1 = cf0;
    Real cf2, cf3;

    int iterations_remaining = max_line_search_iterations_;

    bool is_bound_step = (bound_step_size > 0.0);
    bool at_bound = false;

    if (grad0 >= 0.0) {
      return MINIMIZER_STATUS_DIRECTION_UPHILL;
    }

    // Check initial step size is within bounds
    if (max_step_size_ > 0.0 && ss2 > max_step_size_) {
      ss2 = max_step_size_;
    }
    if (is_bound_step && ss2 >= bound_step_size) {
      ss2 = bound_step_size;
      at_bound = true;
    }

    // First step: bound the minimum
    while (iterations_remaining > 0) {

      MinimizerStatus status
	= line_search_gradient_check(optimizable, x, direction, test_x,
				     step_size, gradient, state_up_to_date,
				     ss2, grad0, dir_scaling,
				     cf2, grad2, curvature_coeff);
      if (status == MINIMIZER_STATUS_SUCCESS) {
	if (at_bound) {
	  status = MINIMIZER_STATUS_BOUND_REACHED;
	}
	return status;
      }
      else if (status != MINIMIZER_STATUS_NOT_YET_CONVERGED) {
	// Cost function or its gradient not finite: revert to
	// previous step
	step_size = cf1;
	if (cf1 > 0.0) {
	  x += (ss1 * dir_scaling) * direction;
	}
	state_up_to_date = 0;
	return status;
      }
     
      if (grad2 > 0.0 || cf2 >= cf1) {
	// Positive gradient or cost function increase -> bounded
	// between points 1 and 2
	break;
      }
      else if (at_bound) {
	// The cost function has been reduced but we are already at
	// the maximum step size and the gradient points towards it:
	// make this point the solution
	x += (ss2 * dir_scaling) * direction;
	step_size = ss2;
	cost_function_ = cf2;
	state_up_to_date = 1;
	return MINIMIZER_STATUS_BOUND_REACHED;
      }
      else {
	// Reduced cost function but not yet bounded -> look further
	// ahead
	Real new_step;
	if (cf1 > cf2+grad2*(ss1-ss2)) {
	  // Positive curvature: fit a quadratic
	  Real curvature = 2.0*(cf1-cf2-grad2*(ss1-ss2))/((ss1-ss2)*(ss1-ss2));
	  new_step = ss2-grad2/curvature; // Newton's method
	  // Bounds on actual step size
	  new_step = std::max(ss1+1.1*(ss2-ss1), std::min(new_step, ss1+10.0*(ss2-ss1)));
	  if (max_step_size_ > 0.0 && new_step-ss2 > max_step_size_) {
	    new_step = ss2 + max_step_size_;
	  }
	}
	else {
	  // Cliff gets steeper... simply jump ahead a lot more
	  new_step = ss2 + 5.0*(ss2-ss1);
	  if (max_step_size_ > 0.0 && new_step-ss2 > max_step_size_) {
	    new_step = ss2 + max_step_size_;
	  }
	}
	ss1 = ss2;
	cf1 = cf2;
	grad1 = grad2;
	ss2 = new_step;

	if (is_bound_step && ss2 >= bound_step_size) {
	  ss2 = bound_step_size;
	  at_bound = true;
	}
      }

    }

    // Second step: reduce the bounds until we get sufficiently close
    // to the minimum
    while (iterations_remaining > 0) {

      if (ss2 <= ss1) {
	// Two points are identical!
	if (cf1 < cf0) {
	  // Return value at point 1
	  x += (ss1 * dir_scaling) * direction;
	  step_size = ss1;
	  cost_function_ = cf1;
	  return MINIMIZER_STATUS_SUCCESS;
	}
	else {
	  // Cost function did not decrease at all
	  return MINIMIZER_STATUS_FAILED_TO_CONVERGE;
	}
      }

      // Minimizer of cubic function
      Real step_diff = ss2-ss1;
      Real theta = (cf1-cf2) * 3.0 / step_diff + grad1 + grad2;
      Real max_grad = std::max(std::fabs(theta),
			       std::max(std::fabs(grad1), std::fabs(grad2)));
      Real scaled_theta = theta / max_grad;
      Real gamma = max_grad * std::sqrt(scaled_theta*scaled_theta
					- (grad1/max_grad) * (grad2/max_grad));
      ss3 = ss1 + ((gamma - grad1 + theta) / (2.0*gamma + grad2 - grad1)) * step_diff;


      // Bound the step size to be at least 5% away from each end
      ss3 = std::max(0.95*ss1+0.05*ss2,
		     std::min(0.05*ss1+0.95*ss2, ss3));

      MinimizerStatus status
	= line_search_gradient_check(optimizable, x, direction, test_x,
				     step_size, gradient, state_up_to_date,
				     ss3, grad0, dir_scaling,
				     cf3, grad3, curvature_coeff);
      if (status == MINIMIZER_STATUS_SUCCESS) {
	return status;
      }
      else if (status != MINIMIZER_STATUS_NOT_YET_CONVERGED) {
	// Cost function or its gradient not finite: revert to
	// previous step
	step_size = cf1;
	if (cf1 > 0.0) {
	  x += (ss1 * dir_scaling) * direction;
	}
	state_up_to_date = 0;
	return status;
      }
     
      if (grad3 > 0.0) {
	// Positive gradient -> bounded between points 1 and 3
	ss2 = ss3;
	cf2 = cf3;
	grad2 = grad3;
      }
      else if (cf3 < cf1) {
	// Reduced cost function, negative gradient
	ss1 = ss3;
	cf1 = cf3;
	grad1 = grad3;
      }
      else {
	// Increased cost function, negative gradient
	ss2 = ss3;
	cf2 = cf3;
	grad2 = grad3;
      }	

      --iterations_remaining;
    }

    // Maximum iterations reached: check if cost function has been
    // reduced at all
    state_up_to_date = -1;
    if (cf2 < cf1) {
      // Return value at point 2
      x += (ss2 * dir_scaling) * direction;
      step_size = ss2;
      cost_function_ = cf2;  
    }
    else if (cf1 < cf0) {
      // Return value at point 1
      x += (ss1 * dir_scaling) * direction;
      step_size = ss1;
      cost_function_ = cf1;  
    }
    else {
      // Cost function did not decrease at all
      return MINIMIZER_STATUS_FAILED_TO_CONVERGE;
    }

    // Cost function decreased
    return MINIMIZER_STATUS_SUCCESS;

  }

}


// =================================================================
// Contents of minimize_conjugate_gradient.cpp
// =================================================================

/* minimize_conjugate_gradient.cpp -- Minimize function using Conjugate Gradient algorithm

    Copyright (C) 2020 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/

#include <limits>
#include <cmath>
#include <adept/Minimizer.h>

namespace adept {

  // Minimize the cost function embodied in "optimizable" using the
  // Conjugate-Gradient algorithm, where "x" is the initial state
  // vector and also where the solution is stored. By default the
  // Polak-Ribiere method is used to compute the new search direction,
  // but Fletcher-Reeves is also available.
  MinimizerStatus
  Minimizer::minimize_conjugate_gradient(Optimizable& optimizable, Vector x,
					 bool use_fletcher_reeves)
  {
    int nx = x.size();

    // Initial values
    n_iterations_ = 0;
    n_samples_ = 0;
    status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
    cost_function_ = std::numeric_limits<Real>::infinity();

    // The Conjugate-Gradient method is the most efficient
    // gradient-based method in terms of memory usage, requiring a
    // working memory of just 4*nx, making it suitable for large state
    // vectors.
    Vector gradient(nx);
    Vector previous_gradient(nx);
    Vector direction(nx);
    Vector test_x(nx); // Used by the line search only

    // Does the last calculation of the cost function in "optimizable"
    // match the current contents of the state vector x? -1=no, 0=yes,
    // 1=yes and the last calculation included the gradient, 2=yes and
    // the last calculation included gradient and Hessian.
    int state_up_to_date = -1;

    // Initial step size
    Real step_size = 1.0;
    if (max_step_size_ > 0.0) {
      step_size = max_step_size_;
    }

    // A restart is performed every nx+1 iterations
    bool do_restart = true;
    int iteration_at_last_restart = n_iterations_;

    // Main loop
    while (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED) {

      // If the last line search found a minimum along the lines
      // satisfying the Wolfe conditions, then the current cost
      // function and gradient will be consistent with the current
      // state vector.  Otherwise we need to compute them.
      if (state_up_to_date < 1) {
	cost_function_ = optimizable.calc_cost_function_gradient(x, gradient);
	state_up_to_date = 1;
	++n_samples_;
      }

      if (n_iterations_ == 0) {
	start_cost_function_ = cost_function_;
      }

      // Check cost function and gradient are finite
      if (!std::isfinite(cost_function_)) {
	status_ = MINIMIZER_STATUS_INVALID_COST_FUNCTION;
	break;
      }
      else if (any(!isfinite(gradient))) {
	status_ = MINIMIZER_STATUS_INVALID_GRADIENT;
	break;
      }

      // Compute L2 norm of gradient to see how "flat" the environment
      // is
      gradient_norm_ = norm2(gradient);

      // Report progress using user-defined function
      optimizable.report_progress(n_iterations_, x, cost_function_, gradient_norm_);

      // Convergence has been achieved if the L2 norm has been reduced
      // to a user-specified threshold
      if (gradient_norm_ <= converged_gradient_norm_) {
	status_ = MINIMIZER_STATUS_SUCCESS;
	break;
      }

      // Restart every nx+1 iterations
      if (n_iterations_ - iteration_at_last_restart > nx) {
	do_restart = true;
      }

      // Find search direction
      if (do_restart) {
	// Simple gradient descent after a restart
	direction = -gradient;
	do_restart = false;
	iteration_at_last_restart = n_iterations_;
      }
      else {
	// The brains of the Conjugate-Gradient method - note that
	// generally the Polak-Ribiere method is believed to be
	// superior to Fletcher-Reeves
	Real beta;
	if (use_fletcher_reeves) {
	  // Fletcher-Reeves method
	  beta = dot_product(gradient, gradient) 
	    / dot_product(previous_gradient, previous_gradient);
	}
	else {
	  // Default: Polak-Ribiere method
	  beta = std::max(sum(gradient * (gradient - previous_gradient))
			  / dot_product(previous_gradient, previous_gradient),
			  0.0);
	}
	// beta==0 is equivalent to gradient descent (i.e. a restart)
	if (beta <= 0) {
	  iteration_at_last_restart = n_iterations_;
	}
	// Compute new direction
	direction = beta*direction - gradient;
      }

      // Store gradient for computing beta in next iteration
      previous_gradient = gradient;

      // Perform line search, storing new state vector in x
      MinimizerStatus ls_status
	= line_search(optimizable, x, direction,
		      test_x, step_size, gradient, state_up_to_date,
		      cg_curvature_coeff_);

      if (ls_status == MINIMIZER_STATUS_SUCCESS) {
	// Successfully minimized along search direction: continue to
	// next iteration
	status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
      }
      else if (iteration_at_last_restart != n_iterations_) {
	// Line search either made no progress or encountered a
	// non-finite cost function or gradient, and this was not a
	// restart; try restarting once
	do_restart = true;
	status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
      }
      else {
	// Unrecoverable failure in line-search: return status to
	// calling function
	status_ = ls_status;
      }

      // Better convergence if first step size on next line search is
      // larger than the actual step size on the last line search
      step_size *= 2.0;

      ++n_iterations_;
      if (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED
	  && n_iterations_ >= max_iterations_) {
	status_ = MINIMIZER_STATUS_MAX_ITERATIONS_REACHED;
      }

      // End of main loop: if status_ is anything other than
      // MINIMIZER_STATUS_NOT_YET_CONVERGED then no more iterations
      // are performed
    }
     
    if (state_up_to_date < ensure_updated_state_) {
      // The last call to calc_cost_function* was not with the state
      // vector returned to the user, and they want it to be.
      if (ensure_updated_state_ > 0) {
	// User wants at least the first derivative
	cost_function_ = optimizable.calc_cost_function_gradient(x, gradient);
      }
      else {
	// User does not need derivatives to have been computed
	cost_function_ = optimizable.calc_cost_function(x);
      }
    }

    return status_;
  }

  // Minimize the cost function embodied in "optimizable" using the
  // Conjugate-Gradient algorithm, where "x" is the initial state
  // vector and also where the solution is stored, subject to the
  // constraint that x lies between min_x and max_x. By default the
  // Polak-Ribiere method is used to compute the new search direction,
  // but Fletcher-Reeves is also available.
  MinimizerStatus
  Minimizer::minimize_conjugate_gradient_bounded(Optimizable& optimizable, Vector x,
					 const Vector& min_x,
					 const Vector& max_x,
					 bool use_fletcher_reeves)
  {
    if (any(min_x >= max_x)
	|| min_x.size() != x.size()
	|| max_x.size() != x.size()) {
      return MINIMIZER_STATUS_INVALID_BOUNDS;
    }

    int nx = x.size();

    // Initial values
    n_iterations_ = 0;
    n_samples_ = 0;
    status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
    cost_function_ = std::numeric_limits<Real>::infinity();

    // The Conjugate-Gradient method is the most efficient
    // gradient-based method in terms of memory usage, requiring a
    // working memory of just 4*nx, making it suitable for large state
    // vectors.
    Vector gradient(nx);
    Vector previous_gradient(nx);
    Vector direction(nx);
    Vector test_x(nx); // Used by the line search only

    // Which state variables are at the minimum bound (-1), maximum
    // bound (1) or free (0)?
    intVector bound_status(nx);
    bound_status = 0;

    // Ensure that initial x lies within the specified bounds
    bound_status.where(x >= max_x) =  1;
    bound_status.where(x <= min_x) = -1;
    x = max(min_x, min(x, max_x));

    int nbound = count(bound_status != 0);
    int nfree  = nx - nbound;

    // Floating-point number containing 1.0 if unbound and 0.0 if
    // bound
    Vector unbound_status(nx);
    unbound_status = 1.0-fabs(bound_status);

    // Does the last calculation of the cost function in "optimizable"
    // match the current contents of the state vector x? -1=no, 0=yes,
    // 1=yes and the last calculation included the gradient, 2=yes and
    // the last calculation included gradient and Hessian.
    int state_up_to_date = -1;

    // Initial step size
    Real step_size = 1.0;
    if (max_step_size_ > 0.0) {
      step_size = max_step_size_;
    }

    // A restart is performed every nx+1 iterations
    bool do_restart = true;
    int iteration_at_last_restart = n_iterations_;

    // Main loop
    while (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED) {

      // If the last line search found a minimum along the lines
      // satisfying the Wolfe conditions, then the current cost
      // function and gradient will be consistent with the current
      // state vector.  Otherwise we need to compute them.
      if (state_up_to_date < 1) {
	cost_function_ = optimizable.calc_cost_function_gradient(x, gradient);
	state_up_to_date = 1;
	++n_samples_;

	if (n_iterations_ == 0) {
	  start_cost_function_ = cost_function_;
	}

	// Check cost function and gradient are finite
	if (!std::isfinite(cost_function_)) {
	  status_ = MINIMIZER_STATUS_INVALID_COST_FUNCTION;
	  break;
	}
	else if (any(!isfinite(gradient))) {
	  status_ = MINIMIZER_STATUS_INVALID_GRADIENT;
	  break;
	}

      }

      // Check whether the bound status of each state variable is
      // consistent with the gradient if a steepest descent were to be
      // taken, and if not flag a restart
      if (any(bound_status == -1 && gradient < 0.0)
	  || any(bound_status == 1 && gradient > 0.0)) {
	bound_status.where(bound_status == -1 && gradient < 0.0) = 0;
	bound_status.where(bound_status ==  1 && gradient > 0.0) = 0;
	unbound_status = 1.0-fabs(bound_status);
	do_restart = true;
      }
      nbound = count(bound_status != 0);
      nfree = nx - nbound;

      // Set gradient at bound points to zero
      gradient.where(bound_status != 0) = 0.0;

      // Compute L2 norm of gradient to see how "flat" the environment
      // is
      if (nfree > 0) {
	gradient_norm_ = norm2(gradient);
      }
      else {
	// If no dimensions are in play we are at a corner of the
	// bounds and the gradient is pointing into the corner: we
	// have reached a minimum in the cost function subject to the
	// bounds so have converged
	gradient_norm_ = 0.0;
      }

      // Report progress using user-defined function
      optimizable.report_progress(n_iterations_, x, cost_function_, gradient_norm_);

      // Convergence has been achieved if the L2 norm has been reduced
      // to a user-specified threshold
      if (gradient_norm_ <= converged_gradient_norm_) {
	status_ = MINIMIZER_STATUS_SUCCESS;
	break;
      }

      // Restart every nx+1 iterations
      if (n_iterations_ - iteration_at_last_restart > nx) {
	do_restart = true;
      }

      // Find search direction
      if (do_restart) {
	// Simple gradient descent after a restart
	direction = -gradient;
	do_restart = false;
	iteration_at_last_restart = n_iterations_;
      }
      else {
	// The brains of the Conjugate-Gradient method - note that
	// generally the Polak-Ribiere method is believed to be
	// superior to Fletcher-Reeves
	Real beta;
	if (use_fletcher_reeves) {
	  // Fletcher-Reeves method
	  beta = dot_product(gradient, gradient) 
	    / dot_product(previous_gradient, previous_gradient);
	}
	else {
	  // Default: Polak-Ribiere method
	  beta = std::max(sum(gradient * (gradient - previous_gradient))
			  / dot_product(previous_gradient, previous_gradient),
			  0.0);
	}
	// beta==0 is equivalent to gradient descent (i.e. a restart)
	if (beta <= 0) {
	  iteration_at_last_restart = n_iterations_;
	}
	// Compute new direction
	direction = beta*direction - gradient;
      }

      // Store gradient for computing beta in next iteration
      previous_gradient = gradient;

      // Distance to the nearest bound
      Real dir_scaling = norm2(direction);
      Real bound_step_size = std::numeric_limits<Real>::max();
      int i_nearest_bound = -1;
      int i_bound_type = 0;
      // Work out the maximum step size along "direction" before a
      // bound is met... there must be a faster way to do this
      for (int ix = 0; ix < nx; ++ix) {
	if (direction(ix) > 0.0 && max_x(ix) < std::numeric_limits<Real>::max()) {
	  Real local_bound_step_size = dir_scaling*(max_x(ix)-x(ix))/direction(ix);
	  if (bound_step_size >= local_bound_step_size) {
	    bound_step_size = local_bound_step_size;
	    i_nearest_bound = ix;
	    i_bound_type = 1;
	  }				   
	}
	else if (direction(ix) < 0.0 && min_x(ix) > -std::numeric_limits<Real>::max()) {
	  Real local_bound_step_size = dir_scaling*(min_x(ix)-x(ix))/direction(ix);
	  if (bound_step_size >= local_bound_step_size) {
	    bound_step_size = local_bound_step_size;
	    i_nearest_bound = ix;
	    i_bound_type = -1;
	  }
	}
      }

      MinimizerStatus ls_status; // line-search outcome
      if (i_nearest_bound >= 0) {
	// Perform line search, storing new state vector in x
	ls_status = line_search(optimizable, x, direction,
			       test_x, step_size, gradient, state_up_to_date,
			       cg_curvature_coeff_, bound_step_size);
	if (ls_status == MINIMIZER_STATUS_BOUND_REACHED) {
	  bound_status(i_nearest_bound) = i_bound_type;
	  do_restart = true;
	  ls_status = MINIMIZER_STATUS_SUCCESS;
	}
      }
      else {
	// Perform line search, storing new state vector in x
	ls_status = line_search(optimizable, x, direction,
				test_x, step_size, gradient, state_up_to_date,
				cg_curvature_coeff_);
      }

      if (ls_status == MINIMIZER_STATUS_SUCCESS) {
	// Successfully minimized along search direction: continue to
	// next iteration
	status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
      }
      else if (iteration_at_last_restart != n_iterations_) {
	// Line search either made no progress or encountered a
	// non-finite cost function or gradient, and this was not a
	// restart; try restarting once
	do_restart = true;
	status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
      }
      else {
	// Unrecoverable failure in line-search: return status to
	// calling function
	status_ = ls_status;
      }

      // Better convergence if first step size on next line search is
      // larger than the actual step size on the last line search
      step_size *= 2.0;

      ++n_iterations_;
      if (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED
	  && n_iterations_ >= max_iterations_) {
	status_ = MINIMIZER_STATUS_MAX_ITERATIONS_REACHED;
      }

      // End of main loop: if status_ is anything other than
      // MINIMIZER_STATUS_NOT_YET_CONVERGED then no more iterations
      // are performed
    }
     
    if (state_up_to_date < ensure_updated_state_) {
      // The last call to calc_cost_function* was not with the state
      // vector returned to the user, and they want it to be.
      if (ensure_updated_state_ > 0) {
	// User wants at least the first derivative
	cost_function_ = optimizable.calc_cost_function_gradient(x, gradient);
      }
      else {
	// User does not need derivatives to have been computed
	cost_function_ = optimizable.calc_cost_function(x);
      }
    }

    return status_;
  }

};


// =================================================================
// Contents of minimize_levenberg_marquardt.cpp
// =================================================================

/* minimize_levenberg_marquardt.cpp -- Minimize function using Levenberg-Marquardt algorithm

    Copyright (C) 2020 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/

#include <limits>
#include <cmath>
#include <adept/Minimizer.h>

namespace adept {

  // Minimize the cost function embodied in "optimizable" using the
  // Levenberg-Marquardt algorithm, where "x" is the initial state
  // vector and also where the solution is stored.
  MinimizerStatus
  Minimizer::minimize_levenberg_marquardt(Optimizable& optimizable, Vector x,
					  bool use_additive_damping)
  {
    int nx = x.size();

    // Initial values
    n_iterations_ = 0;
    n_samples_ = 0;
    status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
    cost_function_ = std::numeric_limits<Real>::infinity();

    Real new_cost;

    // The main memory storage for the Levenberg family of methods
    // consists of the following three vectors...
    Vector new_x(nx);
    Vector gradient(nx);
    Vector dx(nx);

    // ...and the Hessian matrix, which is stored explicitly
    SymmMatrix hessian(nx);
    hessian = 0.0;

    Real damping = levenberg_damping_start_;
    gradient_norm_ = -1.0;

    // Original Levenberg is additive to the diagonal of the Hessian
    // so to make the performance insensitive to an arbitrary scaling
    // of the cost function, we scale the damping factor by the mean
    // of the diagonal of the Hessian
    Real diag_scaling;

    // Does the last calculation of the cost function in "optimizable"
    // match the current contents of the state vector x? -1=no, 0=yes,
    // 1=yes and the last calculation included the gradient, 2=yes and
    // the last calculation included gradient and Hessian.
    int state_up_to_date = -1;

    do {
      // At this point we have either just started or have just
      // reduced the cost function
      cost_function_ = optimizable.calc_cost_function_gradient_hessian(x, gradient, hessian);
      diag_scaling = mean(hessian.diag_vector());
      state_up_to_date = 2;
      ++n_samples_;
      if (n_iterations_ == 0) {
	start_cost_function_ = cost_function_;
      }

      // Check cost function and gradient are finite
      if (!std::isfinite(cost_function_)) {
	status_ = MINIMIZER_STATUS_INVALID_COST_FUNCTION;
	break;
      }
      else if (any(!isfinite(gradient))) {
	status_ = MINIMIZER_STATUS_INVALID_GRADIENT;
	break;
      }
      // Compute L2 norm of gradient to see how "flat" the environment
      // is
      gradient_norm_ = norm2(gradient);
      // Report progress using user-defined function
      optimizable.report_progress(n_iterations_, x, cost_function_, gradient_norm_);
      // Convergence has been achieved if the L2 norm has been reduced
      // to a user-specified threshold
      if (gradient_norm_ <= converged_gradient_norm_) {
	status_ = MINIMIZER_STATUS_SUCCESS;
	break;
      }

      // Try to minimize cost function 
      Real previous_diag_scaling  = 1.0; // Used in Levenberg-Marquardt version
      Real previous_diag_modifier = 0.0; // Used in Levenberg version
      while(true) {
	if (!use_additive_damping) {
	  // Levenberg-Marquardt formula: scale the diagonal of the
	  // Hessian, where the larger the value of "damping", the
	  // closer the resulting behaviour is to steepest descent
	  hessian.diag_vector() *= (1.0 + damping)/previous_diag_scaling;
	  previous_diag_scaling = 1.0 + damping;
	}
	else {
	  // Older Levenberg approach: add to the diagonal instead
	  hessian.diag_vector() += damping*diag_scaling - previous_diag_modifier;
	  previous_diag_modifier = damping*diag_scaling;
	}
	dx = -adept::solve(hessian, gradient);

	// Limit the maximum step size, if required
	if (max_step_size_ > 0.0) {
	  Real max_dx = maxval(abs(dx));
	  if (max_dx > max_step_size_) {
	    dx *= (max_step_size_/max_dx);
	  }
	}

	// Compute new cost state vector and cost function, but not
	// gradient or Hessian for efficiency
	new_x = x+dx;
	new_cost = optimizable.calc_cost_function(new_x);
	state_up_to_date = -1;
	++n_samples_;

	// If cost function is not finite it may be possible to
	// recover by trying smaller step sizes
	bool cost_invalid = !std::isfinite(new_cost);

	if (new_cost >= cost_function_ || cost_invalid) {
	  // We haven't managed to reduce the cost function: increase
	  // damping value to take smaller steps
	  if (damping <= 0.0) {
	    damping = levenberg_damping_restart_;
	  }
	  else if (damping < levenberg_damping_max_) {
	    damping *= levenberg_damping_multiplier_;
	  }
	  else {
	    // The damping value is now larger than the maximum so we
	    // can get no further
	    if (cost_invalid) {
	      status_ = MINIMIZER_STATUS_INVALID_COST_FUNCTION;
	    }
	    else {
	      status_ = MINIMIZER_STATUS_FAILED_TO_CONVERGE;
	    }
	    break;
	  }
	}
	else {
	  // Managed to reduce cost function
	  x = new_x;
	  n_iterations_++;
	  // Reduce damping for next iteration
	  if (damping > levenberg_damping_min_) {
	    damping /= levenberg_damping_divider_;
	  }
	  else {
	    damping = 0.0;
	  }
	  if (n_iterations_ >= max_iterations_) {
	    status_ = MINIMIZER_STATUS_MAX_ITERATIONS_REACHED;
	  }
	  break;
	}
      } // Inner loop
    }
    while (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED);
     
    if (state_up_to_date < ensure_updated_state_) {
      // The last call to calc_cost_function* was not with the state
      // vector returned to the user, and they want it to be.  Note
      // that the cost function and gradient norm ought to be
      // up-to-date already at this point.
      if (ensure_updated_state_ > 0) {
	// User wants at least the first derivative, but
	// calc_cost_function_gradient() is not guaranteed to be
	// present so we call the hessain function
	cost_function_ = optimizable.calc_cost_function_gradient_hessian(x, gradient,
									 hessian);
      }
      else {
	// User does not need derivatives to have been computed
	cost_function_ = optimizable.calc_cost_function(x);
      }
    }

    return status_;
  }


  // Minimize the cost function embodied in "optimizable" using the
  // Levenberg-Marquardt algorithm, where "x" is the initial state
  // vector and also where the solution is stored, subject to the
  // constraint that x lies between min_x and max_x.
  MinimizerStatus
  Minimizer::minimize_levenberg_marquardt_bounded(Optimizable& optimizable,
						  Vector x,
						  const Vector& min_x,
						  const Vector& max_x,
						  bool use_additive_damping)
  {
    if (any(min_x >= max_x)
	|| min_x.size() != x.size()
	|| max_x.size() != x.size()) {
      return MINIMIZER_STATUS_INVALID_BOUNDS;
    }

    int nx = x.size();

    // Initial values
    n_iterations_ = 0;
    n_samples_ = 0;
    status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
    cost_function_ = std::numeric_limits<Real>::infinity();

    Real new_cost;

    // The main memory storage for the Levenberg family of methods
    // consists of the following three vectors...
    Vector new_x(nx);
    Vector gradient(nx);
    Vector dx(nx);

    // ...and the Hessian matrix, which is stored explicitly
    SymmMatrix hessian(nx);
    SymmMatrix modified_hessian(nx);
    SymmMatrix sub_hessian;
    Vector sub_gradient;
    Vector sub_dx;
    hessian = 0.0;
    Real damping = levenberg_damping_start_;

    // Which state variables are at the minimum bound (-1), maximum
    // bound (1) or free (0)?
    intVector bound_status(nx);
    bound_status = 0;

    // Ensure that initial x lies within the specified bounds
    bound_status.where(x >= max_x) =  1;
    bound_status.where(x <= min_x) = -1;
    x = max(min_x, min(x, max_x));

    int nbound = count(bound_status != 0);
    int nfree  = nx - nbound;
    gradient_norm_ = -1.0;

    // Original Levenberg is additive to the diagonal of the Hessian
    // so to make the performance insensitive to an arbitrary scaling
    // of the cost function, we scale the damping factor by the mean
    // of the diagonal of the Hessian
    Real diag_scaling;

    // Does the last calculation of the cost function in "optimizable"
    // match the current contents of the state vector x? -1=no, 0=yes,
    // 1=yes and the last calculation included the gradient, 2=yes and
    // the last calculation included gradient and Hessian.
    int state_up_to_date = -1;

    do {
      // At this point we have either just started or have just
      // reduced the cost function
      cost_function_ = optimizable.calc_cost_function_gradient_hessian(x, gradient, hessian);
      diag_scaling = mean(hessian.diag_vector());
      state_up_to_date = 2;
      ++n_samples_;
      if (n_iterations_ == 0) {
	start_cost_function_ = cost_function_;
      }

      // Check cost function and gradient are finite
      if (!std::isfinite(cost_function_)) {
	status_ = MINIMIZER_STATUS_INVALID_COST_FUNCTION;
	break;
      }
      else if (any(!isfinite(gradient))) {
	status_ = MINIMIZER_STATUS_INVALID_GRADIENT;
	break;
      }

      // Find which dimensions are in play
      if (nbound > 0) {
	// We release any dimensions from being at a minimum or
	// maximum bound if two conditions are met: (1) the gradient
	// in that dimension slopes away from the bound, and (2) the
	// Levenberg-Marquardt formula to compute dx using the current
	// value of "damping" leads to a point on the valid side of the
	// bound
	modified_hessian = hessian;
	if (!use_additive_damping) {
	  modified_hessian.diag_vector() *= (1.0 + damping);
	}
	else {
	  modified_hessian.diag_vector() += damping*diag_scaling;
	}
	dx = -adept::solve(modified_hessian, gradient);
	// Release points at the minimum bound
	bound_status.where(bound_status == -1
			   && gradient < 0.0
			   && dx > 0.0) = 0;
	// Release points at the maximum bound
	bound_status.where(bound_status == 1
			   && gradient > 0.0
			   && dx < 0.0) = 0;
      }

      nbound = count(bound_status != 0);
      nfree  = nx - nbound;

      // List of indices of free state variables
      intVector ifree(nfree);
      if (nbound > 0) {
	ifree = find(bound_status == 0);
      }
      else {
	ifree = range(0, nx-1);
      }

      // Compute L2 norm of gradient to see how "flat" the environment
      // is, restricting ourselves to the dimensions currently in play
      if (nfree > 0) {
	gradient_norm_ = norm2(gradient(ifree));
      }
      else {
	// If no dimensions are in play we are at a corner of the
	// bounds and the gradient is pointing into the corner: we
	// have reached a minimum in the cost function subject to the
	// bounds so have converged
	gradient_norm_ = 0.0;
      }
      // Report progress using user-defined function
      optimizable.report_progress(n_iterations_, x, cost_function_, gradient_norm_);
      // Convergence has been achieved if the L2 norm has been reduced
      // to a user-specified threshold
      if (gradient_norm_ <= converged_gradient_norm_) {
	status_ = MINIMIZER_STATUS_SUCCESS;
	break;
      }

      sub_gradient.clear();
      sub_hessian.clear();
      if (nbound > 0) {
	sub_gradient = gradient(ifree);
	sub_hessian  = SymmMatrix(Matrix(hessian)(ifree,ifree));
      }
      else {
	sub_gradient >>= gradient;
	sub_hessian  >>= hessian;
      }

      // FIX reuse dx if possible below...

      // Try to minimize cost function 
      Real previous_diag_scaling  = 1.0; // Used in Levenberg-Marquardt version
      Real previous_diag_modifier = 0.0; // Used in Levenberg version
      while(true) {
	sub_dx.resize(nfree);
	if (!use_additive_damping) {
	  // Levenberg-Marquardt formula: scale the diagonal of the
	  // Hessian, where the larger the value of "damping", the
	  // closer the resulting behaviour is to steepest descent
	  sub_hessian.diag_vector() *= (1.0 + damping)/previous_diag_scaling;
	  previous_diag_scaling = 1.0 + damping;
	}
	else {
	  // Older Levenberg approach: add to the diagonal instead
	  sub_hessian.diag_vector() += damping*diag_scaling - previous_diag_modifier;
	  previous_diag_modifier = damping*diag_scaling;
	}
	sub_dx = -adept::solve(sub_hessian, sub_gradient);

	// Limit the maximum step size, if required
	if (max_step_size_ > 0.0) {
	  Real max_dx = maxval(abs(sub_dx));
	  if (max_dx > max_step_size_) {
	    sub_dx *= (max_step_size_/max_dx);
	  }
	}

	// Check for collision with new bounds
	intVector new_min_bounds = find(x(ifree)+sub_dx <= min_x(ifree));
	intVector new_max_bounds = find(x(ifree)+sub_dx >= max_x(ifree));
	Real mmin_frac = 2.0;
	Real mmax_frac = 2.0;
	int imin = 0, imax = 0;
	if (!new_min_bounds.empty()) {
	  Vector min_frac = -(x(ifree(new_min_bounds)) - min_x(ifree(new_min_bounds)))
	    / sub_dx(new_min_bounds);
	  mmin_frac = minval(min_frac);
	  imin = new_min_bounds(minloc(min_frac));
	}
	if (!new_max_bounds.empty()) {
	  Vector max_frac = (max_x(ifree(new_max_bounds)) - x(ifree(new_max_bounds)))
	    / sub_dx(new_max_bounds);
	  mmax_frac = minval(max_frac);
	  imax = new_max_bounds(maxloc(max_frac));
	}

	Real frac = 1.0;
	int bound_type = 0;
	int ibound = 0;
	if (mmin_frac <= 1.0 || mmax_frac <= 1.0) {
	  if (mmin_frac < mmax_frac) {
	    frac = mmin_frac;
	    ibound = imin;
	    bound_type = -1;
	  }
	  else {
	    frac = mmax_frac;
	    ibound = imax;
	    bound_type = 1;
	  }	  
	  sub_dx *= frac;
	}

	// Compute new state vector and cost function, but not
	// gradient or Hessian for efficiency
	new_x = x;
	new_x(ifree) += sub_dx;
	new_cost = optimizable.calc_cost_function(new_x);
	state_up_to_date = -1;
	++n_samples_;

	// If cost function is not finite it may be possible to
	// recover by trying smaller step sizes
	bool cost_invalid = !std::isfinite(new_cost);

	if (new_cost >= cost_function_ || cost_invalid) {
	  // We haven't managed to reduce the cost function: increase
	  // damping value to take smaller steps
	  if (damping <= 0.0) {
	    damping = levenberg_damping_restart_;
	  }
	  else if (damping < levenberg_damping_max_) {
	    damping *= levenberg_damping_multiplier_;
	  }
	  else {
	    // The damping value is now larger than the maximum so we
	    // can get no further
	    if (cost_invalid) {
	      status_ = MINIMIZER_STATUS_INVALID_COST_FUNCTION;
	    }
	    else {
	      status_ = MINIMIZER_STATUS_FAILED_TO_CONVERGE;
	    }
	    break;
	  }
	}
	else {
	  // Managed to reduce cost function
	  x = new_x;
	  n_iterations_++;
	  if (frac < 1.0) {
	    // Found a new bound
	    bound_status(ifree(ibound)) = bound_type;
	  }
	  // Reduce damping for next iteration
	  if (damping > levenberg_damping_min_) {
	    damping /= levenberg_damping_divider_;
	  }
	  else {
	    damping = 0.0;
	  }
	  if (n_iterations_ >= max_iterations_) {
	    status_ = MINIMIZER_STATUS_MAX_ITERATIONS_REACHED;
	  }
	  break;
	}
      } // Inner loop
    }
    while (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED);
    
    if (state_up_to_date < ensure_updated_state_) {
      // The last call to calc_cost_function* was not with the state
      // vector returned to the user, and they want it to be.  Note
      // that the cost function and gradient norm ought to be
      // up-to-date already at this point.
      if (ensure_updated_state_ > 0) {
	// User wants at least the first derivative, but
	// calc_cost_function_gradient() is not guaranteed to be
	// present so we call the hessain function
	cost_function_ = optimizable.calc_cost_function_gradient_hessian(x, gradient,
									 hessian);
      }
      else {
	// User does not need derivatives to have been computed
	cost_function_ = optimizable.calc_cost_function(x);
      }
    }

    return status_;
  }

};


// =================================================================
// Contents of minimize_limited_memory_bfgs.cpp
// =================================================================

/* minimize_limited_memory_bfgs.cpp -- Minimize function using Limited-Memory BFGS algorithm

    Copyright (C) 2020 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/

#include <limits>

#include <adept/Minimizer.h>

namespace adept {

  // Structure for storing data from previous iterations used by
  // L-BFGS minimization algorithm
  class LbfgsData {

  public:
    LbfgsData(int nx, int ni)
      : nx_(nx), ni_(ni), iteration_(0) {
      x_diff_.resize(ni,nx);
      gradient_diff_.resize(ni,nx);
      rho_.resize(ni);
      alpha_.resize(ni);
      gamma_.resize(ni);
    }

    // Return false if the dot product of x_diff and gradient_diff is
    // zero, true otherwise
    void store(int iter, const Vector& x_diff, const Vector& gradient_diff) {
      int index = (iter-1) % ni_;
      x_diff_[index] = x_diff;
      gradient_diff_[index] = gradient_diff;
      Real dp = dot_product(x_diff, gradient_diff);
      if (std::fabs(dp) > 10.0*std::numeric_limits<Real>::min()) {
	rho_[index] = 1.0 / dp;
      }
      else if (dp >= 0.0) {
	rho_[index] = 1.0 / std::max(dp, 10.0*std::numeric_limits<Real>::min());
      }
      else {
	rho_[index] = 1.0 / std::min(dp, -10.0*std::numeric_limits<Real>::min());
      }
    }

    // Return read-only vectors containing the differences between
    // state vectors and gradients at sequential iterations, by
    // slicing off the appropriate row of the matrix
    Vector x_diff(int iter) {
      return x_diff_[iter % ni_];
    };
    Vector gradient_diff(int iter) {
      return gradient_diff_[iter % ni_];
    };

    Real& alpha(int iter) { return alpha_[iter % ni_]; }
    Real rho(int iter) const { return rho_[iter % ni_]; }
    Real gamma(int iter) const { return gamma_[iter % ni_]; }

  private:
    // Data
    int nx_; // Number of state variables
    int ni_; // Number of iterations to store
    int iteration_; // Current iteration
    Matrix x_diff_;
    Matrix gradient_diff_;
    Vector rho_;
    Vector alpha_;
    Vector gamma_;
  };


  // Minimize the cost function embodied in "optimizable" using the
  // Limited-Memory Broyden-Fletcher-Goldfarb-Shanno (L-BFGS)
  // algorithm, where "x" is the initial state vector and also where
  // the solution is stored.
  MinimizerStatus
  Minimizer::minimize_limited_memory_bfgs(Optimizable& optimizable, Vector x)
  {

    int nx = x.size();

    // Initial values
    n_iterations_ = 0;
    n_samples_ = 0;
    status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
    cost_function_ = std::numeric_limits<Real>::infinity();

    Vector previous_x(nx);
    Vector gradient(nx);
    Vector previous_gradient(nx);
    Vector direction(nx);
    Vector test_x(nx); // Used by the line search only

    // Previous states needed by the L-BFGS algorithm
    int n_states = std::min(nx, lbfgs_n_states_);
    LbfgsData data(nx, n_states);

    // Does the last calculation of the cost function in "optimizable"
    // match the current contents of the state vector x? -1=no, 0=yes,
    // 1=yes and the last calculation included the gradient, 2=yes and
    // the last calculation included gradient and Hessian.
    int state_up_to_date = -1;

    // Initial step size
    Real step_size = 1.0;
    if (max_step_size_ > 0.0) {
      step_size = max_step_size_;
    }

    // Main loop
    while (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED) {

      // If the last line search found a minimum along the lines
      // satisfying the Wolfe conditions, then the current cost
      // function and gradient will be consistent with the current
      // state vector.  Otherwise we need to compute them.
      if (state_up_to_date < 1) {
	cost_function_ = optimizable.calc_cost_function_gradient(x, gradient);
	state_up_to_date = 1;
	++n_samples_;

	if (n_iterations_ == 0) {
	  start_cost_function_ = cost_function_;
	}

	// Check cost function and gradient are finite
	if (!std::isfinite(cost_function_)) {
	  status_ = MINIMIZER_STATUS_INVALID_COST_FUNCTION;
	  break;
	}
	else if (any(!isfinite(gradient))) {
	  status_ = MINIMIZER_STATUS_INVALID_GRADIENT;
	  break;
	}
      }

      // Check cost function and gradient are finite
      if (!std::isfinite(cost_function_)) {
	status_ = MINIMIZER_STATUS_INVALID_COST_FUNCTION;
	break;
      }
      else if (any(!isfinite(gradient))) {
	status_ = MINIMIZER_STATUS_INVALID_GRADIENT;
	break;
      }

      // Compute L2 norm of gradient to see how "flat" the environment
      // is
      gradient_norm_ = norm2(gradient);

      // Report progress using user-defined function
      optimizable.report_progress(n_iterations_, x, cost_function_, gradient_norm_);

      // Convergence has been achieved if the L2 norm has been reduced
      // to a user-specified threshold
      if (gradient_norm_ <= converged_gradient_norm_) {
	status_ = MINIMIZER_STATUS_SUCCESS;
	break;
      }

      // Store state and gradient differences
      if (n_iterations_ > 0) {
	data.store(n_iterations_, x-previous_x, gradient-previous_gradient);
      }

      // Find search direction: see page 779 of Nocedal (1980):
      // Updating quasi-Newton matrices with limited
      // storage. Mathematics of Computation, 35, 773-782.
      direction = gradient;
      if (n_iterations_ > 0) {

	for (int ii = n_iterations_-1;
	     ii >= std::max(0,n_iterations_-n_states);
	     --ii) {
	  data.alpha(ii) = data.rho(ii) 
	    * dot_product(data.x_diff(ii), direction);
	  direction -= data.alpha(ii) * data.gradient_diff(ii);
	}

	Real gamma = dot_product(x-previous_x, gradient-previous_gradient)
	  / std::max(10.0*std::numeric_limits<Real>::min(),
		     dot_product(gradient-previous_gradient, gradient-previous_gradient));
	direction *= gamma;

	for (int ii = std::max(0,n_iterations_-n_states);
	     ii < n_iterations_;
	     ++ii) {
	  Real beta = data.rho(ii) * dot_product(data.gradient_diff(ii), direction);
	  direction += data.x_diff(ii) * (data.alpha(ii)-beta);
	}

	direction = -direction;
      }
      else {
	direction = -gradient * (step_size / norm2(gradient));
      }

      // Store state and gradient
      previous_x = x;
      previous_gradient = gradient;

      // Perform line search, storing new state vector in x, and
      // returning MINIMIZER_STATUS_NOT_YET_CONVERGED on success
      Real curvature_coeff = lbfgs_curvature_coeff_;
      if (n_iterations_ < n_states) {
	// In the early iterations we require the line search to be
	// more accurate since the L-BFGS update will have fewer
	// states to make a good estimate of the minimum; interpolate
	// between the Conjugate Gradient and L-BFGS curvature
	// coefficients
	curvature_coeff = (cg_curvature_coeff_ * (n_states-n_iterations_)
			   + lbfgs_curvature_coeff_ * n_iterations_)
	  / n_states;
      }

      // Direction points to the best estimate of the actual location
      // of the minimum, so the step size is the norm of the direction
      // vector
      step_size = norm2(direction);
      MinimizerStatus ls_status
	= line_search(optimizable, x, direction,
		      test_x, step_size, gradient, state_up_to_date,
		      curvature_coeff);

      if (ls_status == MINIMIZER_STATUS_SUCCESS) {
	// Successfully minimized along search direction: continue to
	// next iteration
	status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
      }
      else {
	// Unrecoverable failure in line-search: return status to
	// calling function
	status_ = ls_status;
      }

      ++n_iterations_;
      if (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED
	  && n_iterations_ >= max_iterations_) {
	status_ = MINIMIZER_STATUS_MAX_ITERATIONS_REACHED;
      }

      // End of main loop: if status_ is anything other than
      // MINIMIZER_STATUS_NOT_YET_CONVERGED then no more iterations
      // are performed
    }
     
    if (state_up_to_date < ensure_updated_state_) {
      // The last call to calc_cost_function* was not with the state
      // vector returned to the user, and they want it to be.
      if (ensure_updated_state_ > 0) {
	// User wants at least the first derivative
	cost_function_ = optimizable.calc_cost_function_gradient(x, gradient);
      }
      else {
	// User does not need derivatives to have been computed
	cost_function_ = optimizable.calc_cost_function(x);
      }
    }

    return status_;
  }

  // Minimize the cost function embodied in "optimizable" using the
  // Limited-Memory Broyden-Fletcher-Goldfarb-Shanno (L-BFGS)
  // algorithm, where "x" is the initial state vector and also where
  // the solution is stored.
  MinimizerStatus
  Minimizer::minimize_limited_memory_bfgs_bounded(Optimizable& optimizable, Vector x,
						  const Vector& min_x,
						  const Vector& max_x)
  {
    if (any(min_x >= max_x)
	|| min_x.size() != x.size()
	|| max_x.size() != x.size()) {
      return MINIMIZER_STATUS_INVALID_BOUNDS;
    }

    int nx = x.size();

    // Initial values
    n_iterations_ = 0;
    n_samples_ = 0;
    status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
    cost_function_ = std::numeric_limits<Real>::infinity();

    Vector previous_x(nx);
    Vector gradient(nx);
    Vector previous_gradient(nx);
    Vector direction(nx);
    Vector test_x(nx); // Used by the line search only

    // Previous states needed by the L-BFGS algorithm
    int n_states = std::min(nx, lbfgs_n_states_);
    LbfgsData data(nx, n_states);

    // Which state variables are at the minimum bound (-1), maximum
    // bound (1) or free (0)?
    intVector bound_status(nx);
    bound_status = 0;

    // Ensure that initial x lies within the specified bounds
    bound_status.where(x >= max_x) =  1;
    bound_status.where(x <= min_x) = -1;
    x = max(min_x, min(x, max_x));

    int nbound = count(bound_status != 0);
    int nfree  = nx - nbound;

    // Floating-point number containing 1.0 if unbound and 0.0 if
    // bound
    Vector unbound_status(nx);
    unbound_status = 1.0-fabs(bound_status);

    // If we reach a bound we need to restart the L-BFGS storage, so
    // store the iteration at the last restart
    int iteration_last_restart = 0;

    // Does the last calculation of the cost function in "optimizable"
    // match the current contents of the state vector x? -1=no, 0=yes,
    // 1=yes and the last calculation included the gradient, 2=yes and
    // the last calculation included gradient and Hessian.
    int state_up_to_date = -1;

    // Initial step size
    Real step_size = 1.0;
    if (max_step_size_ > 0.0) {
      step_size = max_step_size_;
    }

    // Main loop
    while (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED) {

      // If the last line search found a minimum along the lines
      // satisfying the Wolfe conditions, then the current cost
      // function and gradient will be consistent with the current
      // state vector.  Otherwise we need to compute them.
      if (state_up_to_date < 1) {
	cost_function_ = optimizable.calc_cost_function_gradient(x, gradient);
	state_up_to_date = 1;
	++n_samples_;

	if (n_iterations_ == 0) {
	  start_cost_function_ = cost_function_;
	}

	// Check cost function and gradient are finite
	if (!std::isfinite(cost_function_)) {
	  status_ = MINIMIZER_STATUS_INVALID_COST_FUNCTION;
	  break;
	}
	else if (any(!isfinite(gradient))) {
	  status_ = MINIMIZER_STATUS_INVALID_GRADIENT;
	  break;
	}
      }

      // Check whether the bound status of each state variable is
      // consistent with the gradient if a steepest descent were to be
      // taken, and if not flag a restart
      if (any(bound_status == -1 && gradient < 0.0)
	  || any(bound_status == 1 && gradient > 0.0)) {
	bound_status.where(bound_status == -1 && gradient < 0.0) = 0;
	bound_status.where(bound_status ==  1 && gradient > 0.0) = 0;
	unbound_status = 1.0-fabs(bound_status);
	iteration_last_restart = n_iterations_;
      }
      nbound = count(bound_status != 0);
      nfree = nx - nbound;

      // Set gradient at bound points to zero
      gradient.where(bound_status != 0) = 0.0;

      // Compute L2 norm of gradient to see how "flat" the environment
      // is
      if (nfree > 0) {
	gradient_norm_ = norm2(gradient);
      }
      else {
	// If no dimensions are in play we are at a corner of the
	// bounds and the gradient is pointing into the corner: we
	// have reached a minimum in the cost function subject to the
	// bounds so have converged
	gradient_norm_ = 0.0;
      }

      // Report progress using user-defined function
      optimizable.report_progress(n_iterations_, x, cost_function_, gradient_norm_);

      // Convergence has been achieved if the L2 norm has been reduced
      // to a user-specified threshold
      if (gradient_norm_ <= converged_gradient_norm_) {
	status_ = MINIMIZER_STATUS_SUCCESS;
	break;
      }

      // Store state and gradient differences
      if (n_iterations_ > iteration_last_restart) {
	data.store(n_iterations_, x-previous_x, gradient-previous_gradient);
      }

      // Find search direction: see page 779 of Nocedal (1980):
      // Updating quasi-Newton matrices with limited
      // storage. Mathematics of Computation, 35, 773-782.
      direction = gradient;
      if (n_iterations_ > iteration_last_restart) {

	for (int ii = n_iterations_-1;
	     ii >= std::max(iteration_last_restart,n_iterations_-n_states);
	     --ii) {
	  data.alpha(ii) = data.rho(ii) 
	    * dot_product(data.x_diff(ii), direction);
	  direction -= data.alpha(ii) * data.gradient_diff(ii);
	}

	Real gamma = dot_product(x-previous_x, gradient-previous_gradient)
	  / std::max(10.0*std::numeric_limits<Real>::min(),
		     dot_product(gradient-previous_gradient, gradient-previous_gradient));
	direction *= gamma;

	for (int ii = std::max(iteration_last_restart,n_iterations_-n_states);
	     ii < n_iterations_;
	     ++ii) {
	  Real beta = data.rho(ii) * dot_product(data.gradient_diff(ii), direction);
	  direction += data.x_diff(ii) * (data.alpha(ii)-beta);
	}

	direction = -direction;
      }
      else {
	// We are either at the first iteration or have restarted
	// having changed the bound dimensions: use steepest descent
	direction = -gradient * (step_size / norm2(gradient));
      }

      // Store state and gradient
      previous_x = x;
      previous_gradient = gradient;

      // Perform line search, storing new state vector in x, and
      // returning MINIMIZER_STATUS_NOT_YET_CONVERGED on success
      Real curvature_coeff = lbfgs_curvature_coeff_;
      int n_stored_iterations = n_iterations_ - iteration_last_restart;
      if (n_stored_iterations < n_states) {
	// In the early iterations we require the line search to be
	// more accurate since the L-BFGS update will have fewer
	// states to make a good estimate of the minimum; interpolate
	// between the Conjugate Gradient and L-BFGS curvature
	// coefficients
	curvature_coeff = (cg_curvature_coeff_ * (n_states-n_stored_iterations)
			   + lbfgs_curvature_coeff_ * n_stored_iterations)
	  / n_states;
      }

      // Direction points to the best estimate of the actual location
      // of the minimum, so the step size is the norm of the direction
      // vector
      step_size = norm2(direction);

      // Distance to the nearest bound
      Real dir_scaling = step_size;
      Real bound_step_size = std::numeric_limits<Real>::max();
      int i_nearest_bound = -1;
      int i_bound_type = 0;
      // Work out the maximum step size along "direction" before a
      // bound is met... there must be a faster way to do this
      for (int ix = 0; ix < nx; ++ix) {
	if (direction(ix) > 0.0 && max_x(ix) < std::numeric_limits<Real>::max()) {
	  Real local_bound_step_size = dir_scaling*(max_x(ix)-x(ix))/direction(ix);
	  if (bound_step_size >= local_bound_step_size) {
	    bound_step_size = local_bound_step_size;
	    i_nearest_bound = ix;
	    i_bound_type = 1;
	  }				   
	}
	else if (direction(ix) < 0.0 && min_x(ix) > -std::numeric_limits<Real>::max()) {
	  Real local_bound_step_size = dir_scaling*(min_x(ix)-x(ix))/direction(ix);
	  if (bound_step_size >= local_bound_step_size) {
	    bound_step_size = local_bound_step_size;
	    i_nearest_bound = ix;
	    i_bound_type = -1;
	  }
	}
      }

      MinimizerStatus ls_status; // line-search outcome
      if (i_nearest_bound >= 0) {
	// Perform line search, storing new state vector in x
	ls_status = line_search(optimizable, x, direction,
				test_x, step_size, gradient, state_up_to_date,
				curvature_coeff, bound_step_size);
	if (ls_status == MINIMIZER_STATUS_BOUND_REACHED) {
	  bound_status(i_nearest_bound) = i_bound_type;
	  // Restart the L-BFGS storage
	  iteration_last_restart = n_iterations_+1;
	  ls_status = MINIMIZER_STATUS_SUCCESS;
	}
      }
      else {
	// Perform line search, storing new state vector in x
	ls_status = line_search(optimizable, x, direction,
				test_x, step_size, gradient, state_up_to_date,
				curvature_coeff);
      }

      if (ls_status == MINIMIZER_STATUS_SUCCESS) {
	// Successfully minimized along search direction: continue to
	// next iteration
	status_ = MINIMIZER_STATUS_NOT_YET_CONVERGED;
      }
      else {
	// Unrecoverable failure in line-search: return status to
	// calling function
	status_ = ls_status;
      }

      ++n_iterations_;
      if (status_ == MINIMIZER_STATUS_NOT_YET_CONVERGED
	  && n_iterations_ >= max_iterations_) {
	status_ = MINIMIZER_STATUS_MAX_ITERATIONS_REACHED;
      }

      // End of main loop: if status_ is anything other than
      // MINIMIZER_STATUS_NOT_YET_CONVERGED then no more iterations
      // are performed
    }
     
    if (state_up_to_date < ensure_updated_state_) {
      // The last call to calc_cost_function* was not with the state
      // vector returned to the user, and they want it to be.
      if (ensure_updated_state_ > 0) {
	// User wants at least the first derivative
	cost_function_ = optimizable.calc_cost_function_gradient(x, gradient);
      }
      else {
	// User does not need derivatives to have been computed
	cost_function_ = optimizable.calc_cost_function(x);
      }
    }

    return status_;
  }

};


// =================================================================
// Contents of settings.cpp
// =================================================================

/* settings.cpp -- View/change the overall Adept settings

    Copyright (C) 2016 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/

#include <sstream>
#include <cstring>

#include <adept/base.h>
#include <adept/settings.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_OPENBLAS_CBLAS_HEADER
#include <cblas.h>
#endif

namespace adept {

  // -------------------------------------------------------------------
  // Get compile-time settings
  // -------------------------------------------------------------------

  // Return the version of Adept at compile time
  std::string
  version()
  {
    return ADEPT_VERSION_STR;
  }

  // Return the compiler used to compile the Adept library (e.g. "g++
  // [4.3.2]" or "Microsoft Visual C++ [1800]")
  std::string
  compiler_version()
  {
#ifdef CXX
    std::string cv = CXX; // Defined in config.h
#elif defined(_MSC_VER)
    std::string cv = "Microsoft Visual C++";
#else
    std::string cv = "unknown";
#endif

#ifdef __GNUC__

#define STRINGIFY3(A,B,C) STRINGIFY(A) "." STRINGIFY(B) "." STRINGIFY(C)
#define STRINGIFY(A) #A
    cv += " [" STRINGIFY3(__GNUC__,__GNUC_MINOR__,__GNUC_PATCHLEVEL__) "]";
#undef STRINGIFY
#undef STRINGIFY3

#elif defined(_MSC_VER)

#define STRINGIFY1(A) STRINGIFY(A)
#define STRINGIFY(A) #A
    cv += " [" STRINGIFY1(_MSC_VER) "]";
#undef STRINGIFY
#undef STRINGIFY1

#endif
    return cv;
  }

  // Return the compiler flags used when compiling the Adept library
  // (e.g. "-Wall -g -O3")
  std::string
  compiler_flags()
  {
#ifdef CXXFLAGS
    return CXXFLAGS; // Defined in config.h
#else
    return "unknown";
#endif
  }

  // Return a multi-line string listing numerous aspects of the way
  // Adept has been configured.
  std::string
  configuration()
  {
    std::stringstream s;
    s << "Adept version " << adept::version() << ":\n";
    s << "  Compiled with " << adept::compiler_version() << "\n";
    s << "  Compiler flags \"" << adept::compiler_flags() << "\"\n";
#ifdef BLAS_LIBS
    if (std::strlen(BLAS_LIBS) > 2) {
      const char* blas_libs = &BLAS_LIBS[2];
      s << "  BLAS support from " << blas_libs << " library\n";
    }
    else {
      s << "  BLAS support from built-in library\n";
    }
#endif
#ifdef HAVE_OPENBLAS_CBLAS_HEADER
    s << "  Number of BLAS threads may be specified up to maximum of "
      << max_blas_threads() << "\n";
#endif
    s << "  Jacobians processed in blocks of size " 
      << ADEPT_MULTIPASS_SIZE << "\n";
    return s.str();
  }


  // -------------------------------------------------------------------
  // Get/set number of threads for array operations
  // -------------------------------------------------------------------

  // Get the maximum number of threads available for BLAS operations
  int
  max_blas_threads()
  {
#ifdef HAVE_OPENBLAS_CBLAS_HEADER
    return openblas_get_num_threads();
#else
    return 1;
#endif
  }

  // Set the maximum number of threads available for BLAS operations
  // (zero means use the maximum sensible number on the current
  // system), and return the number actually set. Note that OpenBLAS
  // uses pthreads and the Jacobian calculation uses OpenMP - this can
  // lead to inefficient behaviour so if you are computing Jacobians
  // then you may get better performance by setting the number of
  // array threads to one.
  int
  set_max_blas_threads(int n)
  {
#ifdef HAVE_OPENBLAS_CBLAS_HEADER
    openblas_set_num_threads(n);
    return openblas_get_num_threads();
#else
    return 1;
#endif
  }

  // Was the library compiled with matrix multiplication support (from
  // BLAS)?
  bool
  have_matrix_multiplication() {
#ifdef HAVE_BLAS
    return true;
#else
    return false;
#endif
  }

  // Was the library compiled with linear algebra support (e.g. inv
  // and solve from LAPACK)
  bool
  have_linear_algebra() {
#ifdef HAVE_LAPACK
    return true;
#else
    return false;
#endif
  }

} // End namespace adept


// =================================================================
// Contents of solve.cpp
// =================================================================

/* solve.cpp -- Solve systems of linear equations using LAPACK

    Copyright (C) 2015-2016 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.
*/
                             

#include <vector>

#include <adept/solve.h>
#include <adept/Array.h>
#include <adept/SpecialMatrix.h>

// If ADEPT_SOURCE_H is defined then we are in a header file generated
// from all the source files, so cpplapack.h will already have been
// included
#ifndef AdeptSource_H
#include "cpplapack.h"
#endif

#ifdef HAVE_LAPACK

namespace adept {

  using namespace internal;
  
  // -------------------------------------------------------------------
  // Solve Ax = b for general square matrix A
  // -------------------------------------------------------------------
  template <typename T>
  Array<1,T,false> 
  solve(const Array<2,T,false>& A, const Array<1,T,false>& b) {
    Array<2,T,false> A_;
    Array<1,T,false> b_;

    // LAPACKE is more efficient with column-major input
    // if (A.is_row_contiguous()) {
      A_.resize_column_major(A.dimensions());
      A_ = A;
    // }
    // else {
    //   A_.link(A);
    // }

    // if (b_.offset(0) != 0) {
      b_ = b;
    // }
    // else {
    //   b_.link(b);
    // }

    std::vector<lapack_int> ipiv(A_.dimension(0));

    //    lapack_int status = LAPACKE_dgesv(LAPACK_COL_MAJOR, A_.dimension(0), 1,
    //				      A_.data(), A_.offset(1), &ipiv[0],
    //				      b_.data(), b_.dimension(0));
    lapack_int status = cpplapack_gesv(A_.dimension(0), 1,
				       A_.data(), A_.offset(1), &ipiv[0],
				       b_.data(), b_.dimension(0));

    if (status != 0) {
      std::stringstream s;
      s << "Failed to solve general system of equations: LAPACK ?gesv returned code " << status;
      throw(matrix_ill_conditioned(s.str() ADEPT_EXCEPTION_LOCATION));
    }
    return b_;    
  }

  // -------------------------------------------------------------------
  // Solve AX = B for general square matrix A and rectangular matrix B
  // -------------------------------------------------------------------
  template <typename T>
  Array<2,T,false> 
  solve(const Array<2,T,false>& A, const Array<2,T,false>& B) {
    Array<2,T,false> A_;
    Array<2,T,false> B_;
    
    // LAPACKE is more efficient with column-major input
    // if (A.is_row_contiguous()) {
      A_.resize_column_major(A.dimensions());
      A_ = A;
    // }
    // else {
    //   A_.link(A);
    // }

    // if (B.is_row_contiguous()) {
      B_.resize_column_major(B.dimensions());
      B_ = B;
    // }
    // else {
    //   B_.link(B);
    // }

    std::vector<lapack_int> ipiv(A_.dimension(0));

    //    lapack_int status = LAPACKE_dgesv(LAPACK_COL_MAJOR, A_.dimension(0), B.dimension(1),
    //				      A_.data(), A_.offset(1), &ipiv[0],
    //				      B_.data(), B_.offset(1));
    lapack_int status = cpplapack_gesv(A_.dimension(0), B.dimension(1),
				       A_.data(), A_.offset(1), &ipiv[0],
				       B_.data(), B_.offset(1));
    if (status != 0) {
      std::stringstream s;
      s << "Failed to solve general system of equations for matrix RHS: LAPACK ?gesv returned code " << status;
      throw(matrix_ill_conditioned(s.str() ADEPT_EXCEPTION_LOCATION));
    }
    return B_;    
  }


  // -------------------------------------------------------------------
  // Solve Ax = b for symmetric square matrix A
  // -------------------------------------------------------------------
  template <typename T, SymmMatrixOrientation Orient>
  Array<1,T,false>
  solve(const SpecialMatrix<T,SymmEngine<Orient>,false>& A,
	const Array<1,T,false>& b) {
    SpecialMatrix<T,SymmEngine<Orient>,false> A_;
    Array<1,T,false> b_;

    // Not sure why the original code copies A...
    A_.resize(A.dimension());
    A_ = A;
    // A_.link(A);

    // if (b.offset(0) != 1) {
      b_ = b;
    // }
    // else {
    //   b_.link(b);
    // }

    // Treat symmetric matrix as column-major
    char uplo;
    if (Orient == ROW_LOWER_COL_UPPER) {
      uplo = 'U';
    }
    else {
      uplo = 'L';
    }

    std::vector<lapack_int> ipiv(A_.dimension());

    //    lapack_int status = LAPACKE_dsysv(LAPACK_COL_MAJOR, uplo, A_.dimension(0), 1,
    //				      A_.data(), A_.offset(), &ipiv[0],
    //				      b_.data(), b_.dimension(0));
    lapack_int status = cpplapack_sysv(uplo, A_.dimension(0), 1,
				       A_.data(), A_.offset(), &ipiv[0],
				       b_.data(), b_.dimension(0));

    if (status != 0) {
      //      std::stringstream s;
      //      s << "Failed to solve symmetric system of equations: LAPACK ?sysv returned code " << status;
      //      throw(matrix_ill_conditioned(s.str() ADEPT_EXCEPTION_LOCATION));
      std::cerr << "Warning: LAPACK solve symmetric system failed (?sysv): trying general (?gesv)\n";
      return solve(Array<2,T,false>(A_),b_);
    }
    return b_;    
  }


  // -------------------------------------------------------------------
  // Solve AX = B for symmetric square matrix A
  // -------------------------------------------------------------------
  template <typename T, SymmMatrixOrientation Orient>
  Array<2,T,false>
  solve(const SpecialMatrix<T,SymmEngine<Orient>,false>& A,
	const Array<2,T,false>& B) {
    SpecialMatrix<T,SymmEngine<Orient>,false> A_;
    Array<2,T,false> B_;

    A_.resize(A.dimension());
    A_ = A;
    // A_.link(A);

    // if (B.is_row_contiguous()) {
      B_.resize_column_major(B.dimensions());
      B_ = B;
    // }
    // else {
    //   B_.link(B);
    // }

    // Treat symmetric matrix as column-major
    char uplo;
    if (Orient == ROW_LOWER_COL_UPPER) {
      uplo = 'U';
    }
    else {
      uplo = 'L';
    }

    std::vector<lapack_int> ipiv(A_.dimension());

    //    lapack_int status = LAPACKE_dsysv(LAPACK_COL_MAJOR, uplo, A_.dimension(0), B.dimension(1),
    //				      A_.data(), A_.offset(), &ipiv[0],
    //				      B_.data(), B_.offset(1));
    lapack_int status = cpplapack_sysv(uplo, A_.dimension(0), B.dimension(1),
				       A_.data(), A_.offset(), &ipiv[0],
				       B_.data(), B_.offset(1));

    if (status != 0) {
      std::stringstream s;
      s << "Failed to solve symmetric system of equations with matrix RHS: LAPACK ?sysv returned code " << status;
      throw(matrix_ill_conditioned(s.str() ADEPT_EXCEPTION_LOCATION));
    }
    return B_;
  }

}

#else

namespace adept {
  
  using namespace internal;
  
  // -------------------------------------------------------------------
  // Solve Ax = b for general square matrix A
  // -------------------------------------------------------------------
  template <typename T>
  Array<1,T,false> 
  solve(const Array<2,T,false>& A, const Array<1,T,false>& b) {
    throw feature_not_available("Cannot solve linear equations because compiled without LAPACK");
  }

  // -------------------------------------------------------------------
  // Solve AX = B for general square matrix A and rectangular matrix B
  // -------------------------------------------------------------------
  template <typename T>
  Array<2,T,false> 
  solve(const Array<2,T,false>& A, const Array<2,T,false>& B) {
    throw feature_not_available("Cannot solve linear equations because compiled without LAPACK");
  }

  // -------------------------------------------------------------------
  // Solve Ax = b for symmetric square matrix A
  // -------------------------------------------------------------------
  template <typename T, SymmMatrixOrientation Orient>
  Array<1,T,false>
  solve(const SpecialMatrix<T,SymmEngine<Orient>,false>& A,
	const Array<1,T,false>& b) {
    throw feature_not_available("Cannot solve linear equations because compiled without LAPACK");
  }

  // -------------------------------------------------------------------
  // Solve AX = B for symmetric square matrix A
  // -------------------------------------------------------------------
  template <typename T, SymmMatrixOrientation Orient>
  Array<2,T,false>
  solve(const SpecialMatrix<T,SymmEngine<Orient>,false>& A,
	const Array<2,T,false>& B) {
    throw feature_not_available("Cannot solve linear equations because compiled without LAPACK");
  }

}

#endif


namespace adept {

  // -------------------------------------------------------------------
  // Explicit instantiations
  // -------------------------------------------------------------------
#define ADEPT_EXPLICIT_SOLVE(TYPE,RRANK)				\
  template Array<RRANK,TYPE,false>					\
  solve(const Array<2,TYPE,false>& A, const Array<RRANK,TYPE,false>& b); \
  template Array<RRANK,TYPE,false>					\
  solve(const SpecialMatrix<TYPE,SymmEngine<ROW_LOWER_COL_UPPER>,false>& A, \
	const Array<RRANK,TYPE,false>& b);					\
  template Array<RRANK,TYPE,false>					\
  solve(const SpecialMatrix<TYPE,SymmEngine<ROW_UPPER_COL_LOWER>,false>& A, \
	const Array<RRANK,TYPE,false>& b);

  ADEPT_EXPLICIT_SOLVE(float,1)
  ADEPT_EXPLICIT_SOLVE(float,2)
  ADEPT_EXPLICIT_SOLVE(double,1)
  ADEPT_EXPLICIT_SOLVE(double,2)
#undef ADEPT_EXPLICIT_SOLVE

}



// =================================================================
// Contents of vector_utilities.cpp
// =================================================================

/* vector_utilities.cpp -- Vector utility functions

    Copyright (C) 2016 European Centre for Medium-Range Weather Forecasts

    Author: Robin Hogan <r.j.hogan@ecmwf.int>

    This file is part of the Adept library.

*/

#include <adept/vector_utilities.h>

namespace adept {

  Array<1,Real,false>
  linspace(Real x1, Real x2, Index n) {
    Array<1,Real,false> ans(n);
    if (n > 1) {
      for (Index i = 0; i < n; ++i) {
	ans(i) = x1 + (x2-x1)*i / static_cast<Real>(n-1);
      }
    }
    else if (n == 1 && x1 == x2) {
      ans(0) = x1;
      return ans;
    }
    else if (n == 1) {
      throw(invalid_operation("linspace(x1,x2,n) with n=1 only valid if x1=x2"));
    }
    return ans;
  }

}



#endif

