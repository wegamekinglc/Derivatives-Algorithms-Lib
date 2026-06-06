# DAL Web UI Design System

## Overview

The DAL (Derivatives Algorithms Library) Web UI has been redesigned with an **"Industrial Terminal"** aesthetic, inspired by Bloomberg terminals, trading desk interfaces, and quantitative finance control rooms.

## Design Philosophy

**Data-dense, technical, refined** — an interface that conveys precision, reliability, and quantitative rigor. Every design decision serves the purpose of making complex financial data scannable and actionable.

---

## Visual Identity

### Color Palette

| Token | Hex | Usage |
|-------|-----|-------|
| `--bg` | `#0a0e1a` | Base background (deep navy) |
| `--bg-2` | `#0f1420` | Secondary background (cards, panels) |
| `--bg-3` | `#151b2e` | Tertiary background (table headers) |
| `--bg-elevated` | `#1a2138` | Elevated elements (dropdowns, modals) |
| `--border` | `#2a3350` | Primary borders |
| `--border-subtle` | `#1e2640` | Secondary borders |
| `--text` | `#e8f0ff` | Primary text (cool white) |
| `--text-dim` | `#8b97b8` | Secondary text |
| `--text-muted` | `#5a6585` | Tertiary text |
| `--accent` | `#00d9ff` | Primary accent (electric cyan) |
| `--accent-2` | `#0099cc` | Secondary accent |
| `--accent-glow` | `rgba(0, 217, 255, 0.15)` | Glow effects |
| `--green` | `#00ff88` | Positive values, success states |
| `--red` | `#ff4466` | Negative values, error states |
| `--amber` | `#ffa500` | Warnings, running states |

**Rationale:** Deep navy backgrounds reduce eye strain during extended use. Electric cyan accents provide high contrast without the visual fatigue of pure white. Green/red/amber follow financial conventions for positive/negative/warning states.

### Typography

**Primary font:** `DM Sans` (UI text, labels, buttons)
- Clean, geometric sans-serif
- Excellent readability at small sizes
- Professional but not sterile

**Monospace font:** `JetBrains Mono` (data, numbers, code)
- Designed for code and data display
- Tabular figures for aligned numbers
- Distinctive character shapes prevent misreading

**Font scale:**
- Page titles: 24px / 700 weight
- Section titles: 16px / 600 weight
- Body text: 14px / 400 weight
- Data/numbers: 12-14px / monospace
- Labels: 11px / 600 weight / uppercase

---

## Layout

### Navigation

**Top navigation bar** (56px height) instead of sidebar:
- Maximizes horizontal space for data tables
- Sticky positioning for persistent access
- Translucent background with backdrop blur
- Active state: cyan border + glow effect

**Brand element:**
- Pulsing cyan dot (live status indicator)
- "DAL Workbench" in cyan accent color
- Status bar showing connection state and evaluation date

### Content area

- Max width: 1600px (centered)
- Padding: 32px horizontal, 24px vertical
- Cards and panels use `--bg-2` background
- Subtle grid background (40px × 40px) for technical feel

---

## Components

### Cards

**Metric cards:**
- Large monospace numbers (28px)
- Uppercase labels (11px, tracked)
- Hover effect: cyan border + glow
- Gradient accent bar on top (appears on hover)
- Positive/negative values colored with text-shadow glow

### Tables

**Data tables:**
- Container: `--bg-2` background, rounded corners
- Headers: `--bg-3` background, uppercase labels, subtle border
- Rows: hover effect with `--bg-3` background
- Numbers: right-aligned, tabular figures, monospace
- Borders: subtle `--border-subtle` between rows

### Buttons

**Primary buttons:**
- Cyan background (`--accent-2`)
- Cyan border
- Hover: brighter cyan + glow + subtle lift (-1px)
- Disabled: 40% opacity

**Ghost buttons:**
- Transparent background
- Border: `--border`
- Hover: cyan border + cyan glow background

**Danger buttons:**
- Transparent background
- Red text
- Hover: red background + red border

### Form controls

**Inputs/Selects:**
- Dark background (`--bg`)
- Subtle border
- Focus: cyan border + cyan glow ring
- Monospace for code/data entry

**Labels:**
- Uppercase, 11px, tracked
- `--text-dim` color

### Panels

**Section panels:**
- `--bg-2` background
- Subtle border
- Section titles: 16px with cyan accent bar (3px × 18px)
- Generous padding (24px)

### Status indicators

**Pulse animations:**
- Brand dot: 2s ease-in-out infinite
- Status dots: 2s ease-in-out infinite
- Running indicator: 1.5s (faster pulse)

**Status states:**
- Online: green dot + glow
- Offline: red dot + glow
- Running: amber dot + "running…" text
- Completed: green checkmark icon + "completed" text
- Failed: red X icon + "failed" text

---

## Motion & Animation

### Entrance animations

Cards, panels, and tables fade in with subtle upward motion:
```css
@keyframes fadeIn {
  from { opacity: 0; transform: translateY(8px); }
  to { opacity: 1; transform: translateY(0); }
}
```

Duration: 300ms ease-out

### Hover effects

- Cards: border glow + top accent bar
- Buttons: lift (-1px) + glow
- Table rows: background change
- Links: color transition

### Transitions

All interactive elements use 150-200ms ease transitions for smooth feedback.

---

## Design Principles

1. **Data density over decoration** — maximize information display without clutter
2. **Monospace for numbers** — tabular alignment for financial data
3. **High contrast accents** — cyan on navy for visibility without fatigue
4. **Glow effects** — subtle luminance suggests live, active data
5. **Technical feel** — grid backgrounds, uppercase labels, tracked type
6. **Status visibility** — pulsing indicators for connection and running states
7. **Consistent spacing** — 8px grid system, generous padding

---

## Accessibility

- Color contrast: all text meets WCAG AA standards (4.5:1 minimum)
- Focus states: visible cyan rings on all interactive elements
- Keyboard navigation: all controls accessible via tab
- Semantic HTML: proper heading hierarchy, table structure, form labels

---

## Technical Implementation

### CSS architecture

- CSS custom properties for all design tokens
- BEM-like class naming (`.card`, `.card-title`)
- Utility classes for common patterns (`.mono`, `.muted`, `.num`)
- No CSS-in-JS — pure CSS for performance

### React components

- Functional components with hooks
- TypeScript for type safety
- Helper functions: `css()`, `classNames()`, `inlineStyle()`
- Consistent error handling and loading states

---

## Future Enhancements

- Dark/light theme toggle (CSS custom properties make this trivial)
- Responsive breakpoints for mobile/tablet (currently desktop-optimized)
- Data visualization components (charts, graphs)
- Keyboard shortcuts for power users
- Real-time data streaming indicators

---

## Credits

**Design direction:** Industrial Terminal aesthetic
**Inspiration:** Bloomberg Terminal, trading desk interfaces, control room displays
**Fonts:** DM Sans (Google Fonts), JetBrains Mono (Google Fonts)
**Color theory:** Financial industry conventions + technical precision

---

## Usage

To start the development server:

```bash
cd webui
./start_webui.sh
```

Visit http://localhost:5173

To build for production:

```bash
cd webui/frontend
npm run build
```

---

**Last updated:** 2026-06-06
**Version:** 2.0 (Industrial Terminal redesign)
