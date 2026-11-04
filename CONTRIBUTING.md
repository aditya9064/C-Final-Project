# Contributing to Fireteam: Skirmish

This is a 3-person CSC2210 final project team repo. These are our internal
working conventions.

## Branching strategy

- **`main`** — always buildable, always demoable. Protected: no direct
  pushes, merges only via a reviewed pull request.
- **`develop`** — integration branch where finished feature branches land
  before a periodic merge into `main` (e.g. before each check-in/milestone).
- **`feature/<short-name>`** — one branch per subsystem or task, branched
  from `develop`. Current long-lived feature branches map to our subsystem
  ownership (see `docs/proposal.md` section 5):
  - `feature/networking`
  - `feature/rendering`
  - `feature/combat`

  Cut a short-lived `feature/<task>` branch off `develop` for smaller,
  specific tasks instead of piling everything onto the three long-lived
  branches.

## Workflow

1. Branch from `develop` (or the relevant long-lived `feature/*` branch for
   a small sub-task).
2. Commit in small, logical chunks with clear messages.
3. Open a pull request back into `develop` (or `main` for a
   milestone/check-in merge). Fill out the PR template.
4. At least one other teammate reviews before merging — the goal isn't
   just gatekeeping, it's making sure everyone understands the whole
   program, per the course's teamwork requirement.
5. Delete the branch after merging.

## Commit messages

Short, imperative summary line (e.g. `Add hit detection to Player class`),
with a brief body if the "why" isn't obvious from the diff alone.

## Code style

- Split code into headers (`.h`/`.hpp`) and sources (`.cpp`); each
  function/class should have one clear job.
- Prefer standard containers and smart pointers over raw `new`/`delete`.
- Favor composition; use inheritance only where a true "is-a" polymorphic
  relationship exists (see the design rationale in `docs/proposal.md`).

## Tests

Every new subsystem should land with at least one test in `tests/`. See
`docs/proposal.md` section 6 for the categories we're expected to cover
(normal, boundary/empty, invalid input, failure condition).

## AI use

AI tools are allowed per the course policy, but every teammate must be able
to explain, trace, and modify any code they submit without AI help. Log
significant AI use (what/why/what you checked) — see `docs/ai-use-log.md`.
