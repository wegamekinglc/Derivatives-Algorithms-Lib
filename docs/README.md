# DAL Documentation

This directory contains technical documentation for the Derivatives Algorithms Library (DAL).

## Documentation Structure

### Installation

- **[installation.md](installation.md)** — Complete Installation Guide
  - System requirements (C++ compiler, Python, Node.js)
  - C++ library installation (Linux and Windows)
  - Python bindings setup with uv
  - Web UI installation and startup
  - Verification and troubleshooting

### Methodology (`methodology/`)

Deep dives into the quantitative methods and algorithms implemented in DAL:

- **[aad.md](methodology/aad.md)** — Automatic Adjoint Differentiation (AAD)
  - Expression templates, tape management, reverse-mode propagation
  - Backend architecture (native, XAD, CoDiPack, Adept)
  - Parallel AAD for Monte Carlo simulations

- **[yield_curve.md](methodology/yield_curve.md)** — Yield Curve Construction
  - Discount curve framework (`DiscountPWLF_`, `DiscountPWC_`)
  - Piecewise-linear and piecewise-constant forward rates
  - Multi-curve construction and calibration
  - Integration with the underdetermined solver

- **[underdetermined_search.md](methodology/underdetermined_search.md)** — Underdetermined Optimization
  - Scaled quasi-Newton method for underdetermined systems
  - Application to yield curve calibration
  - Regularization and smoothness penalties

### Designs (`designs/`)

Architectural design documents for specific features:

- **[adept-aad-backend.md](designs/adept-aad-backend.md)** — Adept AAD Backend Integration
  - Design for integrating the Adept automatic differentiation library
  - Tape management and propagation semantics

## Documentation Conventions

All documentation uses GitHub-flavored Markdown with:

- **Mathematical notation** — LaTeX-style math in `$...$` (inline) or `$$...$$` (display)
- **Code references** — Inline code with backticks, file paths relative to repo root
- **Cross-references** — Links between docs use relative paths (e.g., `[AAD](methodology/aad.md)`)

## Relationship to `.claude/`

The `.claude/` directory contains **operational configuration** for the Claude Code agent:

- `.claude/rules/` — Style guides and coding conventions enforced by the agent
- `.claude/agents/` — Agent definitions (orchestrator, implementer, reviewer, etc.)
- `.claude/skills/` — Reusable skills and workflows

These are **not reference documentation** but rather instructions for AI-assisted development. See them in the repository root.

## Contributing

When adding new documentation:

1. **Methodology docs** go in `docs/methodology/` — explain algorithms, math, and design decisions
2. **Design docs** go in `docs/designs/` — describe feature implementations and architectural choices
3. **Update this index** — add a brief description and link to new documents
4. **Cross-reference** — link related documents using relative paths

Keep documentation focused and technical. Avoid duplicating information that belongs in code comments or the main README.
