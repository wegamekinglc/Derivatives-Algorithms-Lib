//
// Created by wegam on 2023/1/23.
//

#include <dal/platform/config.hpp>

#ifdef USE_EXCEL_REPORT
//#include <dal/platform/strict.hpp>
#include <dal/report/exceldriverlite.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/report/excelimport.hpp>
#include <stdexcept>


namespace Dal {

    void ExcelDriver_::ToSheetHorizontal(Excel::_WorksheetPtr sheet,
                                         int sheetRow,
                                         int sheetColumn,
                                         const String_& label,
                                         const Vector_<>& values) {
        // Get cells.
        Excel::RangePtr pRange = sheet->Cells;

        // First cell contains the label.
        Excel::RangePtr item = pRange->Item[sheetRow][sheetColumn];
        //	item->Value = label.c_str();
        item->Value2 = label.c_str();

        sheetColumn++;

        // Next cells contain the values.
        for (std::size_t i = 0; i < values.size(); i++) {
            Excel::RangePtr item = pRange->Item[sheetRow][sheetColumn];
            item->Value2 = values[i];

            sheetColumn++;
        }
    }

    // Writes label and vector to cells in vertical direction.

    void ExcelDriver_::ToSheetVertical(Excel::_WorksheetPtr sheet,
                                       int sheetRow,
                                       int sheetColumn,
                                       const String_& label,
                                       const Vector_<>& values) {
        // Get cells.
        Excel::RangePtr pRange = sheet->Cells;

        // First cell contains the label.
        Excel::RangePtr item = pRange->Item[sheetRow][sheetColumn];
        // item->Value = label.c_str();
        item->Value2 = label.c_str();

        sheetRow++;

        // Next cells contain the values.
        for (std::size_t i = 0; i < values.size(); i++) {
            Excel::RangePtr item = pRange->Item[sheetRow][sheetColumn];
            // item->Value = values[i];
            item->Value2 = values[i];
            sheetRow++;
        }
    }

    // Throw COM error as string.

    void ExcelDriver_::ThrowAsString(_com_error& error) {
        bstr_t description = error.Description();
        if (!description) {
            description = error.ErrorMessage();
        }
        THROW(String_(description));
    }

    // Constructor. Starts Excel in invisible mode.

    ExcelDriver_::ExcelDriver_(int currentColumn) : curDataColumn_(currentColumn) {
        try {
            // Initialize COM Runtime Libraries.
            CoInitialize(NULL);

            // Start excel application.
            xl_.CreateInstance(L"Excel.Application");
            xl_->Workbooks->Add((long)Excel::xlWorksheet);

            // Rename "Sheet1" to "Chart Data".
            Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
            Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->GetItem(1);
            pSheet->Name = "Chart Data";
        } catch (_com_error& error) {
            ThrowAsString(error);
        }
    }

    // Destructor.

    ExcelDriver_::~ExcelDriver_() {
        // Undo initialization of COM Runtime Libraries.
        CoUninitialize();
    }

    // Access to single, shared instance of ExcelDriver (singleton).

    ExcelDriver_& ExcelDriver_::Instance() {
        static ExcelDriver_ singleton;
        return singleton;
    }

    // Create chart with a number of functions. The arguments are:
    //  x:			vector with input values
    //  labels:		labels for output values
    //  vectorList: list of vectors with output values.
    //  chartTitle:	title of chart
    //  xTitle:		label of x axis
    //  yTitle:		label of y axis
    void ExcelDriver_::CreateChart(const Vector_<>& x,
                                   const Vector_<String_>& labels,
                                   const Vector_<Vector_<>>& vectorList,
                                   const String_& chartTitle,
                                   const String_& xTitle,
                                   const String_& yTitle) {
        try {
            // Activate sheet with numbers.
            Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
            Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->GetItem("Chart Data");

            // Calculate range with source data.
            // The first row contains the labels shown in the chart's legend.
            int beginRow = 1;
            int beginColumn = curDataColumn_;
            int endRow = x.size() + 1;                       // +1 to include labels.
            int endColumn = beginColumn + vectorList.size(); // 1st is input, rest is output.

            // Write label + input values to cells in vertical direction.
            ToSheetVertical(pSheet, 1, curDataColumn_, xTitle, x); // X values.
            curDataColumn_++;

            // Write label + output values to cells in vertical direction.
            auto labelIt = labels.begin();
            for (auto vectorIt = vectorList.begin(); vectorIt != vectorList.end(); vectorIt++) {
                // Get label and row index.
                String_ label = *labelIt;

                // Add label + output values to Excel.
                ToSheetVertical(pSheet, 1, curDataColumn_, label, *vectorIt);

                // Advance row and label.
                curDataColumn_++;
                ++labelIt;
            }

            // Create range objects for source data.
            Excel::RangePtr pBeginRange = pSheet->Cells->Item[beginRow][beginColumn];
            Excel::RangePtr pEndRange = pSheet->Cells->Item[endRow][endColumn];
            Excel::RangePtr pTotalRange = pSheet->Range[static_cast<Excel::Range*>(pBeginRange)][static_cast<Excel::Range*>(pEndRange)];

            // Create the chart and sets its type
            Excel::_ChartPtr pChart = xl_->ActiveWorkbook->Charts->Add();
            pChart->ChartWizard(static_cast<Excel::Range*>(pTotalRange), (long)Excel::xlXYScatter, 6L, (long)Excel::xlColumns, 1L,
                                1L, true, chartTitle.c_str(), xTitle.c_str(), yTitle.c_str());
            pChart->ApplyCustomType(Excel::xlXYScatterSmooth);
            pChart->Name = chartTitle.c_str();

            // Make all titles visible again. They were erased by the ApplyCustomType method.
            pChart->HasTitle = true;
            pChart->ChartTitle->Text = chartTitle.c_str();

            Excel::AxisPtr pAxis = pChart->Axes((long)Excel::xlValue, Excel::xlPrimary);
            pAxis->HasTitle = true;
            pAxis->AxisTitle->Text = yTitle.c_str();

            pAxis = pChart->Axes((long)Excel::xlCategory, Excel::xlPrimary);
            pAxis->HasTitle = true;
            pAxis->AxisTitle->Text = xTitle.c_str();

            // Add extra row space to make sheet more readable.
            curDataColumn_++;
        } catch (_com_error& error) {
            ThrowAsString(error);
        }
    }

