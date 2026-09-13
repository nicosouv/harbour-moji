cmake_minimum_required(VERSION 3.5)

project(harbour-moji VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Qml Quick Concurrent Sql)

# Optional so a build without linguist tools still produces a working app,
# just an untranslated one
find_package(Qt5LinguistTools QUIET)

# Tesseract and Leptonica are cross-compiled into 3rdparty/ by the scripts in
# scripts/, inside the Sailfish SDK container, and cached by CI on each script's
# hash. On a host that happens to package them - which is how the syntax-check
# lane works - pkg-config is used instead, so the code can be compile-checked
# without the SDK.
set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/3rdparty)

find_package(PkgConfig REQUIRED)

# %{_libdir} is /usr/lib on armv7hl and /usr/lib64 on aarch64. Hardcoding "lib"
# builds fine on both and then fails packaging on aarch64 only, at the %files
# stage, long after anything that looks like a compile error.
include(GNUInstallDirs)

if(EXISTS ${THIRDPARTY_DIR}/tesseract)
    message(STATUS "Tesseract: bundled in 3rdparty/")
    set(TESSERACT_INCLUDE_DIRS
        ${THIRDPARTY_DIR}/tesseract/include
        ${THIRDPARTY_DIR}/leptonica/include)
    find_library(TESSERACT_LIB tesseract
        PATHS ${THIRDPARTY_DIR}/tesseract/lib NO_DEFAULT_PATH)
    find_library(LEPTONICA_LIB lept
        PATHS ${THIRDPARTY_DIR}/leptonica/lib NO_DEFAULT_PATH)
    set(TESSERACT_LIBRARIES ${TESSERACT_LIB} ${LEPTONICA_LIB})
else()
    message(STATUS "Tesseract: from pkg-config")
    pkg_check_modules(TESSERACT REQUIRED tesseract lept)
endif()

# The pure layers: no Tesseract, no OpenCV, no Qt Quick. Everything that makes a
# decision lives here so tests/ can reach it with plain Qt5, which is the only
# feedback this project gets before a tag.
set(CORE_SOURCES
    src/ocrresult.cpp
    src/textlayout.cpp
    src/fieldparser.cpp
    src/imageprep.cpp
)

set(CORE_HEADERS
    src/ocrresult.h
    src/textlayout.h
    src/fieldparser.h
    src/imageprep.h
)

set(SOURCES
    src/main.cpp
    src/logging.cpp
    src/settings.cpp
    src/ocrengine.cpp
    src/historystore.cpp
    src/pdfexport.cpp
    ${CORE_SOURCES}
)

set(HEADERS
    src/logging.h
    src/settings.h
    src/ocrengine.h
    src/historystore.h
    src/pdfexport.h
    ${CORE_HEADERS}
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Single source for the version shown on the About page: project() above
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TESSERACT_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Gui
    Qt5::Qml
    Qt5::Quick
    Qt5::Concurrent
    Qt5::Sql
    ${TESSERACT_LIBRARIES}
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

if(EXISTS ${THIRDPARTY_DIR}/tesseract)
    file(GLOB BUNDLED_LIBS
        ${THIRDPARTY_DIR}/tesseract/lib/libtesseract.so*
        ${THIRDPARTY_DIR}/leptonica/lib/liblept.so*)
    install(FILES ${BUNDLED_LIBS}
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}
    )
    # So the loader finds them without a wrapper script setting LD_LIBRARY_PATH.
    # Same libdir as above, or the rpath points at a directory that is only
    # correct on one of the two architectures.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

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
