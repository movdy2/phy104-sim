# PHY104 Optical Assistant - what is done, what to add next

## Done
* Experiments: Refraction & TIR (two-case screen **and** the single-case TIR screen), Lenses & Mirrors (thin lens and
  signed-f convex/concave mirror with d_i-vs-d_o chart), Prisms (+dispersion), Double slit - each with LCD registers,
  notices, charts, live update while typing, expressions in fields (`0.5*mm`, `1/3`, `π`), clickable ▲▼ arrows.
* Invalid-input pattern from Figma: inline red message, Calculate disabled, **Restore d = 0.250 mm**, last valid result
  retained, red engine light and status bar.
* Calculator with an on-screen Casio keypad; global virtual keyboard with fallback target (works before any click).
* Formula Solver (42 formulas, search, chapters, paging, LaTeX), constants, SVG optical setups, Excel/CSV export.
* **Markdown Study Workspace** (Notes & Formula Jotter): Gruvbox highlighter (H1-H4, formulas, bullets), snippet toolbar,
  problem template, Edit/Split/Preview, autosave into `Snapshot.notesContent` and the `.phy` file.
* Session file (`.phy`): runs, notes, snapshots (parameters + calculator registers), equations and drawing storage.
* Tests: core numbers of every Figma frame, data round trips, UI (colours, fonts, highlighter), **30 click-through tests**.

## Built in the core, UI still missing
1. **Equation Lab** - write an equation in LaTeX or plain text, live typeset preview, solve it for any variable.
   `EquationText` (LaTeX <-> expression) and `EquationSolver` are done and tested; the page is not built yet.
2. **Draw Space** - whiteboard for optics sketches (pen, line, ray, lens, mirror, prism, interface, text/LaTeX labels,
   select/move, undo/redo, PNG/SVG export). `Drawing` (shapes, hit-testing, undo/redo) and its persistence are done and tested.
3. **Dedicated Export screen** (Figma frame "Export (Excel/CSV)": series description, full preview, "recent export" card).
   The export code works (it is on the Formulas & Notes page today).

## Should add next (priority order)
1. Equation Lab page and Draw Space page (above) - they make the app a full write-and-draw workspace.
2. Dedicated Export page + remembered "recent export".
3. Print / PDF report: notes + registers + diagrams + chart images in one document.
4. Experiment extras: thick lens, spherical aberration, single-slit diffraction envelope, diffraction grating,
   thin-film interference, polarisation (Malus / Brewster) screens - formulas already exist in `formulas.json`.
5. Compare runs: overlay two saved runs on one chart; edit/delete saved runs from the dashboard.
6. Notes: LaTeX-rendered formulas in the preview (use `LatexRenderer`), note templates per chapter, search, export to .md/.pdf.
7. Keyboard shortcuts (Ctrl+Enter calculate, Ctrl+S save, Ctrl+1..9 pages), undo for field edits, drag-and-drop of data files.
8. Units: unit dropdown per field (nm/µm/mm/cm/m), significant-figures setting, copy-as-LaTeX for every register.
9. Accessibility and polish: high-contrast theme, font-size setting, window-size tests (1080x700), translations (Arabic UI).
10. Packaging: Windows installer (windeployqt + NSIS), Linux AppImage, GitHub Actions build matrix.
