import { defineConfig, devices } from "@playwright/test";
import { accessSync, constants as fsConstants, existsSync, readdirSync, statSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const BASE_URL = "http://localhost:5173";
const testBackendFlag = process.env.DAL_PLAYWRIGHT_TEST_BACKEND;

if (testBackendFlag !== undefined && testBackendFlag !== "0" && testBackendFlag !== "1") {
  throw new Error("DAL_PLAYWRIGHT_TEST_BACKEND must be either 0 or 1");
}

const useTestBackend = testBackendFlag === "1";

const frontendDir = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(frontendDir, "..", "..");

// Playwright's bundled browsers are not available on every Linux distro, so
// `dal-web/scripts/setup-playwright.sh` downloads Chrome into `<repo>/chrome`
// and extracts its NSS runtime libraries into `<repo>/chrome-libs/extract`.
const chromeDir = resolve(repoRoot, "chrome");
const chromeLibBase = resolve(repoRoot, "chrome-libs", "extract", "usr", "lib");

function findChromeLibDir(): string | null {
  if (!existsSync(chromeLibBase)) return null;
  const candidates = existsSync(chromeLibBase)
    ? readdirSync(chromeLibBase, { withFileTypes: true })
        .filter((entry) => entry.isDirectory() && entry.name.endsWith("-linux-gnu"))
        .map((entry) => entry.name)
    : [];
  // Prefer the first discovered multiarch dir; x86_64-linux-gnu is the most
  // common and a reasonable fallback.
  return candidates.length > 0
    ? resolve(chromeLibBase, candidates[0])
    : null;
}

const chromeLibDir = findChromeLibDir();

// Locate the Chrome binary downloaded by setup-playwright.sh. The download
// creates a versioned directory such as `chrome/linux-149.0.7827.115/...`, so
// pick the highest version available. `PLAYWRIGHT_CHROME_PATH` overrides this.
function resolveChromeExecutable(): string {
  const override = process.env.PLAYWRIGHT_CHROME_PATH;
  if (override) {
    if (!existsSync(override)) {
      throw new Error(
        `PLAYWRIGHT_CHROME_PATH points to a non-existent file: ${override}`
      );
    }
    const overrideStat = statSync(override);
    if (!overrideStat.isFile()) {
      throw new Error(
        `PLAYWRIGHT_CHROME_PATH is not a regular file: ${override}`
      );
    }
    try {
      accessSync(override, fsConstants.X_OK);
    } catch {
      throw new Error(
        `PLAYWRIGHT_CHROME_PATH is not executable: ${override}`
      );
    }
    return override;
  }

  const candidates = existsSync(chromeDir)
    ? readdirSync(chromeDir, { withFileTypes: true })
        .filter((entry) => entry.isDirectory() && entry.name.startsWith("linux-"))
        .map((entry) => resolve(chromeDir, entry.name, "chrome-linux64", "chrome"))
        .filter((path) => existsSync(path))
        .sort((a, b) => a.localeCompare(b, undefined, { numeric: true }))
    : [];

  const executable = candidates.at(-1);
  if (!executable) {
    throw new Error(
      `No Chrome binary found under ${chromeDir}. Run ./dal-web/scripts/setup-playwright.sh first.`
    );
  }

  return executable;
}

// Prepend the extracted NSS libraries so Chrome can load libnspr4/libnss3.
function chromeLibraryPath(): string {
  const parts: string[] = [];
  if (chromeLibDir && existsSync(chromeLibDir)) {
    parts.push(chromeLibDir);
  }
  if (process.env.LD_LIBRARY_PATH) {
    parts.push(process.env.LD_LIBRARY_PATH);
  }
  return parts.join(":");
}

export default defineConfig({
  testDir: "./tests/e2e",
  fullyParallel: false,
  retries: process.env.CI ? 1 : 0,
  reporter: process.env.CI ? "github" : "line",
  use: {
    baseURL: BASE_URL,
    trace: "on-first-retry",
    launchOptions: {
      executablePath: resolveChromeExecutable(),
      env: {
        ...process.env,
        ...((): Record<string, string> => {
          const libPath = chromeLibraryPath();
          return libPath ? { LD_LIBRARY_PATH: libPath } : {};
        })(),
      },
    },
  },
  webServer: {
    command: useTestBackend
      ? "../scripts/playwright-test-backend-start.sh"
      : "../scripts/playwright-start.sh",
    url: BASE_URL,
    reuseExistingServer: !process.env.CI && !useTestBackend,
    timeout: 180_000,
  },
  projects: [
    {
      name: "chromium",
      use: { ...devices["Desktop Chrome"] },
    },
  ],
});
