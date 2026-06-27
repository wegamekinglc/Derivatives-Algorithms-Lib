//
// Created by wegam on 2023/1/23.
//

#pragma once

#include <dal/platform/config.hpp>

#ifdef USE_EXCEL_REPORT

#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/io/excelimport.hpp>

namespace Dal {
    // Initializes COM for the lifetime of the enclosing object. Declared as the
    // first member of ExcelDriver_ so it is destroyed after xl_, keeping COM
    // available for every COM call issued through the driver.
    class ComInitializer_ {
    private:
        bool initialized_;

    public:
        ComInitializer_();
        ~ComInitializer_();

        ComInitializer_(const ComInitializer_&) = delete;
        ComInitializer_& operator=(const ComInitializer_&) = delete;
    };

    class ExcelDriver_ {
    private:
        // Declaration order matters: comInit_ must outlive xl_ so that COM is
        // still initialized when xl_ is released in the destructor.
        ComInitializer_ comInit_;
        Excel::_ApplicationPtr xl_;

        int curDataColumn_;

        void ToSheetHorizontal(Excel::_WorksheetPtr sheet,
                               int sheetRow,
                               int sheetColumn,
                               const String_& label,
                               const Vector_<>& values);

        void ToSheetVertical(Excel::_WorksheetPtr sheet,
                             int sheetRow,
                             int sheetColumn,
                             const String_& label,
                             const Vector_<>& values);

        void ThrowAsString(_com_error& error);

    public:
        explicit ExcelDriver_(int currentColumn = 1);
        ~ExcelDriver_();

        // Access to a per-thread instance of ExcelDriver.
        // Excel COM automation objects are thread-affine, so the driver must
        // not be shared across threads.
        static ExcelDriver_& Instance() {
            thread_local ExcelDriver_ instance;
            return instance;
        }

        void CreateChart(const Vector_<>& x,
                         const Vector_<String_>& labels,
                         const Vector_<Vector_<>>& vectorList,
                         const String_& chartTitle,
                         const String_& xTitle = "X",
                         const String_& yTitle = "Y");

        void CreateChart(const Vector_<>& x,
                         const Vector_<>& y,
                         const String_& chartTitle,
                         const String_& xTitle = "X",
                         const String_& yTitle = "Y");

        void MakeVisible(bool b);

        void AddMatrix(const Matrix_<>& matrix, const String_& name = String_("Matrix"), int row = 1, int col = 1);

        void AddMatrix(const Matrix_<>& matrix,
                       const String_& sheetName,
                       const Vector_<String_>& rowLabels,
                       const Vector_<String_>& columnLabels,
                       int row = 1,
                       int col = 1);

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

        void PrintStringInExcel(const String_& s, int row, int col);
        void PrintStringInExcel(const Vector_<String_>& s, int row, int col);
    };
} // namespace Dal

#endif
