// Small formatting and styling helpers.

export function fmtNum(value: number, digits = 4): string {
  if (!Number.isFinite(value)) {
    return "-";
  }
  return value.toLocaleString(undefined, {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits,
  });
}

export function fmtMoney(value: number): string {
  if (!Number.isFinite(value)) {
    return "-";
  }
  return value.toLocaleString(undefined, {
    maximumFractionDigits: 2,
  });
}

export function classNames(...parts: (string | false | undefined)[]): string {
  return parts.filter(Boolean).join(" ");
}

// Support both single class name and multiple class names
export function css(...classNames: (string | false | undefined)[]): { className: string } {
  return { className: classNames.filter(Boolean).join(" ") };
}

export function inlineStyle(style: Record<string, string | number>): { style: Record<string, string | number> } {
  return { style };
}
