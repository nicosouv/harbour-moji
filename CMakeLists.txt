cmake_minimum_required(VERSION 3.5)

project(harbour-moji VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Qml Quick Concurrent Sql)

# Optional so a build without linguist tools still produces a working app,
# just an untranslated one
find_package(Qt5LinguistTools QUIET)

# Native dependencies are cross-compiled into 3rdparty/ by the scripts in
# scripts/, inside the Sailfish SDK container, and cached by CI on each script's
# hash. Nothing links them yet - they arrive with src/ocrengine.cpp - but
# scripts/download_models_for_build.sh already fetches the language data, because
# the size it adds to the RPM is worth discovering now rather than at v0.9.
set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/3rdparty)

# The pure layers: no Tesseract, no OpenCV, no Qt Quick. Everything that makes a
# decision lives here so tests/ can reach it with plain Qt5, which is the only
# feedback this project gets before a tag.
set(CORE_SOURCES
    src/ocrresult.cpp
    src/textlayout.cpp
    src/fieldparser.cpp
)

set(CORE_HEADERS
    src/ocrresult.h
    src/textlayout.h
    src/fieldparser.h
)

set(SOURCES
    src/main.cpp
    src/logging.cpp
    src/settings.cpp
    ${CORE_SOURCES}
)

set(HEADERS
    src/logging.h
    src/settings.h
    ${CORE_HEADERS}
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Single source for the version shown on the About page: project() above
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Gui
    Qt5::Qml
    Qt5::Quick
    Qt5::Concurrent
    Qt5::Sql
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# The whole tree, so a new QML directory needs no change here - qml/Mochi came in
# that way. Documentation travelling with a module is the one thing not worth
# putting in the RPM.
install(DIRECTORY qml
    DESTINATION share/${PROJECT_NAME}
    PATTERN "*.md" EXCLUDE
)

# The language data, fetched and checksum-verified by
# scripts/download_models_for_build.sh. This is what makes the app work with the
# network switched off, and it is most of the RPM's size.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/tessdata
    DESTINATION share/${PROJECT_NAME}
    FILES_MATCHING PATTERN "*.traineddata"
)

if(Qt5LinguistTools_FOUND)
    file(GLOB TS_FILES ${CMAKE_SOURCE_DIR}/translations/*.ts)
    qt5_add_translation(QM_FILES ${TS_FILES})
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
    install(FILES ${QM_FILES}
        DESTINATION share/${PROJECT_NAME}/translations
    )
else()
    message(WARNING "Qt5LinguistTools not found - translations will not be built")
endif()

install(FILES harbour-moji.desktop
    DESTINATION share/applications
)

foreach(SIZE 86 108 128 172)
    install(FILES icons/${SIZE}x${SIZE}/harbour-moji.png
        DESTINATION share/icons/hicolor/${SIZE}x${SIZE}/apps
    )
endforeach()

find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
if(DESKTOP_FILE_VALIDATE)
    add_custom_target(validate_desktop ALL
        COMMAND ${DESKTOP_FILE_VALIDATE} ${CMAKE_SOURCE_DIR}/harbour-moji.desktop
    )
endif()

# The unit tests are a project of their own so they build without the Sailfish
# SDK and without Tesseract - see tests/CMakeLists.txt. Nothing here is ever
# compiled on the developer's machine, so that lane is the only pre-tag feedback
# there is.
option(BUILD_TESTS "Build the unit tests" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
