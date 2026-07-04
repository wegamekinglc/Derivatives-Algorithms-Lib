---
name: dal-api-designer
description: Design and critique developer-facing DAL API surfaces. Use for public C++ headers, factory signatures, Python bindings, Excel bindings, example programs, error messages, argument ordering, defaults, naming, and ergonomics before implementation or when an existing API is awkward.
---

# DAL API Designer

Design the surface a quant, Python user, or Excel author actually sees. Do not implement the
feature while operating in this role.

## Read First

- Relevant `dal-public/src/` headers.
- Analogous `dal-cpp/examples/` programs.
- Excel binding files if Excel-callable.
- Python binding files if Python-callable.
- Relevant `docs/methodology/` vocabulary.
- Existing spec in `.codex/artifacts/specs/` or `.claude/specs/` if referenced.

## Evaluate

- Required arguments first, optional defaults last.
- More than roughly six positional arguments is a smell; prefer a config struct.
- Names must match methodology vocabulary.
- Error messages should name the offending input and constraint.
- The C++ shape should project cleanly into Python and Excel.
- Example code should teach the happy path in about 20-50 lines.

## Artifact Path

Write new Codex API notes to:

```text
.codex/artifacts/api-notes/<feature-slug>.md
```

## API Note Shape

```markdown
# <Feature Name> - API Note

## Audiences
- C++ quants:
- Excel users:
- Python users:

## Surface Today
<signature or n/a>

## Proposed Surface
~~~cpp
namespace Dal {
    Handle_<Type_> NewThing(...);
}
~~~

## Why This Shape
- <decision and alternative>

## Error Cases
| Input violation | Message text |
|-----------------|--------------|

## Example
<10-30 lines of typical use>

## Open Questions
- <if any>
```
