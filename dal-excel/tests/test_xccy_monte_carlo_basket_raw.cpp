#include <gtest/gtest.h>

#include <array>

#ifdef _WIN32
#define NOMINMAX
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccymontecarlobasket.hpp>
#include <dal-public/src/global.hpp>

#include "../../dal-cpp/tests/curve/xccygaussianmomentfixture.hpp"

#include <Windows.h>
#include <dal-excel/src/_xlcall.hpp>

using namespace Dal;

namespace {
    struct RawText_ {
        std::wstring storage_;
        OPER_ cell_{};
        explicit RawText_(const std::wstring& value) : storage_(1, static_cast<wchar_t>(value.size())) {
            storage_ += value;
            cell_.xltype = xltypeStr;
            cell_.val.str = storage_.data();
        }
        explicit RawText_(const String_& value) : RawText_(std::wstring(value.begin(), value.end())) {}
    };
    OPER_ Multi(OPER_* cells, int rows, int columns) {
        OPER_ result{};
        result.xltype = xltypeMulti;
        result.val.array.lparray = cells;
        result.val.array.rows = rows;
        result.val.array.columns = columns;
        return result;
    }
    template <class... T_> OPER_* Call(const char* name, T_... arguments) {
        const auto address = GetProcAddress(GetModuleHandleA("dal_excel.xll"), name);
        REQUIRE(address, "missing basket generated export " + String_(name));
        return reinterpret_cast<OPER_* (*)(T_...)>(address)(arguments...);
    }
    struct Output_ {
        OPER_* value_;
        explicit Output_(OPER_* value) : value_(value) {}
        ~Output_() {
            const auto free = reinterpret_cast<void (*)(OPER_*)>(GetProcAddress(GetModuleHandleA("dal_excel.xll"), "xlAutoFree12"));
            free(value_);
        }
        String_ Text() const {
            REQUIRE((value_->xltype & xltypeMulti) && value_->val.array.rows == 1 && value_->val.array.columns == 1, "expected scalar output");
            const auto& cell = *value_->val.array.lparray;
            REQUIRE(cell.xltype == xltypeStr && cell.val.str, "expected text output");
            String_ result;
            for (int i = 1; i <= cell.val.str[0]; ++i)
                result += static_cast<char>(cell.val.str[i]);
            return result;
        }
    };
    void Error(const Output_& output, std::initializer_list<const char*> tokens) {
        const auto message = std::string(output.Text().c_str());
        ASSERT_EQ(message.substr(0, 7), "#Error:");
        for (const auto* token : tokens)
            ASSERT_NE(message.find(token), std::string::npos) << message;
    }
} // namespace

TEST(ExcelXccyMonteCarloBasketRawTest, TestGeneratedMembersReadEntireRangeBeforeErrorCoercion) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    RawText_ id(std::wstring(L"member_id")), instrument(std::wstring(L"instrument")), measure(std::wstring(L"measure_id"));
    RawText_ first(std::wstring(L"A")), second(std::wstring(L"a")), price(std::wstring(L"Price"));
    const auto contract = TestXccyMoments::Instrument();
    RawText_ key(Excel::ScriptTestStoreRepository(handle_cast<Storable_>(contract)));
    std::array<OPER_, 12> cells{};
    for (auto& cell : cells)
        cell.xltype = xltypeNil;
    cells[0] = id.cell_;
    cells[1] = instrument.cell_;
    cells[2] = measure.cell_;
    cells[3] = first.cell_;
    cells[4] = key.cell_;
    cells[5] = price.cell_;
    cells[9] = second.cell_;
    cells[10] = key.cell_;
    cells[11] = price.cell_;
    auto raw = Multi(cells.data(), 4, 3);
    cells[11].xltype = xltypeErr;
    cells[11].val.err = xlerrNA;
    const Output_ error(Call("xl_XccyMonteCarloBasketMembers_New", &raw));
    ASSERT_NO_FATAL_FAILURE(Error(error, {"XccyMonteCarloBasketMembers_New", "members row=4 column=3", "measure_id", "Excel type="}));
    cells[11] = price.cell_;
    const Output_ blankRow(Call("xl_XccyMonteCarloBasketMembers_New", &raw));
    ASSERT_NO_FATAL_FAILURE(Error(blankRow, {"members row=3 column=1"}));
    raw = Multi(cells.data(), 2, 3);
    const Output_ accepted(Call("xl_XccyMonteCarloBasketMembers_New", &raw));
    ASSERT_NE(std::string(accepted.Text().c_str()).substr(0, 7), "#Error:");
    cells[5].xltype = xltypeBool;
    cells[5].val.xbool = 1;
    const Output_ boolean(Call("xl_XccyMonteCarloBasketMembers_New", &raw));
    ASSERT_NO_FATAL_FAILURE(Error(boolean, {"members row=2 column=3", "expected text"}));
}

