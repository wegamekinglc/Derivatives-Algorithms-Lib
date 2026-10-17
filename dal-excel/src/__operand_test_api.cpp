//
// Test-only wrappers around the Excel operand conversion helpers.
//

#include "__excel_test_api.hpp"

#ifdef _WIN32
#include "_xlcall.hpp"
#include "_excel.hpp"

namespace Dal {
    namespace {
        OPER_ IntCell(int value) {
            OPER_ cell{};
            cell.xltype = xltypeInt;
            cell.val.w = value;
            return cell;
        }
    } // namespace

    int IntOperandForTest(int value) {
        const auto cell = IntCell(value);
        return Excel::ToInt(&cell);
    }

    bool BoolOperandForTest(int value) {
        const auto cell = IntCell(value);
        return Excel::ToBool(&cell);
    }

    double CellIntOperandForTest(int value) {
        const auto cell = IntCell(value);
        return Cell::ToDouble(Excel::ToCell(&cell));
    }
} // namespace Dal
#endif
