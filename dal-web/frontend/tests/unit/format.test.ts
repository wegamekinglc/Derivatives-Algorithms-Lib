import { describe, expect, it } from "vitest";
import { classNames, css, fmtMoney, fmtNum, inlineStyle } from "../../src/format";

describe("fmtNum", () => {
  it("formats with the default four fraction digits", () => {
    expect(fmtNum(1234.5)).toBe("1,234.5000");
  });

  it("honours a custom digit count", () => {
    expect(fmtNum(0.123456, 2)).toBe("0.12");
    expect(fmtNum(8, 0)).toBe("8");
  });

  it("groups thousands", () => {
    expect(fmtNum(1_000_000, 2)).toBe("1,000,000.00");
  });

  it("renders non-finite values as a dash", () => {
    expect(fmtNum(Number.NaN)).toBe("-");
    expect(fmtNum(Number.POSITIVE_INFINITY)).toBe("-");
    expect(fmtNum(Number.NEGATIVE_INFINITY)).toBe("-");
  });

  it("keeps negative values", () => {
    expect(fmtNum(-42.5, 1)).toBe("-42.5");
  });
});

describe("fmtMoney", () => {
  it("formats with at most two fraction digits", () => {
    expect(fmtMoney(8_000_000)).toBe("8,000,000");
    expect(fmtMoney(1234.567)).toBe("1,234.57");
  });

  it("renders non-finite values as a dash", () => {
    expect(fmtMoney(Number.NaN)).toBe("-");
    expect(fmtMoney(Number.POSITIVE_INFINITY)).toBe("-");
  });
});

describe("classNames", () => {
  it("joins truthy parts with a single space", () => {
    expect(classNames("a", "b", "c")).toBe("a b c");
  });

  it("drops false and undefined parts", () => {
    expect(classNames("a", false, undefined, "b")).toBe("a b");
  });

  it("returns an empty string when nothing is truthy", () => {
    expect(classNames(false, undefined)).toBe("");
  });
});

describe("css", () => {
  it("wraps joined class names in a className prop object", () => {
    expect(css("panel")).toEqual({ className: "panel" });
    expect(css("metric", false, "pos")).toEqual({ className: "metric pos" });
  });
});

describe("inlineStyle", () => {
  it("wraps the style record in a style prop object", () => {
    const style = { marginBottom: 12, display: "flex" };
    expect(inlineStyle(style)).toEqual({ style });
  });
});
