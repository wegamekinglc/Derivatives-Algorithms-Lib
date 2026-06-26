//
// Created by wegam on 2023/1/23.
//

#include <dal/platform/config.hpp>

#ifdef USE_EXCEL_REPORT
#include <dal/io/exceldriverlite.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/io/excelimport.hpp>


namespace Dal {

    void ExcelDriver_::ToSheetHorizontal(Excel::_WorksheetPtr sheet,
                                         int sheetRow,
                                         int sheetColumn,
                                         const String_& label,
                                         const Vector_<>& values) {
        Excel::RangePtr pRange = sheet->Cells;

        Excel::RangePtr item = pRange->Item[sheetRow][sheetColumn];
        item->Value2 = label.c_str();

        sheetColumn++;

        for (std::size_t i = 0; i < values.size(); ++i) {
            Excel::RangePtr item = pRange->Item[sheetRow][sheetColumn];
            item->Value2 = values[i];

            sheetColumn++;
        }
    }

    void ExcelDriver_::ToSheetVertical(Excel::_WorksheetPtr sheet,
                                       int sheetRow,
                                       int sheetColumn,
                                       const String_& label,
                                       const Vector_<>& values) {
        Excel::RangePtr pRange = sheet->Cells;

        Excel::RangePtr item = pRange->Item[sheetRow][sheetColumn];
        item->Value2 = label.c_str();

        sheetRow++;

        for (std::size_t i = 0; i < values.size(); ++i) {
            Excel::RangePtr item = pRange->Item[sheetRow][sheetColumn];
            item->Value2 = values[i];
            sheetRow++;
        }
    }

    void ExcelDriver_::ThrowAsString(_com_error& error) {
        bstr_t description = error.Description();
        if (!description) {
            description = error.ErrorMessage();
        }
        THROW(String_(description));
    }

    class ComInitializer_ {
    private:
        bool initialized_;
    public:
        ComInitializer_() : initialized_(false) {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (SUCCEEDED(hr)) {
                initialized_ = true;
            } else if (hr != S_FALSE) {
                // S_FALSE means COM is already initialized on this thread, which is OK
                THROW(String_("CoInitializeEx failed"));
            }
        }

        ~ComInitializer_() {
            if (initialized_) {
                CoUninitialize();
            }
        }

        ComInitializer_(const ComInitializer_&) = delete;
        ComInitializer_& operator=(const ComInitializer_&) = delete;
    };

    ExcelDriver_::ExcelDriver_(int currentColumn) : curDataColumn_(currentColumn) {
        ComInitializer_ comInit;

        try {
            xl_.CreateInstance(L"Excel.Application");
            xl_->Workbooks->Add((long)Excel::xlWorksheet);

            Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
            Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->GetItem(1);
            pSheet->Name = "Chart Data";
        } catch (_com_error& error) {
            ThrowAsString(error);
        }
    }

    ExcelDriver_::~ExcelDriver_() {
        try {
            if (xl_) {
                Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
                if (pWorkbook) {
                    pWorkbook->Close(VARIANT_FALSE);
                }
                xl_->Quit();
                xl_ = nullptr;
            }
        } catch (_com_error&) {
            // Destructors must not throw; continue with COM uninitialization.
        } catch (...) {
            // Destructors must not throw; continue with COM uninitialization.
        }
        // COM is automatically uninitialized when ComInitializer_ is destroyed during static destruction
    }


