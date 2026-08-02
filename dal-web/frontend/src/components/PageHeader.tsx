import type { ReactNode } from "react";
import { css } from "../format";

interface PageHeaderProps {
  eyebrow: string;
  title: string;
  subtitle?: string;
  children?: ReactNode;
}

// Shared page header: section eyebrow, title, optional subtitle and optional
// right-aligned controls — the same structure every page leads with.
export default function PageHeader({ eyebrow, title, subtitle, children }: PageHeaderProps) {
  return (
    <div {...css("page-header")}>
      <div>
        <span {...css("eyebrow")}>{eyebrow}</span>
        <h1>{title}</h1>
        {subtitle && <p>{subtitle}</p>}
      </div>
      {children && <div {...css("page-header-controls")}>{children}</div>}
    </div>
  );
}
