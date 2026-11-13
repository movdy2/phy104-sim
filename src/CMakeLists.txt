find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_subdirectory(data)          # defines the phy104_data library (JSON repositories)

set(PHY104_UI_SOURCES
    App.cpp
    ui/ThemeManager.cpp ui/Icons.cpp ui/Components.cpp ui/Plots.cpp ui/OpticsCanvases.cpp
    ui/AppContext.cpp ui/BasePage.cpp ui/ExperimentPages.cpp ui/ModelExperimentPages.cpp ui/ScientificKeyboard.cpp ui/EquationLabPage.cpp ui/DashboardPage.cpp ui/NotesExportPage.cpp
    ui/LatexRenderer.cpp ui/FormulaListDelegate.cpp ui/SvgDiagramView.cpp ui/DiagramsPage.cpp
    ui/MarkdownHighlighter.cpp ui/MarkdownWorkspace.cpp ui/MathSolverPage.cpp ui/VirtualKeyboard.cpp ui/SolverPage.cpp ui/ConstantsPage.cpp ui/MainWindow.cpp
)

# All widgets live in a library so the tests can build the very same application.
add_library(phy104_ui STATIC ${PHY104_UI_SOURCES})
target_include_directories(phy104_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/ui)
target_link_libraries(phy104_ui PUBLIC phy104_data phy104_core Qt6::Widgets Qt6::Svg)

add_executable(PHY104OpticalAssistant main.cpp ../assets/assets.qrc)
target_link_libraries(PHY104OpticalAssistant PRIVATE phy104_ui)

# Copy the JSON database next to the executable
add_custom_command(TARGET PHY104OpticalAssistant POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:PHY104OpticalAssistant>/data)

if(WIN32)
    set_target_properties(PHY104OpticalAssistant PROPERTIES WIN32_EXECUTABLE ON)
endif()