TEST(ExcelXccyMonteCarloBasketRawTest, TestGeneratedRequestTypedHandlesAndPrepareEvaluate) {
    InitGlobalData();
    Excel::ScriptTestInitialize(1);
    const auto spec = TestXccyMoments::ModelSpec();
    const Handle_<StorableCalibrationMonteCarloSampling_> sampling(
        new StorableCalibrationMonteCarloSampling_({"PseudoRandom", 3, 4, 1024, "dal.mrg32-normal-paths/1"}));
    const Handle_<StorableMarketFixingSnapshot_> fixings(
        new StorableMarketFixingSnapshot_(Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({}))));
    const Handle_<StorableXccyMonteCarloBasketMembers_> members(
        new StorableXccyMonteCarloBasketMembers_({{"one", TestXccyMoments::Instrument(), "Price"}}));
    RawText_ phase(std::wstring(L"HeldOut"));
    RawText_ samplingKey(Excel::ScriptTestStoreRepository(handle_cast<Storable_>(sampling)));
    RawText_ fixingsKey(Excel::ScriptTestStoreRepository(handle_cast<Storable_>(fixings)));
    RawText_ membersKey(Excel::ScriptTestStoreRepository(handle_cast<Storable_>(members)));
    RawText_ modelKey(Excel::ScriptTestStoreRepository(handle_cast<Storable_>(NewXccyGaussianModel("raw", spec))));
    OPER_ blank{};
    blank.xltype = xltypeNil;
    const Output_ request(Call("xl_XccyMonteCarloBasketRequest_New", &phase.cell_, &samplingKey.cell_, &fixingsKey.cell_, &blank));
    RawText_ requestKey(request.Text());
    const Output_ preparedOutcome(Call("xl_XccyBasket_PrepareMonteCarlo", &membersKey.cell_, &modelKey.cell_, &requestKey.cell_));
    RawText_ preparedOutcomeKey(preparedOutcome.Text());
    const Output_ status(Call("xl_XccyMonteCarloBasketOutcome_Get_Status", &preparedOutcomeKey.cell_));
    ASSERT_EQ(status.Text(), "Prepared");
    const Output_ prepared(Call("xl_XccyMonteCarloBasketOutcome_Get_Prepared", &preparedOutcomeKey.cell_));
    RawText_ preparedKey(prepared.Text());
    const Output_ evaluated(Call("xl_XccyBasket_EvaluateMonteCarlo", &preparedKey.cell_, &blank));
    RawText_ evaluatedKey(evaluated.Text());
    const Output_ estimate(Call("xl_XccyMonteCarloBasketOutcome_Get_Status", &evaluatedKey.cell_));
    ASSERT_EQ(estimate.Text(), "Estimate");
    const Output_ noPartial(Call("xl_XccyMonteCarloBasketOutcome_Get_Partial", &evaluatedKey.cell_));
    ASSERT_NO_FATAL_FAILURE(Error(noPartial, {"CalibrationCandidateUnavailable", "/outcome/partial"}));
    const Output_ wrongType(Call("xl_XccyMonteCarloBasketRequest_New", &phase.cell_, &fixingsKey.cell_, &fixingsKey.cell_, &blank));
    ASSERT_NO_FATAL_FAILURE(Error(wrongType, {"sampling", "wrong type"}));
    phase.cell_.xltype = xltypeBool;
    phase.cell_.val.xbool = 1;
    const Output_ badPhase(Call("xl_XccyMonteCarloBasketRequest_New", &phase.cell_, &samplingKey.cell_, &fixingsKey.cell_, &blank));
    ASSERT_NO_FATAL_FAILURE(Error(badPhase, {"phase", "expected text"}));
}
#endif
