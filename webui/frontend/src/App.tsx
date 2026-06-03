import { useEffect, useState } from "react";
import { NavLink, Navigate, Route, Routes } from "react-router-dom";
import { api, type Health } from "./api/client";
import { classNames } from "./format";
import Dashboard from "./pages/Dashboard";
import Portfolios from "./pages/Portfolios";
import Trades from "./pages/Trades";
import ProductBuilder from "./pages/ProductBuilder";
import Models from "./pages/Models";
import Valuations from "./pages/Valuations";

const NAV = [
  { to: "/dashboard", label: "Dashboard" },
  { to: "/portfolios", label: "Portfolios" },
  { to: "/trades", label: "Trades" },
  { to: "/products", label: "Product Builder" },
  { to: "/models", label: "Models" },
  { to: "/valuations", label: "Valuation Runs" },
];

const css = (className: string) => ({ className });

export default function App() {
  const [health, setHealth] = useState<Health | null>(null);

  useEffect(() => {
    void api.health().then(setHealth).catch(() => {
      setHealth(null);
    });
  }, []);

  return (
    <div {...css("app")}>
      <aside {...css("sidebar")}>
        <div {...css("brand")}>
          DAL Workbench
          <small>Derivatives Portfolio Management</small>
        </div>
        {NAV.map((n) => (
          <NavLink
            key={n.to}
            to={n.to}
            {...{
              className: ({ isActive }: { isActive: boolean }) => classNames("nav-link", isActive && "active"),
            }}
          >
            {n.label}
          </NavLink>
        ))}
        <div {...css("backend-badge")}>
          DAL backend: {" "}
          {health ? (
            <span {...css(health.is_native ? "badge-native" : "badge-stub")}>
              {health.backend}
              {health.is_native ? " (native)" : " (stub)"}
            </span>
          ) : (
            <span {...css("muted")}>offline</span>
          )}
          <br />
          eval date: {health?.evaluation_date ?? "-"}
        </div>
      </aside>
      <main {...css("main")}>
        <Routes>
          <Route path="/" element={<Navigate to="/dashboard" replace />} />
          <Route path="/dashboard" element={<Dashboard />} />
          <Route path="/portfolios" element={<Portfolios />} />
          <Route path="/trades" element={<Trades />} />
          <Route path="/products" element={<ProductBuilder />} />
          <Route path="/models" element={<Models />} />
          <Route path="/valuations" element={<Valuations />} />
        </Routes>
      </main>
    </div>
  );
}
