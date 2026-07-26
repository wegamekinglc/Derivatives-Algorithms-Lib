import type { CalibrationKind } from "../api/client";

const index = {
  spot_lag: 0,
  fixing_lag: 0,
  use_projection_curve: false,
  forecast_tenor: "P12M",
  day_basis: "ACT_365F",
  business_day_convention: "Following",
  fixing_holidays: "",
  accrual_holidays: "",
  end_of_month: false,
  collateral: "OIS",
};

const single = {
  schema_version: 1,
  name: "usd_ois_workbench",
  today: "2026-01-02",
  currency: "USD",
  declaration: {
    curve_name: "usd_ois",
    target_collateral: "OIS",
    target_tenor: null,
    calibrate_discount_curve: true,
    libor_basis: "ACT_365F",
    parameterization: "PIECEWISE_CONSTANT_FWD",
    log_df_scheme: null,
    knot_policy: "INPUT",
    knot_dates: ["2027-01-02"],
    base_curve_id: null,
    discount_curve_ids: {},
    forward_curve_ids: {},
    initial_guess_per_node: [0.03],
  },
  instruments: [{
    kind: "DEPOSIT",
    label: "USD DEP 1Y",
    trade_date: "2026-01-02",
    start: "2026-01-02",
    maturity: "2027-01-02",
    market_rate: 0.04,
    index,
  }],
  solver: {
    solve_mode: "EXACT",
    smoothing_weight: 1,
    tolerance: 1e-9,
    fit_tolerance: 1e-7,
    initial_guess: 0.03,
    max_evaluations: 200,
    max_restarts: 20,
  },
  options: {
    jacobian_mode: "ANALYTIC",
    include_jacobian: true,
    include_effective_inverse: true,
  },
};

const staged = {
  schema_version: 1,
  name: "usd_eur_staged",
  valuation_time: "2026-01-02T00:00:00",
  pair: { domestic: "USD", foreign: "EUR" },
  collateral_currency: "USD",
  fx_spot: 1.1,
  fx_forward_collateral: "OIS",
  domestic_curve_block: {
    name: "usd", currency: "USD", libor_basis: "ACT_365F",
    discount_curve_ids: { OIS: "replace_with_curve_id" }, forward_curve_ids: {},
  },
  foreign_curve_block: {
    name: "eur", currency: "EUR", libor_basis: "ACT_365F",
    discount_curve_ids: { OIS: "replace_with_curve_id" }, forward_curve_ids: {},
  },
  basis: {
    curve_name: "usd_eur_basis",
    knot_dates: ["2027-01-02"],
    instruments: [],
    initial_guess_per_node: [0],
  },
  fixings: [],
  solver: single.solver,
  options: single.options,
};

const joint = {
  schema_version: 1,
  name: "usd_eur_joint",
  valuation_time: "2026-01-02T00:00:00",
  pair: { domestic: "USD", foreign: "EUR" },
  collateral_currency: "USD",
  fx_spot: 1.1,
  domestic: { currency: "USD", libor_basis: "ACT_365F", declarations: [] },
  foreign: { currency: "EUR", libor_basis: "ACT_365F", declarations: [] },
  basis: {
    curve_name: "usd_eur_basis",
    parameterization: "PIECEWISE_CONSTANT_FWD",
    log_df_scheme: null,
    knot_dates: ["2027-01-02"],
    instruments: [],
    smoothing_weight: 1,
    initial_guess_per_node: [0],
  },
  fixings: [],
  solver: single.solver,
  options: single.options,
};

export const calibrationExamples: Record<CalibrationKind, object> = {
  single,
  xccy_staged: staged,
  xccy_joint: joint,
};
