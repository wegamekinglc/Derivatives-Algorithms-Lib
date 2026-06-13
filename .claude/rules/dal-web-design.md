# Web UI Design Standards

## Design Philosophy

Adopt an **Industrial Terminal** style, inspired by Bloomberg terminals and trading desk interfaces.

**Core Principles:**
- **Data-dense, technical, refined** - Make complex financial data scannable and actionable
- **Professional & authoritative** - Colors and typography convey the stability of the financial industry
- **Function first** - Every design decision serves the purpose of data presentation
- **Restrained decoration** - Avoid flashy effects, keep it clean and professional

## Color System

### Base Palette (Dark Theme)

```css
/* Background layers */
--bg: #0f1419;              /* Main background - dark gray-black */
--bg-2: #161b22;            /* Secondary background - slightly lighter */
--bg-3: #21262d;            /* Tertiary layer */
--bg-elevated: #2d333b;     /* Elevated elements */

/* Borders */
--border: #30363d;          /* Primary border */
--border-subtle: #21262d;   /* Secondary border */

/* Text hierarchy */
--text: #c9d1d9;            /* Primary text - cool gray-white */
--text-dim: #8b949e;        /* Secondary text */
--text-muted: #6e7681;      /* Tertiary text */
```

### Accent Colors (Financial Palette)

```css
/* Gold/amber - financial industry signature colors */
--accent: #d4a017;          /* Primary accent - gold */
--accent-2: #b8860b;        /* Secondary accent - dark gold */
--accent-glow: rgba(212, 160, 23, 0.2);

/* Status colors */
--green: #2ea043;           /* Positive/success */
--red: #da3633;             /* Negative/error */
--amber: #d29922;           /* Warning/running */
```

### Color Usage Rules

1. **Background layers** - Use at least 3 gray layers to create depth
2. **Gold accents** - Only for active states, buttons, important elements
3. **Status color conventions** - Follow financial industry standards (green up, red down)
4. **No glow effects** - Don't use `box-shadow` or `text-shadow` glow
5. **High contrast** - Text-to-background contrast ratio at least 4.5:1

## Typography System

### Font Selection

```css
/* Primary font - UI text */
font-family: "DM Sans", "Segoe UI", system-ui, sans-serif;

/* Monospace font - data and code */
font-family: "JetBrains Mono", monospace;
```

### Typography Hierarchy

```css
/* Page title */
font-size: 24px;
font-weight: 700;
letter-spacing: -0.5px;

/* Section title */
font-size: 16px;
font-weight: 600;
letter-spacing: -0.3px;

/* Body text */
font-size: 14px;
font-weight: 400;
line-height: 1.5;

/* Labels (uppercase) */
font-size: 11px;
font-weight: 600;
text-transform: uppercase;
letter-spacing: 0.5px;

/* Numbers/data */
font-family: "JetBrains Mono", monospace;
font-size: 12-14px;
font-variant-numeric: tabular-nums;  /* Aligned tabular numbers */
```

### Typography Rules

1. **Numbers must use monospace font** - Financial data requires vertical alignment
2. **Labels in uppercase** - Enhance professionalism and readability
3. **Negative letter-spacing for titles** - Makes titles more compact and powerful
4. **Body text minimum 14px** - Ensure readability

## Layout System

### Overall Layout

```css
.app {
  display: flex;
  flex-direction: column;  /* Top navigation, not sidebar */
  min-height: 100vh;
}

.topbar {
  height: 56px;
  position: sticky;
  top: 0;
  z-index: 100;
}

.main {
  padding: 20px 24px;  /* Reduced padding */
  width: 100%;         /* Full width, no max-width limit */
}
```

### Layout Rules

1. **Top navigation** - Don't use sidebar, maximize horizontal space
2. **Full-width content** - Remove `max-width` limits, utilize full screen
3. **Reduced whitespace** - Padding 20-24px, compact but not crowded
4. **No background textures** - Solid color backgrounds, no grids or patterns
5. **8px grid system** - All spacing uses multiples of 8 (or 4px multiples)

## Component Specifications

### Cards

```css
.card {
  background: var(--bg-2);
  border: 1px solid var(--border);
  border-radius: 6px;
  padding: 20px;
  transition: all 0.2s ease;
}

.card:hover {
  border-color: var(--accent-2);
  transform: translateY(-2px);  /* Subtle lift */
}

.card h3 {
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 1px;
  color: var(--text-dim);
}

.metric {
  font-size: 28px;
  font-family: "JetBrains Mono", monospace;
  font-weight: 700;
}

.metric.pos { color: var(--green); }
.metric.neg { color: var(--red); }
```

**Card rules:**
- No excessive border radius (6px max)
- Hover effect: border color change + subtle lift
- No glow effects
- Large numbers use monospace font

### Buttons

```css
button {
  background: var(--accent-2);
  color: white;
  border: 1px solid var(--accent);
  border-radius: 4px;
  padding: 8px 16px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s ease;
}

button:hover {
  background: var(--accent);
}

button.ghost {
  background: transparent;
  border: 1px solid var(--border);
  color: var(--text-dim);
}

button.ghost:hover {
  color: var(--text);
  border-color: var(--text-dim);
  background: var(--bg-3);
}

button.danger {
  background: transparent;
  border: 1px solid var(--border);
  color: var(--red);
}

button.danger:hover {
  background: var(--red-dim);
  border-color: var(--red);
}
```

