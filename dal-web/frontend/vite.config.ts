import { defineConfig } from "vitest/config";
import react from "@vitejs/plugin-react";

const frontendPort = Number(process.env.DAL_PLAYWRIGHT_FRONTEND_PORT ?? "5173");
const backendPort = Number(process.env.DAL_PLAYWRIGHT_BACKEND_PORT ?? "8001");

// Vite dev server proxies /api to the FastAPI backend.
export default defineConfig({
  plugins: [react()],
  server: {
    port: frontendPort,
    proxy: {
      "/api": {
        target: `http://127.0.0.1:${backendPort}`,
        changeOrigin: true,
      },
    },
  },
  test: {
    environment: "jsdom",
    include: ["tests/unit/**/*.test.{ts,tsx}"],
    setupFiles: ["tests/unit/setup.ts"],
    restoreMocks: true,
  },
});
