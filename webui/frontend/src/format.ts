// Small formatting helpers.

export function fmtNum(value: number, digits = 4): string {
  if (!isFinite(value)) return "-";
  return value.toLocaleString(undefined, {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits,
  });
}

export function fmtMoney(value: number): string {
  if (!isFinite(value)) return "-";
  return value.toLocaleString(undefined, {
    maximumFractionDigits: 2,
  });
}

export function classNames(...parts: (string | false | undefined)[]): string {
  return parts.filter(Boolean).join(" ");
}
