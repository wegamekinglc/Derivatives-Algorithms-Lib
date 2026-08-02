import { describe, expect, it } from "vitest";
import {
  declarationLabel,
  formatTenor,
  instrumentDayCount,
  quoteSeries,
  quoteToPercent,
  quotesAsOf,
  stepperStates,
} from "../../src/curves/curveBuilderUtils";

describe("declarationLabel", () => {
  it("renders the component tail with a readable role", () => {
    expect(declarationLabel("clab/v1/local/discount/USD/OIS")).toBe("USD OIS · Discount");
    expect(declarationLabel("clab/v1/local/projection/USD/3M")).toBe("USD 3M · Projection");
    expect(declarationLabel("clab/v1/local/basis/USD-EUR")).toBe("USD-EUR · Basis");
  });

  it("falls back to the raw key when the shape is unexpected", () => {
    expect(declarationLabel("odd")).toBe("odd");
  });
});

describe("formatTenor", () => {
  it("labels days, months and years", () => {
    expect(formatTenor("2026-01-15", "2026-01-16")).toBe("1D");
    expect(formatTenor("2026-01-15", "2026-04-15")).toBe("3M");
    expect(formatTenor("2026-01-15", "2028-01-15")).toBe("2Y");
    expect(formatTenor("2026-01-15", "2031-01-15")).toBe("5Y");
  });

  it("returns a placeholder for unparseable or inverted ranges", () => {
    expect(formatTenor("2026-01-15", "2026-01-15")).toBe("—");
    expect(formatTenor("2026-01-15", "not-a-date")).toBe("—");
  });
});

describe("instrumentDayCount", () => {
  it("prefers the single-basis term and formats separators", () => {
    expect(instrumentDayCount({ day_basis: "ACT_365F" })).toBe("ACT/365F");
    expect(instrumentDayCount({ fixed_day_basis: "ACT_360" })).toBe("ACT/360");
  });

  it("returns a placeholder without a basis", () => {
    expect(instrumentDayCount({})).toBe("—");
    expect(instrumentDayCount(undefined)).toBe("—");
  });
});

describe("quoteToPercent", () => {
  it("converts decimals to percent and passes percent lexemes through", () => {
    expect(quoteToPercent("DEPOSIT", "0.0433")).toBeCloseTo(4.33);
    expect(quoteToPercent("IRS", "3.765")).toBeCloseTo(3.765);
  });

  it("converts futures prices to the implied rate", () => {
    expect(quoteToPercent("FUTURE", "95.8225")).toBeCloseTo(4.1775);
  });

  it("rejects unparseable quotes", () => {
    expect(quoteToPercent("OIS", "abc")).toBeNull();
  });
});

describe("quoteSeries", () => {
  const instruments = [
    {
      instrument_id: "a",
      instrument_type: "DEPOSIT",
      raw_quote: "0.0433",
      maturity_date: "2026-01-16",
      included: true,
    },
    {
      instrument_id: "b",
      instrument_type: "BASIS_SWAP",
      raw_quote: "0.001",
      maturity_date: "2028-01-15",
      included: true,
    },
    {
      instrument_id: "c",
      instrument_type: "IRS",
      raw_quote: "3.765",
      maturity_date: "2028-01-15",
      included: false,
    },
    {
      instrument_id: "d",
      instrument_type: "OIS",
      raw_quote: "4.228",
      maturity_date: "2026-04-15",
      included: true,
    },
  ];

  it("keeps included rate families sorted by tenor and skips spreads/excluded rows", () => {
    const points = quoteSeries(instruments, "2026-01-15");
    expect(points.map((point) => point.key)).toEqual(["a", "d"]);
    expect(points[0].tenor).toBe("1D");
    expect(points[1].tenor).toBe("3M");
    expect(points[1].percent).toBeCloseTo(4.228);
    expect(points.every((point) => point.normalizedPercent === null)).toBe(true);
  });

  it("joins normalized quotes from a succeeded build by instrument id", () => {
    const points = quoteSeries(instruments, "2026-01-15", [
      { instrument_id: "d", normalized_quote: "0.0423" },
    ]);
    expect(points.find((point) => point.key === "d")?.normalizedPercent).toBeCloseTo(4.23);
    expect(points.find((point) => point.key === "a")?.normalizedPercent).toBeNull();
  });

  it("returns nothing without a valid as-of date", () => {
    expect(quoteSeries(instruments, "not-a-date")).toEqual([]);
  });
});

describe("stepperStates", () => {
  const base = {
    declarationCount: 1,
    dependencyCount: 0,
    dependencyAvailable: 0,
    includedInstrumentCount: 2,
    buildState: null,
    fitState: null,
    mode: "MULTI_CURVE",
  };

  it("marks content steps done and leaves solve/validate open before any build", () => {
    expect(stepperStates(base)).toEqual({
      declaration: "done",
      dependencies: "done",
      instruments: "done",
      solve: "todo",
      validate: "todo",
    });
  });

  it("activates dependencies when versions exist but none are selected", () => {
    const states = stepperStates({ ...base, dependencyAvailable: 3 });
    expect(states.dependencies).toBe("active");
  });

  it("treats dependencies as done for single-curve builds", () => {
    const states = stepperStates({ ...base, dependencyAvailable: 3, mode: "SINGLE" });
    expect(states.dependencies).toBe("done");
  });

  it("reflects a running, succeeded and failed build", () => {
    expect(stepperStates({ ...base, buildState: "QUEUED" }).solve).toBe("running");
    expect(stepperStates({ ...base, buildState: "SUCCEEDED" }).solve).toBe("done");
    expect(stepperStates({ ...base, buildState: "FAILED" }).solve).toBe("failed");
  });

  it("completes validate only on the validated fit state", () => {
    expect(
      stepperStates({ ...base, buildState: "SUCCEEDED", fitState: "NATIVE_ARCHIVE_VALIDATED" })
        .validate,
    ).toBe("done");
    expect(stepperStates({ ...base, buildState: "SUCCEEDED", fitState: "SOLVING" }).validate)
      .toBe("todo");
  });
});

describe("quotesAsOf", () => {
  it("takes the latest observation and falls back to the as-of date", () => {
    expect(quotesAsOf([
      { observed_at: "2026-01-14T00:00:00Z" },
      { observed_at: "2026-01-15T16:00:00Z" },
    ], "2026-01-15")).toBe("2026-01-15T16:00:00Z");
    expect(quotesAsOf([{}], "2026-01-15")).toBe("2026-01-15");
  });
});
