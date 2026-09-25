add_library(profile_analysis STATIC
    profile_analyzer/profile_analyzer_api.h
    profile_analyzer/profile_analysis.h
    profile_analyzer/profile_analysis.cpp
)
target_include_directories(profile_analysis PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(profile_analysis PROPERTIES AUTOMOC OFF AUTOUIC OFF)

if(WIN32)
    add_library(ProfileAnalyzer SHARED
        profile_analyzer/profile_analyzer_api.h
        profile_analyzer/profile_analyzer_api.cpp
        profile_analyzer/profile_analyzer.def
    )
    target_include_directories(ProfileAnalyzer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(ProfileAnalyzer PRIVATE profile_analysis)
    set_target_properties(ProfileAnalyzer PROPERTIES AUTOMOC OFF AUTOUIC OFF)
endif()

add_library(bom_core STATIC
    domain/bom_document.h
    domain/bom_fields.h
    domain/analytics_result.h
    domain/operation_result.h
    core/bom_analytics.h
    core/bom_analytics.cpp
)
target_include_directories(bom_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bom_core PUBLIC Qt6::Core)

add_library(bom_io STATIC
    io/bom_csv_reader.h
    io/bom_csv_reader.cpp
    io/bom_json_writer.h
    io/bom_json_writer.cpp
)
target_include_directories(bom_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bom_io PUBLIC Qt6::Core)

add_library(bom_ui STATIC
    models/bom_tree_model.h
    models/bom_tree_model.cpp
    ui/main_window.h
    ui/main_window.cpp
    ui/bom_item_delegate.h
    ui/bom_item_delegate.cpp
    ui/profile_analyzer_client.h
    ui/profile_analyzer_client.cpp
)
target_link_libraries(bom_ui PUBLIC bom_core Qt6::Widgets PRIVATE bom_io)

qt_add_executable(BomAnalyzer WIN32
    app/main.cpp
)
target_link_libraries(BomAnalyzer PRIVATE bom_ui)

if(WIN32)
    add_dependencies(BomAnalyzer ProfileAnalyzer)
endif()

if(MSVC)
    set_target_properties(BomAnalyzer PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>;%PATH%"
    )
endif()
