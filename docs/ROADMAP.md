# PHY104 Optical Assistant - what is done, what to add next

## Done
* Experiments: Refraction & TIR (two-case screen **and** the single-case TIR screen), Lenses & Mirrors (thin lens and
  signed-f convex/concave mirror with d_i-vs-d_o chart), Prisms (+dispersion), Double slit - each with LCD registers,
  notices, charts, live update while typing, expressions in fields (`0.5*mm`, `1/3`, `π`), clickable ▲▼ arrows.
* Invalid-input pattern from Figma: inline red message, Calculate disabled, **Restore d = 0.250 mm**, last valid result
  retained, red engine light and status bar.
* Calculator with an on-screen Casio keypad; global virtual keyboard with fallback target (works before any click).
* Formula Solver (42 formulas, search, chapters, paging, LaTeX), constants, SVG optical setups.
* **Export (Excel/CSV)** page: every data series with its description, the full table (no 7-row sample), workbook/CSV
  settings, and a "recent export" card remembered in the `.phy` session.
* **Draw Space** (sketchpad): pen, line, ray, rectangle/ellipse, lens, mirror, prism, interface, axis, normal, text and
  LaTeX labels; select/move, Delete, undo/redo, grid snap, six colours, three widths, PNG (2x) and SVG export; the sketch
  autosaves into the `.phy` session.
* **Markdown Study Workspace** (Notes & Formula Jotter): Gruvbox highlighter (H1-H4, formulas, bullets), snippet toolbar,
  problem template, Edit/Split/Preview, autosave into `Snapshot.notesContent` and the `.phy` file.
* Session file (`.phy`): runs, notes, snapshots (parameters + calculator registers), equations and drawing storage.
* Tests: core numbers of every Figma frame, data round trips, UI (colours, fonts, highlighter), **30 click-through tests**.

## Built in the core, UI still missing
* Nothing right now: `EquationLabPage` (LaTeX lab) and `DrawSpacePage` (sketchpad) are both built on top of their
  tested engines (`EquationText`/`EquationSolver`, `Drawing`/`DrawingHistory`).

## Should add next (priority order)
1. Print / PDF report: notes + registers + diagrams + chart images in one document.
2. Experiment extras: thick lens, spherical aberration, single-slit diffraction envelope, thin-film colours,
   second tabs (thin film, Michelson) - the formulas already exist in `formulas.json`.
3. Compare runs: overlay two saved runs on one chart; edit/delete saved runs from the dashboard.
4. Notes: note templates per chapter, search, export to .md/.pdf (LaTeX formula images in the preview are done).
5. Keyboard shortcuts (Ctrl+Enter calculate, Ctrl+S save, Ctrl+1..9 pages), undo for field edits, drag-and-drop of data files.
6. Units: unit dropdown per field (nm/µm/mm/cm/m), significant-figures setting, copy-as-LaTeX for every register.
7. Accessibility and polish: high-contrast theme, font-size setting, window-size tests (1080x700), translations (Arabic UI).
8. Packaging: Windows installer (windeployqt + NSIS), Linux AppImage, GitHub Actions build matrix.