    void ExcelDriver_::CreateChart(const Vector_<>& x,
                                   const Vector_<String_>& labels,
                                   const Vector_<Vector_<>>& vectorList,
                                   const String_& chartTitle,
                                   const String_& xTitle,
                                   const String_& yTitle) {
        try {
            if (labels.size() != vectorList.size()) {
                THROW(String_("CreateChart error: labels.size() must equal vectorList.size()"));
            }
            for (size_t i = 0; i < vectorList.size(); ++i) {
                if (vectorList[i].size() != x.size()) {
                    THROW(String_("CreateChart error: each series must have the same length as x"));
                }
            }

            Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
            Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->GetItem("Chart Data");

            int beginRow = 1;
            int beginColumn = curDataColumn_;
            int endRow = x.size() + 1;
            int endColumn = beginColumn + vectorList.size();

            ToSheetVertical(pSheet, 1, curDataColumn_, xTitle, x);
            curDataColumn_++;

            auto labelIt = labels.begin();
            for (auto vectorIt = vectorList.begin(); vectorIt != vectorList.end(); ++vectorIt) {
                String_ label = *labelIt;
                ToSheetVertical(pSheet, 1, curDataColumn_, label, *vectorIt);

                curDataColumn_++;
                ++labelIt;
            }

            Excel::RangePtr pBeginRange = pSheet->Cells->Item[beginRow][beginColumn];
            Excel::RangePtr pEndRange = pSheet->Cells->Item[endRow][endColumn];
            Excel::RangePtr pTotalRange = pSheet->Range[static_cast<Excel::Range*>(pBeginRange)][static_cast<Excel::Range*>(pEndRange)];

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

    void ExcelDriver_::CreateChart(const Vector_<>& x,
                                  const Vector_<>& y,
                                  const String_& chartTitle,
                                  const String_& xTitle,
                                  const String_& yTitle) {
        Vector_<Vector_<>> functions = {y};
        Vector_<String_> labels = {chartTitle};

        CreateChart(x, labels, functions, chartTitle, xTitle, yTitle);
    }

    void ExcelDriver_::MakeVisible(bool b) {
        xl_->Visible = b ? VARIANT_TRUE : VARIANT_FALSE;
    }

    void ExcelDriver_::AddMatrix(const Matrix_<>& matrix,
                                 const String_& sheetName,
                                 const Vector_<String_>& rowLabels,
                                 const Vector_<String_>& columnLabels,
                                 int row,
                                 int col) {
        if (rowLabels.size() != matrix.Rows()) {
            THROW(String_("AddMatrix error: rowLabels.size() must equal matrix.Rows()"));
        }
        if (columnLabels.size() != matrix.Cols()) {
            THROW(String_("AddMatrix error: columnLabels.size() must equal matrix.Cols()"));
        }

        Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
        Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->Add();
        pSheet->Name = sheetName.c_str();

        Excel::RangePtr pRange = pSheet->Cells;

        long col2 = col + 1;
        for (auto columnLabelIt = columnLabels.begin(); columnLabelIt != columnLabels.end(); ++columnLabelIt) {
            pRange->Item[row][col2] = columnLabelIt->c_str();
            ++col2;
        }

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
        Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
        Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->Add();
        pSheet->Name = name.c_str();

        Vector_<String_> rowLabels;
        for (std::size_t r2 = 0; r2 < matrix.Rows(); ++r2) {
            rowLabels.push_back(String_());
        }

        auto rowLabelIt = rowLabels.begin();
        for (std::size_t r = 0; r < matrix.Rows(); ++r) {
            Vector_<> rowArray = matrix.Row(r);
            ToSheetHorizontal(pSheet, row, col, *rowLabelIt, rowArray);
            ++rowLabelIt;
            ++row;
        }
    }

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

    void ExcelDriver_::PrintStringInExcel(const String_& s, int row, int col) {
        Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
        Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->Add();

        Excel::RangePtr pRange = pSheet->Cells;
        pRange->Item[row][col] = s.c_str();
    }

    void ExcelDriver_::PrintStringInExcel(const Vector_<String_>& s, int row, int col) {
        Excel::_WorkbookPtr pWorkbook = xl_->ActiveWorkbook;
        Excel::_WorksheetPtr pSheet = pWorkbook->Worksheets->Add();

        Excel::RangePtr pRange = pSheet->Cells;

        for (auto it = s.begin(); it != s.end(); ++it) {
            pRange->Item[row][col] = (*it).c_str();
            ++row;
        }
    }
} // namespace Dal

#endif
