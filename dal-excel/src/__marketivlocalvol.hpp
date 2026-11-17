#pragma once

#include "__marketivsurface.hpp"
#include <dal-public/src/marketivlocalvol.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_MARKET_LV_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_MARKET_LV_API __declspec(dllimport)
#else
#define DAL_MARKET_LV_API
#endif

namespace Dal {
    inline const char* RiskValueType(const MarketIvLocalVolModelSettings_&) { return "MarketIvLocalVolModelSettings"; }
    namespace Excel {
        template <> inline void StorableRiskValue_<MarketIvLocalVolModelSettings_>::Write(Archive::Store_&) const {
            throw MarketIvLocalVolError_("ArchiveUnsupported", "binding", "/settings", "in-process handle", "archive");
        }
    } // namespace Excel
    using StorableMarketIvLocalVolModelSettings_ = Excel::StorableRiskValue_<MarketIvLocalVolModelSettings_>;
    DAL_MARKET_LV_API void MarketIvLocalVolModelSettings_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableMarketIvLocalVolModelSettings_>*);
    DAL_MARKET_LV_API void MarketIvLocalVolModelSettings_Get_Configuration(const Handle_<StorableMarketIvLocalVolModelSettings_>&, Matrix_<Cell_>*);
    DAL_MARKET_LV_API void MarketIvLocalVolModelData_New(const String_&,
                                                         const Handle_<StorableMarketIvDupireResult_>&,
                                                         const Handle_<StorableMarketIvLocalVolModelSettings_>&,
                                                         Handle_<MarketIvLocalVolModelData_>*);
    DAL_MARKET_LV_API void MarketIvLocalVolModelData_Get_Configuration(const Handle_<MarketIvLocalVolModelData_>&, Matrix_<Cell_>*);
    DAL_MARKET_LV_API void MarketIvLocalVolModelData_Get_Summary(const Handle_<MarketIvLocalVolModelData_>&, Matrix_<Cell_>*);
    DAL_MARKET_LV_API void MarketIvLocalVolModelData_Get_Carry(const Handle_<MarketIvLocalVolModelData_>&, Matrix_<Cell_>*);
    DAL_MARKET_LV_API void MarketIvLocalVolModelData_Get_Capabilities(const Handle_<MarketIvLocalVolModelData_>&, Matrix_<Cell_>*);
    DAL_MARKET_LV_API void MarketIvLocalVolModelData_Get_Warnings(const Handle_<MarketIvLocalVolModelData_>&, Matrix_<Cell_>*);
    DAL_MARKET_LV_API void MarketIvLocalVolModelData_Get_ConstructionExecution(const Handle_<MarketIvLocalVolModelData_>&, Matrix_<Cell_>*);
    DAL_MARKET_LV_API void MarketIvLocalVolModelData_Get_Execution(const Handle_<ScriptProductData_>&,
                                                                   const Handle_<MarketIvLocalVolModelData_>&,
                                                                   const Handle_<StorableScriptValuationSettings_>&,
                                                                   const Handle_<StorableMonteCarloSettings_>&,
                                                                   Matrix_<Cell_>*);
} // namespace Dal

#undef DAL_MARKET_LV_API
