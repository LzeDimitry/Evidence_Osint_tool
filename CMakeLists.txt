cmake_minimum_required(VERSION 3.20)

project(evidence_trace VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_path(LIBZIP_INCLUDE_DIR zip.h REQUIRED)
find_library(LIBZIP_LIBRARY NAMES zip REQUIRED)

# Backend/core: no Qt dependency. The GUI and CLI are adapters over this
# library and must keep business rules out of widgets and argument parsing.
set(EVIDENCE_TRACE_BACKEND_SOURCES
    src/domain/types.cpp
    src/database/sqlite_database.cpp
    src/database/migrations.cpp
    src/storage/attachment_store.cpp
    src/services/event_logger.cpp
    src/services/case_service.cpp
    src/services/investigation_service.cpp
    src/services/playbook_service.cpp
    src/import_export/json.cpp
    src/import_export/import_export_service.cpp
)

add_library(evidence_trace_core ${EVIDENCE_TRACE_BACKEND_SOURCES})
target_include_directories(evidence_trace_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LIBZIP_INCLUDE_DIR}
)
target_link_libraries(evidence_trace_core
    PUBLIC
        OpenSSL::Crypto
        ${LIBZIP_LIBRARY}
)
if (TARGET SQLite3::SQLite3)
    target_link_libraries(evidence_trace_core PUBLIC SQLite3::SQLite3)
else()
    target_link_libraries(evidence_trace_core PUBLIC SQLite::SQLite3)
endif()

target_compile_options(evidence_trace_core PRIVATE -Wall -Wextra -Wpedantic)

# Frontend: scriptable CLI over evidence_trace_core.
add_executable(evidence-trace src/cli/main.cpp)
target_link_libraries(evidence-trace PRIVATE evidence_trace_core)
target_compile_options(evidence-trace PRIVATE -Wall -Wextra -Wpedantic)

# Frontend: Qt desktop application over evidence_trace_core.
set(EVIDENCE_TRACE_GUI_SOURCES
    src/gui/main.cpp
    src/gui/evidence_trace.qrc
    src/gui/application_context.cpp
    src/gui/gui_helpers.cpp
    src/gui/dialogs.cpp
    src/gui/global_pages.cpp
    src/gui/investigation_pages.cpp
    src/gui/context_pages.cpp
    src/gui/main_window.cpp
)

add_executable(evidence-trace-gui ${EVIDENCE_TRACE_GUI_SOURCES})
target_link_libraries(evidence-trace-gui PRIVATE evidence_trace_core Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_options(evidence-trace-gui PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(evidence_trace_tests tests/integration_tests.cpp)
target_link_libraries(evidence_trace_tests PRIVATE evidence_trace_core)
target_compile_options(evidence_trace_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME evidence_trace_integration COMMAND evidence_trace_tests)

install(TARGETS evidence-trace evidence-trace-gui RUNTIME DESTINATION bin)
install(FILES "docs/image/Neon Hexagonal Cube Network.png"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps"
    RENAME evidence-trace.png
)
install(FILES packaging/evidence-trace.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)
