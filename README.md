# PHY104 Optical Assistant

[![build](https://github.com/movdy2/phy104-sim/actions/workflows/build.yml/badge.svg)](https://github.com/movdy2/phy104-sim/actions/workflows/build.yml)

Desktop application (C++20, Qt 6 Widgets, CMake, JSON data) for PHY104 - Optics and Wave Optics.
Windows and Linux. No database, no network. The UI follows the Figma design `optics_lab.phy`
(see [docs/FIGMA_MAPPING.md](docs/FIGMA_MAPPING.md)).

| | |
|---|---|
| ![Dashboard](docs/screenshots/dashboard.png) | ![Refraction](docs/screenshots/refraction_tir.png) |
| ![Lenses](docs/screenshots/lenses_mirrors.png) | ![Prisms](docs/screenshots/prisms.png) |
| ![Wave](docs/screenshots/wave_interference.png) | ![Export](docs/screenshots/notes_export.png) |

## Study workspace (Notes)
Markdown editor with live preview (Edit / Split / Preview) and Gruvbox colour coding: `#` yellow 18 pt, `##` orange 14 pt,
`###` green 12 pt, `####` red 11 pt (exam warning), formulas (`$...$`, `$$...$$`, `` `code` ``, bracketed maths) aqua in JetBrains Mono,
bullets light grey. Snippet toolbar (θ λ φ δ Δ n₁ n₂ sin arcsin ... and a formulas menu) and a **Problem template**
(`### Given:` / `### Formula:` / `### Substitution & Solution:`). In the preview every formula is drawn as a LaTeX image
by `LatexRenderer`; Qt's Markdown importer drops images with an empty alt text, so each formula image carries an alt text
(`![formula](latex://N)`) and an image written as `![](…)` is given one before import. Notes autosave ~0.7 s after typing
into the session, into every snapshot's `notesContent`, and into the `.phy` file.

## Screens

| Sidebar item | What it does |
|---|---|
| **Dashboard** | metrics, the four experiment cards with their last result, recent calculations (**Recent / Compare**: overlay two saved runs, delete a run), lab notebook |
| **Refraction & TIR** | Snell's law and total internal reflection: two incidence cases, regression chart, LCD registers |
| **Lenses & Mirrors** | thin lens and spherical mirror (same equations), principal rays, image properties |
| **Prisms** | ray path, minimum deviation, deviation curve, dispersion mode (red / λ / violet rays) |
| **Wave Interference** | Young's double slit: wavefronts, fringes, intensity profile, 6 light-source presets |
| **Polarization**, **Single slit**, **Grating**, **Thin film**, **Michelson**, **Newton's rings** | the six screens of the second Figma batch (Save + export icon, Send to Notes); **both tabs of every screen work** (thin-film transmission, Michelson fringe count, grating orders, slit geometry, Malus/Brewster) and Newton's viewing-mode dropdown switches to the transmitted rings |
| **Notes** | the clean Markdown study workspace of the design: editor + snippet toolbar + live LaTeX preview, autosaved (nothing else on the page) |
| **Export icon** (every page header) | one download button with **PDF report / Excel (.xlsx) / CSV (.csv)**: it opens the matching screen with the format already selected |
| *Tools:* **PDF Report** | printable laboratory report: every experiment's headline, diagram and chart, the registers, the notes and the saved runs in one A4 document (Save PDF / Print…) |
| *Tools:* **Export (Excel/CSV)** | data series and what it contains, every row of the table, workbook/CSV settings, remembered recent export |
| *Tools:* **Formula Solver** | 42 formulas in 7 chapters: search, chapter filter, paging, LaTeX, LCD input/answer |
| *Tools:* **Calculator** | free math mode (functions, `pi`, `e`, `Ans`, DEG/RAD), bounded history |
| *Tools:* **Constants**, **Optical Setups** | constants with search/copy; six SVG diagrams with PNG export |

Common to the experiment pages: live update while typing, **Calculate & Simulate** saves a run to the session,
**Reset parameters**, invalid input never shows `NaN` - a red field, an `E_...` register with the
**last valid result retained**, and an explanation card. Session (`optics_lab.phy`) autosaves and is restored on start.
Light and dark theme (status bar), virtual keyboard (status bar).

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build              # core + real-data round trips + UI/LaTeX/SVG/fonts/colours
./build/src/PHY104OpticalAssistant  # data/ is copied next to the executable
```

Needs Qt 6 **Widgets and Svg** (Ubuntu: `qt6-base-dev qt6-svg-dev`). Fonts (Inter, JetBrains Mono) and icons
(Lucide) are bundled in `assets/` - nothing to install. Windows: add `-DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/msvc2019_64`.
Core only (no Qt): `-DPHY104_BUILD_GUI=OFF`.
CI (`.github/workflows/build.yml`) builds Linux (Qt 6 + ctest), Windows (MSVC + Qt 6.7.2) and a Qt-free `g++` core
test on every push and pull request - the badge above shows the last run.

### Keyboard shortcuts

| keys | what it does |
|---|---|
| `Ctrl+S` | save the session (`.phy`) |
| `Ctrl+Enter` | the main action of the page that is showing: **Calculate & Simulate** on the experiment screens, **Solve** on the Formula Solver |
| `Ctrl+P` | open the **PDF Report** screen (preview, Save PDF, Print…) |
| `Ctrl+1` … `Ctrl+9` | the first nine pages of the sidebar |
| `Ctrl+Tab` | next page |
| `F11` | full screen (the window starts maximised) |

Developer option: `PHY104OpticalAssistant --screenshots <folder>` drives the UI (saves runs, exports a workbook,
triggers error states) and writes a PNG of every page in both themes.

## Layers

```
Presentation  src/ui            Qt Widgets: pages, components, canvases, charts, LaTeX renderer, theme
Business      src/core          pure C++ (no Qt): physics, ExperimentReports, SolverEngine, FormulaManager,
                                MathCalculator, SessionLog, DataExport + XlsxWriter, ExpressionEvaluator ...
Data          src/data + data/  JSON repositories (formulas, variables, constants, chapters, keyboard,
                                optics presets, formula reference) and the .phy session file
Assets        assets/           fonts, Lucide icons, SVG diagrams (generated by tools/make_diagrams.py)
```

`src/core` never includes Qt and reaches files only through the interfaces in `Repositories.h`.
The pages contain **no physics and no number formatting**: `ExperimentReports` returns the LCD registers,
notices and chart data as plain structs and the page only displays them.

## Data files (`data/`)

`formulas.json` (42 formulas, equation text + LaTeX + chapter) · `chapters.json` · `variables.json` ·
`constant.json` · `keyboard.json` · `optics_presets.json` (media with Cauchy dispersion, light sources) ·
`reference.json` (formula reference topics and the unit-discipline note).

### Add a formula (no C++)
1. Add `{ "id", "name", "equation", "latex", "chapter" }` to `formulas.json` (`chapter` = id from `chapters.json`).
2. Every variable must exist in `variables.json` (the app and `ctest` warn otherwise).
3. Optional `"units"` override and `"noSolutionHint"`. Run `ctest`: it solves the formula for every variable.

### Add a medium or light source
Edit `optics_presets.json`; it appears in the dropdowns. `cauchyA + cauchyB/λ²` (λ in µm) must give `n` at 589.3 nm
(a test checks this).

## Units
SI everywhere; angles in degrees as labelled. Formula Solver fields accept `550*nm`, `0.5*mm`, `π`, `√(16)`, `c`, `ε₀`.
Fonts: Inter and JetBrains Mono (SIL OFL), icons: Lucide (ISC) - see `assets/fonts/LICENSES.txt`.
