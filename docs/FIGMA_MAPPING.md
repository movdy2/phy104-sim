# Figma design -> code

Source: Figma file "optics_lab.phy" (frames Dashboard, Refraction & TIR, Lenses & Mirrors, Prisms,
Wave Interference, Formulas Notes & Export, Component Sheet).

## Frames
| Figma frame | Class | Notes |
|---|---|---|
| Window shell, sidebar, status bar | `MainWindow` | 224 px sidebar, Lucide icons, two-line label for the long item, status messages |
| Dashboard | `DashboardPage` | metric cards, clickable experiment cards (`PreviewIcon`), table, notebook |
| Refraction & TIR | `RefractionPage` + `RefractionCanvas`, `LineChart` | cases A and B on one bench, regression chart |
| Lenses & Mirrors | `LensesPage` + `LensCanvas`, `MirrorCanvas` | |
| Prisms | `PrismsPage` + `PrismCanvas`, `LineChart` | dispersion mode draws three rays |
| Wave Interference | `WavePage` + `WaveCanvas`, `LineChart` | Intensity / Wavefronts tabs |
| Notes | `NotesPage` | the Markdown study workspace (editor + live preview); the reference is on the Formula Solver screen |
| Export (Excel/CSV) | `ExportPage` | series description + full preview + "recent export" card; `XlsxWriter` builds real .xlsx files without a library |
| Component sheet | `Components.h` + `ThemeManager` | see below |

## Component sheet
| Component | Code |
|---|---|
| Panel (header + body) | `Panel` |
| Input with unit, focus ring, error | `UnitField` (amber 2 px focus, red error, ↕ spinner, Up/Down keys) |
| Segmented tabs | `ModeTabs` |
| Casio calculation register | `LcdRegisterView` (data from `LcdRegister`) |
| Notice / guarded-case card | `NoticeCard` (amber title, red when `error`) |
| Buttons | style sheet: `primary` (amber, hover `#e5ac39`, pressed `#b57614`), `secondary`, `outline` |
| Colours | `Theme::figmaDark()` - `#282828 #3c3836 #ebdbb2 #d79921 #504945 #fb4934`, LCD `#9bb094 / #0b1c08 / #171e15 / #708369` |
| Type | Inter (UI), JetBrains Mono (numbers, registers) |

## Numbers in the design are reproduced exactly
`48.5904°`, `θc = 41.8103°`, `sin θ₂ = 1.1491 > 1`, `dᵢ = +15.0000 cm`, `m = -0.5000`, `hᵢ = -1.5000 cm`,
`δmin = 37.1808°`, `β = 3.1920 mm`, `y₂ = +6.3840 mm`, `Δ = 2λ = 1064.0 nm` - all asserted in `tests/test_core.cpp`.

## Where the app goes beyond or differs from the design
* **Light theme** (Gruvbox-light mapping) - the design is dark only. Toggle in the status bar.
* **Tools section** in the sidebar (Notes, Equation Lab, Study Hub, Formula Solver, Calculator, Export, PDF Report):
  features from the project brief that have no own Figma frame. The Study Hub includes tabs for chapters, shared Notes
  and Optical Setups; chapter-specific constants remain beside each chapter's rules. The Notes sidebar shortcut stays,
  while Optical Setups is accessed through the Hub.
* The shared page header keeps **Save** separate from one **Export** menu (PDF / Excel / CSV); the export destinations
  hide their duplicate menu. The Draw Space sketchpad was removed at the author's request, and the Notes page is only the
  Markdown workspace (no formula reference, no export panels).
* **Error red** is the design's `#fb4934` (earlier brief: `#cc241d`); physics notices use the design's amber card.
* **Real functionality behind the frames**: Calculate & Simulate saves runs, the session autosaves and restores,
  export writes real files. The "21 saved calculations" in the frame is demo data; a fresh session starts at 00.
* **Export (Excel/CSV)** is its own screen (second batch) next to the combined Formulas & Notes page: the export
  settings live on both, the recent-export record is shared and stored in the `.phy` session.
* Charts are drawn with QPainter (`LineChart`), not Qt Charts, to avoid an extra Qt module; the panel meta keeps
  the design's "QT CHARTS" label.
* The faux title bar of the frame is the native window title.
