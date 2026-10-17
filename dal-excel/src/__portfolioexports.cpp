//
// Created by Codex on 2026/10/7.
//

#include "__portfoliorisk.hpp"

#include "__platform.hpp"
#include "__portfolioinput.hpp"

/*IF--------------------------------------------------------------------------
public ScriptPortfolio_New
    Seal ordered script trades with explicit original model ownership
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "ScriptPortfolio_New; name");
+argName = "trades"; Excel::ValidatePortfolioTrades(xl_trades);
trades is cell[][]
    Physical three-column table: unique trade ID, product handle, model handle
&outputs
portfolio is handle ScriptPortfolioData
    Immutable portfolio preserving original model identities before snapshots
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioWeightedRiskRequest_New
    Construct an immutable global portfolio weighted-risk request
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "PortfolioWeightedRiskRequest_New; name");
+const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get();
+argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "PortfolioWeightedRiskRequest_New");
&optional
settings is cell[][]
    Two columns: inputs, outputs, weights, report_factors and three byte budgets
&outputs
request is handle StorablePortfolioWeightedRiskRequest
    Typed immutable request; blank lists explicitly select none
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioJacobianRiskRequest_New
    Construct an immutable global portfolio attribution request
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "PortfolioJacobianRiskRequest_New; name");
+const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get();
+argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "PortfolioJacobianRiskRequest_New");
&optional
settings is cell[][]
    Two columns: inputs, outputs, report_factors, max_block_width and three byte budgets
&outputs
request is handle StorablePortfolioJacobianRiskRequest
    Typed immutable request; width defaults to one
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioMonteCarlo_ValueWithWeightedRisk
    Value a sealed portfolio and one global weighted gradient
&inputs
portfolio is handle ScriptPortfolioData
    Sealed portfolio
+xl_portfolio = Excel::ScriptScalarInput(xl_portfolio);
+argName = "n_paths"; Excel::ValidateRiskRequestPaths(xl_n_paths, "PortfolioMonteCarlo_ValueWithWeightedRisk");
n_paths is number
    Positive integer paths, excluding bool and text, at most INT_MAX
+xl_request = Excel::ScriptScalarInput(xl_request);
+xl_valuation = Excel::ScriptScalarInput(xl_valuation);
+xl_simulation = Excel::ScriptScalarInput(xl_simulation);
&optional
request is handle StorablePortfolioWeightedRiskRequest
    Blank selects every payoff with unit weights and all native columns
valuation is handle StorableScriptValuationSettings
    Blank captures current date and required history once
simulation is handle StorableMonteCarloSettings
    Blank enables native AAD; passive mode has zero risk columns
&outputs
result is handle StorablePortfolioWeightedRiskResult
    Owning completed weighted result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioMonteCarlo_ValueWithJacobianRisk
    Value separate portfolio rows and their global blocked Jacobian
&inputs
portfolio is handle ScriptPortfolioData
    Sealed portfolio
+xl_portfolio = Excel::ScriptScalarInput(xl_portfolio);
+argName = "n_paths"; Excel::ValidateRiskRequestPaths(xl_n_paths, "PortfolioMonteCarlo_ValueWithJacobianRisk");
n_paths is number
    Positive integer paths, excluding bool and text, at most INT_MAX
+xl_request = Excel::ScriptScalarInput(xl_request);
+xl_valuation = Excel::ScriptScalarInput(xl_valuation);
+xl_simulation = Excel::ScriptScalarInput(xl_simulation);
&optional
request is handle StorablePortfolioJacobianRiskRequest
    Blank selects every payoff, all native columns and width one
valuation is handle StorableScriptValuationSettings
    Blank captures current date and required history once
simulation is handle StorableMonteCarloSettings
    Blank enables native AAD; passive rows retain sharp prices and zero columns
&outputs
result is handle StorablePortfolioJacobianRiskResult
    Owning completed attribution result with actual group work
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Objective
    Extract a completed weighted portfolio objective
&inputs
result is handle Storable
    Completed weighted portfolio result; attribution results reject
&outputs
objective is number
    Weighted mean
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Values
    Extract independent selected portfolio row values and weights
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
&outputs
values is cell[][]
    Header and rows: id, label, slot, mean, weight; attribution weights are blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Jacobian
    Extract a detached global gradient or Jacobian matrix
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
+xl_reported = Excel::ScriptScalarInput(xl_reported);
+argName = "reported"; Excel::ValidateRiskRequestBoolean(xl_reported, "PortfolioRiskResult_Get_Jacobian; reported");
&optional
reported is boolean (false)
    Apply each selected input report factor once
&outputs
jacobian is cell[][]
    Risk matrix; zero columns spill one blank cell, with exact dimensions in Get_Shape
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Shape
    Extract exact risk dimensions without copying risk payloads
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
&outputs
shape is cell[][]
    One row: weighted one or attribution output count, and selected input count
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Outputs
    Extract selected or complete global portfolio output coordinates
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
+xl_complete = Excel::ScriptScalarInput(xl_complete);
+argName = "complete"; Excel::ValidateRiskRequestBoolean(xl_complete, "PortfolioRiskResult_Get_Outputs; complete");
&optional
complete is boolean (false)
    True selects the original complete output catalog
&outputs
outputs is cell[][]
    Header and rows: id, label, slot
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Inputs
    Extract selected or complete global portfolio input coordinates
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
+xl_complete = Excel::ScriptScalarInput(xl_complete);
+argName = "complete"; Excel::ValidateRiskRequestBoolean(xl_complete, "PortfolioRiskResult_Get_Inputs; complete");
&optional
complete is boolean (false)
    True selects the original complete owner/private catalog
&outputs
inputs is cell[][]
    Header and rows: id, label, family, ordinal, value, native unit, physical unit, report scale
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Execution
    Extract actual frozen portfolio group work and whole-request peaks
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
&outputs
execution is cell[][]
    Group owner/trades, dimensions, work, requested/actual widths, attempts and peaks
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Sampling
    Extract complete original group sampling definitions without valuation
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
&outputs
sampling is cell[][]
    Long form: group, sample, field, ordinal, subordinal, value; vector sizes retain empty rows
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Provenance
    Extract frozen portfolio method and valuation context
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
&outputs
provenance is cell[][]
    Two columns: field and value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Trades
    Extract original sealed trade and model-owner mapping
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
&outputs
trades is cell[][]
    Header and rows: trade, id, model_owner, model_type
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_TradeProvenance
    Extract one original trade's frozen valuation provenance
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
+argName = "trade"; Excel::ValidatePortfolioTradeOrdinal(xl_trade, "PortfolioRiskResult_Get_TradeProvenance");
trade is number
    Strict zero-based original trade ordinal
&outputs
provenance is cell[][]
    Two columns: field and value; no valuation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_History
    Extract one original trade's frozen historical observations
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
+argName = "trade"; Excel::ValidatePortfolioTradeOrdinal(xl_trade, "PortfolioRiskResult_Get_History");
trade is number
    Strict zero-based original trade ordinal
&outputs
history is cell[][]
    Header and rows: index, fixing_time, historical, value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_Product
    Extract one original trade's frozen source table
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
+argName = "trade"; Excel::ValidatePortfolioTradeOrdinal(xl_trade, "PortfolioRiskResult_Get_Product");
trade is number
    Strict zero-based original trade ordinal
&outputs
product is cell[][]
    Date or constant name, event or constant value text
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PortfolioRiskResult_Get_ModelSnapshot
    Extract one original trade's passive model JSON chunks
&inputs
result is handle Storable
    Completed weighted or attribution portfolio result
+argName = "trade"; Excel::ValidatePortfolioTradeOrdinal(xl_trade, "PortfolioRiskResult_Get_ModelSnapshot");
trade is number
    Strict zero-based original trade ordinal
&outputs
json is string[]
    Concatenate chunks without separators to recover the frozen snapshot
-IF-------------------------------------------------------------------------*/

namespace Dal {
#ifdef _WIN32
#include <dal-excel/auto/MG_PortfolioJacobianRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_PortfolioMonteCarlo_ValueWithJacobianRisk_public.inc>
#include <dal-excel/auto/MG_PortfolioMonteCarlo_ValueWithWeightedRisk_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_History_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Inputs_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Jacobian_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_ModelSnapshot_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Objective_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Outputs_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Product_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Provenance_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Sampling_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Shape_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_TradeProvenance_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Trades_public.inc>
#include <dal-excel/auto/MG_PortfolioRiskResult_Get_Values_public.inc>
#include <dal-excel/auto/MG_PortfolioWeightedRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_ScriptPortfolio_New_public.inc>
#endif
} // namespace Dal
