find_package(Qt6 REQUIRED COMPONENTS Core)
add_library(phy104_data STATIC JsonLoader.cpp JsonRepositories.cpp)
target_include_directories(phy104_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(phy104_data PUBLIC phy104_core Qt6::Core)