    // Create chart with a number of functions. The arguments are:
    //  x:			vector with input values
    //  y:			vector with output values.
    //  chartTitle:	title of chart
    //  xTitle:		label of x axis
    //  yTitle:		label of y axis
    void ExcelDriver_::CreateChart(const Vector_<>& x,
                                  const Vector_<>& y,
                                  const String_& chartTitle,
                                  const String_& xTitle,
                                  const String_& yTitle) {
        // Create list with single function and single label.
        Vector_<Vector_<>> functions = {y};
        Vector_<String_> labels = {chartTitle};

        CreateChart(x, labels, functions, chartTitle, xTitle, yTitle);
    }

    // Make Excel window visible.

    void ExcelDriver_::MakeVisible(bool b) {
        // Make excel visible.
        xl_->Visible = b ? VARIANT_TRUE : VARIANT_FALSE;
    }

    void ExcelDriver_::AddMatrix(const Matrix_<>& matrix,
                                 const String_& sheetName,
                                 const Vector_<String_>& rowLabels,
                                 const Vector_<String_>& columnLabels,
                                 int row,
                                 int col) {
        // Add sheet.
        Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
        Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->Add();
        pSheet->Name = sheetName.c_str();

        // Add column labels starting at column col.
        Excel::RangePtr pRange = pSheet->Cells;

        long col2 = col + 1;
        for (auto columnLabelIt = columnLabels.begin(); columnLabelIt != columnLabels.end(); columnLabelIt++) {
            pRange->Item[row][col2] = columnLabelIt->c_str();
            ++col2;
        }

        // Add row labels + values.
        row++;
        auto rowLabelIt = rowLabels.begin();
        for (long r = 0; r < matrix.Rows(); ++r) {
            Vector_<> rowArray = matrix.Row(r);
            ToSheetHorizontal(pSheet, row, col, *rowLabelIt, rowArray);
            ++rowLabelIt;
            ++row;
        }
    }

    void ExcelDriver_::AddMatrix(const Matrix_<>& matrix, const String_& name, int row, int col) {

        // Add sheet.
        Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
        Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->Add();
        pSheet->Name = name.c_str();

        // Make empty row labels. Can later make it more general
        Vector_<String_> rowLabels;
        for (std::size_t r2 = 0; r2 < matrix.Rows(); ++r2) {
            rowLabels.push_back(String_(""));
        }

        // Add row labels + values.
        auto rowLabelIt = rowLabels.begin();
        for (std::size_t r = 0; r < matrix.Rows(); ++r) {
            Vector_<> rowArray = matrix.Row(r);
            ToSheetHorizontal(pSheet, row, col, *rowLabelIt, rowArray);
            ++rowLabelIt;
            ++row;
        }
    }

    // Vector visualisation
    void ExcelDriver_::AddVector(const Vector_<>& vec, const String_& name, int row, int col) {

        Matrix_<> m = Matrix::FromVectors(Vector_<Vector_<>>(1, vec));
        AddMatrix(m, name, row, col);
    }

    void ExcelDriver_::AddVector(const Vector_<>& vec,
                                 const String_& sheetName,
                                 const String_& rowLabel,
                                 const Vector_<String_>& columnLabels,
                                 int row,
                                 int col) {
        Matrix_<> m = Matrix::FromVectors(Vector_<Vector_<>>(1, vec));
        Vector_<String_> rowLabels;
        rowLabels.push_back(rowLabel);
        AddMatrix(m, sheetName, rowLabels, columnLabels, row, col);
    }

    // For debugging, for example

    void ExcelDriver_::PrintStringInExcel(const String_& s, int row, int col) {
        // Add sheet.
        Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
        Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->Add();

        // Add properties to cells.
        Excel::RangePtr pRange = pSheet->Cells;
        pRange->Item[row][col] = s.c_str();
    }

    void ExcelDriver_::PrintStringInExcel(const Vector_<String_>& s, int row, int col) {

        // Add sheet.
        Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
        Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->Add();

        Excel::RangePtr pRange = pSheet->Cells;

        for (auto it = s.begin(); it != s.end(); it++) {
            pRange->Item[row][col] = (*it).c_str();
            ++row;
        }
    }
}

#endif