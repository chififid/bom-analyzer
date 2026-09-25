find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

qt_add_executable(ProfileAnalysisTests profile_analysis_test.cpp)
set_target_properties(ProfileAnalysisTests PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(ProfileAnalysisTests PRIVATE profile_analysis Qt6::Test)

add_test(NAME profile_analysis COMMAND ProfileAnalysisTests
    -o -,txt
    -o "${CMAKE_CURRENT_BINARY_DIR}/profile-analysis-results.xml,junitxml"
)

qt_add_executable(BomAnalyticsTests bom_analytics_test.cpp)
set_target_properties(BomAnalyticsTests PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(BomAnalyticsTests PRIVATE bom_core Qt6::Test)

add_test(NAME bom_analytics COMMAND BomAnalyticsTests
    -o -,txt
    -o "${CMAKE_CURRENT_BINARY_DIR}/bom-analytics-results.xml,junitxml"
)
qt_add_executable(BomCsvReaderTests bom_csv_reader_test.cpp)
set_target_properties(BomCsvReaderTests PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(BomCsvReaderTests PRIVATE bom_io bom_core Qt6::Test)

add_test(NAME bom_csv_reader COMMAND BomCsvReaderTests
    -o -,txt
    -o "${CMAKE_CURRENT_BINARY_DIR}/bom-csv-reader-results.xml,junitxml"
)
qt_add_executable(BomJsonWriterTests bom_json_writer_test.cpp)
set_target_properties(BomJsonWriterTests PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(BomJsonWriterTests PRIVATE bom_io Qt6::Test)

add_test(NAME bom_json_writer COMMAND BomJsonWriterTests
    -o -,txt
    -o "${CMAKE_CURRENT_BINARY_DIR}/bom-json-writer-results.xml,junitxml"
)
qt_add_executable(BomUiTests bom_ui_test.cpp)
set_target_properties(BomUiTests PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(BomUiTests PRIVATE bom_ui Qt6::Test)

add_test(NAME bom_ui COMMAND BomUiTests
    -o -,txt
    -o "${CMAKE_CURRENT_BINARY_DIR}/bom-ui-results.xml,junitxml"
)
# Local deployment may include only the Windows plugin; tests need offscreen.
set_tests_properties(bom_ui PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>"
)
set_tests_properties(profile_analysis bom_analytics bom_csv_reader bom_json_writer bom_ui PROPERTIES TIMEOUT 15)

if(WIN32)
    add_dependencies(BomUiTests ProfileAnalyzer)
    qt_add_executable(ProfileAnalyzerDllTests profile_analyzer_dll_test.cpp)
    set_target_properties(ProfileAnalyzerDllTests PROPERTIES WIN32_EXECUTABLE FALSE)
    target_include_directories(ProfileAnalyzerDllTests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(ProfileAnalyzerDllTests PRIVATE Qt6::Test)
    add_dependencies(ProfileAnalyzerDllTests ProfileAnalyzer)
    add_test(NAME profile_analyzer_dll COMMAND ProfileAnalyzerDllTests
        -o -,txt
        -o "${CMAKE_CURRENT_BINARY_DIR}/profile-analyzer-dll-results.xml,junitxml"
    )
    set_tests_properties(profile_analyzer_dll PROPERTIES TIMEOUT 15)

    set_tests_properties(profile_analysis profile_analyzer_dll bom_analytics bom_csv_reader bom_json_writer bom_ui PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()
