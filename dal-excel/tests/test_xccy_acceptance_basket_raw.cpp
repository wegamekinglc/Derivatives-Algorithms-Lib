#include <gtest/gtest.h>

#include <array>

#ifdef _WIN32
#define NOMINMAX
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccymontecarlobasket.hpp>
#include <dal-public/src/global.hpp>

#include <Windows.h>
#include <dal-excel/src/_xlcall.hpp>

using namespace Dal;

namespace {
    struct Text_ {
        std::wstring text_;
        OPER_ cell_{};
        explicit Text_(const std::wstring& value) : text_(1, static_cast<wchar_t>(value.size())) {
            text_ += value;
            cell_.xltype = xltypeStr;
            cell_.val.str = text_.data();
        }
        explicit Text_(const String_& value) : Text_(std::wstring(value.begin(), value.end())) {}
    };
    OPER_ Range(OPER_* cells, int rows, int columns) {
        OPER_ result{};
        result.xltype = xltypeMulti;
        result.val.array.lparray = cells;
        result.val.array.rows = rows;
        result.val.array.columns = columns;
        return result;
    }
    template <class... T_> String_ Call(const char* name, T_... arguments) {
        const auto module = GetModuleHandleA("dal_excel.xll");
        const auto address = GetProcAddress(module, name);
        REQUIRE(address, "missing generated basket export");
        const auto output = reinterpret_cast<OPER_* (*)(T_...)>(address)(arguments...);
        const auto free = reinterpret_cast<void (*)(OPER_*)>(GetProcAddress(module, "xlAutoFree12"));
        String_ result;
        if (output && (output->xltype & xltypeMulti) && output->val.array.rows == 1 && output->val.array.columns == 1) {
            const auto& cell = output->val.array.lparray[0];
            if (cell.xltype == xltypeStr && cell.val.str)
                for (int i = 1; i <= cell.val.str[0]; ++i)
                    result += static_cast<char>(cell.val.str[i]);
        }
        free(output);
        REQUIRE(!result.empty(), "expected scalar generated result");
        return result;
    }
    void Refuses(const String_& text, std::initializer_list<const char*> tokens) {
        const std::string message(text.c_str());
        ASSERT_EQ(message.substr(0, 7), "#Error:");
        for (const auto* token : tokens)
            ASSERT_NE(message.find(token), std::string::npos) << message;
    }
} // namespace

TEST(ExcelXccyAcceptanceBasketRawTest, TestGeneratedRequestChecksEntireSelectedRangeAndTypedInputs) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const Handle_<StorableCalibrationMonteCarloSampling_> sampling(
        new StorableCalibrationMonteCarloSampling_({"PseudoRandom", 3, 4, 1024, "dal.mrg32-normal-paths/1"}));
    const Handle_<StorableMarketFixingSnapshot_> fixings(
        new StorableMarketFixingSnapshot_(Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({}))));
    Text_ phase(std::wstring(L"HeldOut")), derivative(std::wstring(L"derivative_method")), none(std::wstring(L"None"));
    Text_ simulation(std::wstring(L"simulation"));
    Text_ sampleKey(Excel::ScriptTestStoreRepository(handle_cast<Storable_>(sampling)));
    Text_ fixingKey(Excel::ScriptTestStoreRepository(handle_cast<Storable_>(fixings)));
    OPER_ blank{};
    blank.xltype = xltypeNil;
    const auto call = [&](const OPER_* p, const OPER_* s, const OPER_* f, const OPER_* options) {
        return Call("xl_XccyMonteCarloBasketRequest_New", p, s, f, options);
    };
    ASSERT_NE(std::string(call(&phase.cell_, &sampleKey.cell_, &fixingKey.cell_, &blank).c_str()).substr(0, 7), "#Error:");
    std::array<OPER_, 8> cells{};
    for (auto& cell : cells)
        cell.xltype = xltypeNil;
    cells[0] = derivative.cell_;
    cells[1] = none.cell_;
    cells[6] = simulation.cell_;
    cells[7].xltype = xltypeErr;
    cells[7].val.err = xlerrNA;
    auto options = Range(cells.data(), 4, 2);
    ASSERT_NO_FATAL_FAILURE(Refuses(call(&phase.cell_, &sampleKey.cell_, &fixingKey.cell_, &options),
                                    {"XccyMonteCarloBasketRequest_New", "settings row=4 column=2", "field=simulation", "Excel type="}));
    ASSERT_NO_FATAL_FAILURE(Refuses(call(&phase.cell_, &fixingKey.cell_, &fixingKey.cell_, &blank), {"sampling (input #2)"}));
    ASSERT_NO_FATAL_FAILURE(Refuses(call(&phase.cell_, &sampleKey.cell_, &sampleKey.cell_, &blank), {"fixings (input #3)"}));
    std::array<OPER_, 2> phases{phase.cell_, phase.cell_};
    auto many = Range(phases.data(), 2, 1);
    ASSERT_NO_FATAL_FAILURE(Refuses(call(&many, &sampleKey.cell_, &fixingKey.cell_, &blank), {"phase", "expected scalar phase"}));
    options = Range(cells.data(), 1048577, 2);
    ASSERT_NO_FATAL_FAILURE(Refuses(call(&phase.cell_, &sampleKey.cell_, &fixingKey.cell_, &options), {"invalid table dimensions/storage"}));
    options = Range(cells.data(), 2, 3);
    ASSERT_NO_FATAL_FAILURE(Refuses(call(&phase.cell_, &sampleKey.cell_, &fixingKey.cell_, &options), {"wrong table column count"}));
    for (int argument = 0; argument < 3; ++argument) {
        const std::array<const OPER_*, 3> args{&phase.cell_, &sampleKey.cell_, &fixingKey.cell_};
        auto selected = args;
        selected[argument] = &blank;
        ASSERT_NO_FATAL_FAILURE(Refuses(call(selected[0], selected[1], selected[2], &blank), {"#Error:"}));
    }
}
#endif