**Button rules:**
- Primary button: gold background
- Ghost button: transparent background, fills on hover
- Danger button: red text, red background on hover
- No glow effects, only color changes
- 4px border radius

### Tables

```css
.table-container {
  background: var(--bg-2);
  border: 1px solid var(--border);
  border-radius: 6px;
  overflow: hidden;
}

table {
  width: 100%;
  border-collapse: collapse;
}

th {
  background: var(--bg-3);
  color: var(--text-dim);
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  font-weight: 600;
  border-bottom: 1px solid var(--border);
}

td {
  padding: 12px 16px;
  border-bottom: 1px solid var(--border-subtle);
}

tbody tr:hover {
  background: var(--bg-3);
}

td.num, th.num {
  text-align: right;
  font-family: "JetBrains Mono", monospace;
  font-size: 12px;
  font-variant-numeric: tabular-nums;
}
```

**Table rules:**
- Headers: uppercase labels, dark background
- Number columns: right-aligned, monospace font
- Row hover: background color change
- All tables wrapped in `.table-container`

### Status Indicators

```css
.status-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  animation: pulse 2s ease-in-out infinite;
}

.status-dot.online {
  background: var(--green);
}

.status-dot.offline {
  background: var(--red);
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

.status-running {
  color: var(--amber);
  font-family: "JetBrains Mono", monospace;
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

.status-running::before {
  content: "";
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--amber);
  animation: pulse 1.5s ease-in-out infinite;
}
```

**Status indicator rules:**
- Use pulse animation for active states
- Green = online/success
- Red = offline/failure
- Amber = running/warning
- No glow effects, only opacity animation

## Animation Specifications

### Entrance Animations

```css
@keyframes fadeIn {
  from {
    opacity: 0;
    transform: translateY(8px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.card, .panel, .table-container {
  animation: fadeIn 0.3s ease-out;
}
```

### Transition Animations

```css
/* All interactive elements */
transition: all 0.2s ease;

/* Hover effects */
.card:hover {
  transform: translateY(-2px);
}
```

### Animation Rules

1. **Entrance animations** - 0.3s fade-in + upward movement
2. **Transition effects** - 150-200ms ease
3. **Pulse animations** - 2s loop (status indicators 1.5s)
4. **Avoid complex animations** - Only use opacity, transform, color changes
5. **No bounce/rotation** - Maintain professional feel

## Form Controls

```css
input, select, textarea {
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 4px;
  color: var(--text);
  padding: 8px 12px;
  font-size: 13px;
  transition: all 0.2s ease;
}

input:focus, select:focus, textarea:focus {
  outline: none;
  border-color: var(--accent-2);
  /* No glow ring */
}

label {
  display: block;
  margin-bottom: 6px;
  color: var(--text-dim);
  font-size: 11px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}
```

**Form control rules:**
- Dark background input fields
- Focus state: only border color change, no glow ring
- Labels: uppercase, small, bold
- Code inputs use monospace font

## Design Don'ts

### ❌ Don't Do

1. **Don't use glow effects** - `box-shadow: 0 0 20px`, etc.
2. **Don't use grid backgrounds** - Use solid color backgrounds
3. **Don't use excessive border radius** - Maximum 6px
4. **Don't use sidebar navigation** - Use top navigation
5. **Don't limit max-width** - Utilize full screen space
6. **Don't use Inter/Roboto fonts** - Use DM Sans and JetBrains Mono
7. **Don't use purple gradients** - Avoid "AI-generated" feel
8. **Don't use excessive decoration** - Keep it clean and professional
9. **Don't use bounce/rotation animations** - Only use fade-in, transform, color changes
10. **Don't use colored text glow** - `text-shadow` glow effects

### ✅ Should Do

1. **Use financial color palette** - Gold/amber as accent colors
2. **Numbers use monospace font** - JetBrains Mono + tabular-nums
3. **Labels in uppercase** - Enhance professionalism
4. **Maintain data density** - Compact but not crowded
5. **Use subtle hover effects** - Border color change, subtle lift
6. **Use pulse animations** - Indicate active states
7. **Maintain high contrast** - Ensure readability
8. **Use 8px grid** - All spacing uses multiples of 8
9. **Use CSS variables** - Maintain design system consistency
10. **Prioritize mobile experience** - Responsive design

## File Locations

All styles are defined in:
```
dal-web/frontend/src/styles.css
```

When modifying styles, update this file and follow the specifications above.

## Design Review Checklist

Before submitting Web UI code, check:

- [ ] Uses financial color palette (gold accents)?
- [ ] Numbers use monospace font?
- [ ] All glow effects removed?
- [ ] Uses top navigation instead of sidebar?
- [ ] Content is full-width (no max-width limit)?
- [ ] Uses 8px grid system?
- [ ] Animations are simple (no bounce/rotation)?
- [ ] Labels use uppercase?
- [ ] Table numbers are right-aligned?
- [ ] Avoids "AI-generated" generic feel?

## References

- **Style file**: `dal-web/frontend/src/styles.css`
- **Web UI overview**: `dal-web/README.md`
- **Inspiration sources**: Bloomberg Terminal, trading desk interfaces, control room displays

---

**Last updated**: 2026-06-06
**Version**: 2.1 (removed grid background, optimized color palette)
