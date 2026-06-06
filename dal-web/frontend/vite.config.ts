import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Vite dev server proxies /api to the FastAPI backend on :8001.
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      "/api": {
        target: "http://127.0.0.1:8001",
        changeOrigin: true,
      },
    },
  },
});
