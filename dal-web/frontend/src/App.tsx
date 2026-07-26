import { useEffect, useState } from "react";
import { NavLink, Navigate, Route, Routes } from "react-router-dom";
import { api, type Health } from "./api/client";
import { css } from "./format";
import Dashboard from "./pages/Dashboard";
import Portfolios from "./pages/Portfolios";
import Trades from "./pages/Trades";
import ProductBuilder from "./pages/ProductBuilder";
import Models from "./pages/Models";
import Valuations from "./pages/Valuations";
import Curves from "./pages/Curves";
import CurveRun from "./pages/CurveRun";

const NAV = [
  { to: "/dashboard", label: "Dashboard" },
  { to: "/portfolios", label: "Portfolios" },
  { to: "/trades", label: "Trades" },
  { to: "/products", label: "Product Builder" },
  { to: "/models", label: "Models" },
  { to: "/valuations", label: "Valuation Runs" },
  { to: "/curves", label: "Curve Lab" },
];

export default function App() {
  const [health, setHealth] = useState<Health | null>(null);

  useEffect(() => {
    void api.health().then(setHealth).catch(() => {
      setHealth(null);
    });
  }, []);

  return (
    <div {...css("app")}>
      <header {...css("topbar")}>
        <div {...css("brand")}>
          DAL Workbench
        </div>
        <nav {...css("nav")}>
          {NAV.map((n) => (
            <NavLink
              key={n.to}
              to={n.to}
              {...{
                className: ({ isActive }: { isActive: boolean }) => css("nav-link", isActive && "active").className,
              }}
            >
              {n.label}
            </NavLink>
          ))}
        </nav>
        <div {...css("status-bar")}>
          <div {...css("status-indicator")}>
            <span {...css("status-dot", health ? "online" : "offline")} />
            <span>{health ? "online" : "offline"}</span>
          </div>
          {health && (
            <>
              <div {...css("backend-badge")}>
                <span {...css(health.is_native ? "badge-native" : "badge-stub")}>
                  {health.backend}
                  {health.is_native ? " (native)" : " (stub)"}
                </span>
              </div>
              <div {...css("mono")}>
                eval: {health.evaluation_date}
              </div>
            </>
          )}
        </div>
      </header>
      <main {...css("main")}>
        <Routes>
          <Route path="/" element={<Navigate to="/dashboard" replace />} />
          <Route path="/dashboard" element={<Dashboard />} />
          <Route path="/portfolios" element={<Portfolios />} />
          <Route path="/trades" element={<Trades />} />
          <Route path="/products" element={<ProductBuilder />} />
          <Route path="/models" element={<Models />} />
          <Route path="/valuations" element={<Valuations />} />
          <Route path="/curves" element={<Curves />} />
          <Route path="/curves/runs/:runId" element={<CurveRun />} />
        </Routes>
      </main>
    </div>
  );
}
