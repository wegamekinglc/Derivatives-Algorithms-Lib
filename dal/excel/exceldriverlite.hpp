//
// Created by wegam on 2023/1/23.
// ExcelDriverLite.hpp
//
// Excel driver class. This class is for VISUALISATION only
//
// Using std::vector<double> for conveneience. The Matrix class is generic.
//
// (C) Datasim Education BV 2003 - 2017
//

#pragma once

#include <dal/platform/config.hpp>

#ifdef USE_EXCEL_REPORT

// PLEASE USE CORRECT VERSION OF OFFICE
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/excel/excelimport.hpp>
#include <dal/string/strings.hpp>

// Excel driver class definition. Contains functionality to add charts
// and matrices. Hides all COM details. COM exceptions are re-thrown
// as STL strings.
namespace Dal {
    class ExcelDriver_ {
    private:
        // Private member data.
        Excel::_ApplicationPtr xl_; // Pointer to Excel.

        int curDataColumn_;

        // Writes label and std::vector<double> to cells in horizontal direction.
        void ToSheetHorizontal(Excel::_WorksheetPtr sheet,
                               int sheetRow,
                               int sheetColumn,
                               const String_& label,
                               const Vector_<>& values);

        // Writes label and std::vector<double> to cells in vertical direction.
        void ToSheetVertical(Excel::_WorksheetPtr sheet,
                             int sheetRow,
                             int sheetColumn,
                             const String_& label,
                             const Vector_<>& values);

        // Throw COM error as string.
        void ThrowAsString(_com_error& error);

    public:
        // Constructor. Starts Excel in invisible mode.
        ExcelDriver_(int currentColumn = 1);
        ~ExcelDriver_();

        // Access to single, shared instance of ExcelDriver (Singleton pattern).
        static ExcelDriver_& Instance();

        // Create chart with a number of functions. The arguments are:
        //  x:			std::vector<double> with input values
        //  labels:		labels for output values
        //  vectorList: list of vectors with output values.
        //  chartTitle:	title of chart
        //  xTitle:		label of x axis
        //  yTitle:		label of y axis
        void CreateChart(const Vector_<>& x,
                         const Vector_<String_>& labels,
                         const Vector_<Vector_<>>& vectorList,
                         const String_& chartTitle,
                         const String_& xTitle = "X",
                         const String_& yTitle = "Y");

        // Create chart with a number of functions. The arguments are:
        //  x:			std::vector<double> with input values
        //  y:			std::vector<double> with output values.
        //  chartTitle:	title of chart
        //  xTitle:		label of x axis
        //  yTitle:		label of y axis

        void CreateChart(const Vector_<>& x,
                         const Vector_<>& y,
                         const String_& chartTitle,
                         const String_& xTitle = "X",
                         const String_& yTitle = "Y");

        void MakeVisible(bool b);

        // Matrix visualisation
        void AddMatrix(const Matrix_<>& matrix, const String_& name = String_("Matrix"), int row = 1, int col = 1);

        void AddMatrix(const Matrix_<>& matrix,
                       const String_& sheetName,
                       const Vector_<String_>& rowLabels,
                       const Vector_<String_>& columnLabels,
                       int row = 1,
                       int col = 1);

        // Vector visualisation as numbers
        void AddVector(const Vector_<>& vec,
                       const String_& sheetName = String_("Vector"),
                       int row = 1,
                       int col = 1);

        void AddVector(const Vector_<>& vec,
                       const String_& sheetName,
                       const String_& rowLabel,
                       const Vector_<String_>& columnLabels,
                       int row = 1,
                       int col = 1);

        // For debugging, for example; print string at a (row,col) position
        void PrintStringInExcel(const String_& s, int row, int col);
        void PrintStringInExcel(const Vector_<String_>& s, int row, int col);
    };
}

#endif