# PHY104 Optical Assistant - what is done, what to add next

## Done
* Experiments: Refraction & TIR (two-case screen **and** the single-case TIR screen), Lenses & Mirrors (thin lens and
  signed-f convex/concave mirror with d_i-vs-d_o chart), Prisms (+dispersion), Double slit - each with LCD registers,
  notices, charts, live update while typing, expressions in fields (`0.5*mm`, `1/3`, `π`), clickable ▲▼ arrows.
* Invalid-input pattern from Figma: inline red message, Calculate disabled, **Restore d = 0.250 mm**, last valid result
  retained, red engine light and status bar.
* Calculator with an on-screen Casio keypad; global virtual keyboard with fallback target (works before any click).
* Formula Solver (42 formulas, search, chapters, paging, LaTeX), constants, SVG optical setups.
* **One Export page** for CSV, Excel and PDF: the **Series data** mode shows every data series, its description and the
  full table (no 7-row sample), workbook/CSV settings and Print…; the **PDF report** mode produces an academic lab report
  with date/session metadata, numbered experiment sections, diagrams/charts with captions, parameters, LCD registers,
  notes, saved calculations, A4 portrait/landscape, repeated page headers and numbered footers. Recent exports are
  remembered in the `.phy` session.
* **Second tabs on all six model screens**: Michelson **Fringes** (spacing λ/(2n), count over ±2 µm, contrast from the
  beam balance), Thin-Film **Transmission** (`cos²(2πnt/λ)`, no net reversal, transmitted-beam diagram), Grating **Orders**
  (every peak with its angle, `d/λ` capacity, all maxima marked on the chart), Single slit **Geometry** (minima positions
  on the screen, `yₘ`-vs-order chart) and Malus **Brewster** (Fresnel `R_p`/`R_s`, `R_p(θ)` chart). Newton's viewing-mode
  dropdown now switches the physics and the ring pattern, and the Michelson reference arm draws a piezo stack.
* **Markdown Study Workspace** (Notes): Gruvbox highlighter (H1-H4, formulas, bullets), snippet toolbar, problem
  template, Edit/Split/Preview, autosave into `Snapshot.notesContent` and the `.phy` file - and **only** that: the
  formula reference (now on the Formula Solver page) and the export panels were removed.
* **One export icon** in every page header: PDF report / Excel (.xlsx) / CSV (.csv), opening the matching mode on the
  unified Export destination. Every page fills the whole window (the old 1500-px content cap is gone); the Draw Space page is gone.
* **Compare saved runs**: dashboard **Recent / Compare** switch - two run pickers overlay their sweeps on one chart
  (kept with the run as `RunRecord::series`, session format version 6); rows are selectable and **Delete selected run**
  drops a run and autosaves.
* **Keyboard shortcuts**: Ctrl+S save, Ctrl+Enter calculate/solve, Ctrl+P PDF report, Ctrl+1..9 pages, Ctrl+Tab next
  page, F11 full screen; the pages fill the window (tested at 1920 px wide).
* **CI**: GitHub Actions build matrix (Linux Qt 6 + ctest, Windows MSVC + Qt 6.7.2, Qt-free `g++` core test).
* Session file (`.phy`, format version 6): runs (with their sweep), notes, snapshots (parameters + calculator registers),
  equations and the recent export. The removed Draw Space sketch is no longer saved; legacy drawing payloads are ignored.
* **Experiment extras** (v2.8): **Thick Lens & Aberration** page (power with a finite thickness, principal planes,
  BFL/FFL, principal-plane imaging, third-order spherical aberration ∝ D², f/#; a **Spectrum** tab with Cauchy's law and
  the Abbe number), **Thin-Film Colours** tab (CIE 1931 integration of `R(λ)`, a swatch, visibility and order count),
  **Single-Slit Envelope** tab (two slits of finite width: fringes inside the envelope, missing orders at integer
  `d/a`) and **Grating Resolution** tab (`R = mN`, Rayleigh `δλ`, free spectral range). Qt-free `ThickLens` and
  `Spectrum` modules, tested in `test_core`.
* **Study Hub** (v2.7): `data/study_hub.json` is a **JSON array** of ten chapters in course order - summary, rules
  (formula + why it holds) and the constants of the chapter; the Formula Solver stays a separate page.
* **Input stays mode-specific**: Calculator uses plain-text math and its focused keypad; LaTeX stays in Equation Lab and
  Notes. The shared scientific keyboards are unchanged. The light theme is darker and the dark theme's small text is brighter (v2.7).
* **Unified tools & dashboard (v2.9)**: all main-page headers share **Save + one Export menu** (PDF / Excel / CSV);
  the Study Hub tabs now include the shared Notes workspace, Optical Setups and global Constants (Notes keeps its sidebar
  shortcut; the other two move into the Hub); the dashboard keeps four featured cards and adds a Browse all 11 menu;
  Calculator is a cleaner expression/result screen with just its math keypad and high-contrast text.
* Tests: core numbers of every Figma frame (plus both modes of Newton, Michelson and thin film), the report HTML
  builder (escaping, options, image keys), data round trips,
  UI (colours, fonts, highlighter), click-through tests of every screen (now including the second tabs of
  thin film and Michelson and Newton's viewing-mode dropdown).

## Built in the core, UI still missing
* Nothing right now: `EquationLabPage` (LaTeX lab) is built on top of the tested `EquationText`/`EquationSolver` engine.

## Should add next (priority order)
1. Experiment extras, part 2: coma / astigmatism for the thick lens (the spherical aberration is done), a chromatic
   focal shift readout on the thick-lens page in mm at the focal plane, grating blaze efficiency, and a "film colour
   vs thickness" strip (the colour of every thickness in one band) next to the single swatch.
2. Editing a saved run (the description/result of a run) next to the delete that is already there.
3. Study Hub: per-chapter quiz cards and a "send the rule to the calculator" button.
4. Notes: note templates per chapter, search, export to .md/.pdf (LaTeX formula images in the preview are done).
5. Undo for field edits and drag-and-drop of data files (the shortcuts - Ctrl+S, Ctrl+Enter, Ctrl+P, Ctrl+1..9,
   Ctrl+Tab, F11 - are done).
6. Units: unit dropdown per field (nm/µm/mm/cm/m), significant-figures setting, copy-as-LaTeX for every register.
7. Accessibility and polish: high-contrast theme, font-size setting, window-size tests (1080x700), translations (Arabic UI).
8. Packaging: Windows installer (windeployqt + NSIS), Linux AppImage (the GitHub Actions build matrix is done).
