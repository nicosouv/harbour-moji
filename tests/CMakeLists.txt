# Tests build on their own, without the app.
#
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
#
# Nothing here needs the Sailfish SDK, a cross-compiler, a device - or Tesseract.
# Plain Qt5 on any machine. That is the whole point twice over: the RPM is only
# ever built in CI, so this is the only feedback available before a tag; and the
# layers worth testing were written so they do not touch the OCR library, which is
# why they can be reached at all.
cmake_minimum_required(VERSION 3.5)
project(harbour-moji-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Test)

enable_testing()

set(MOJI_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

# IBAN mod-97, Luhn, ISBN-10/13 and the four MRZ check digits, against published
# reference values. The point of the layer is that it says "found, but wrong", so
# the negative cases matter as much as the positive ones.
add_executable(tst_fieldparser
    ${CMAKE_CURRENT_LIST_DIR}/tst_fieldparser.cpp
    ${MOJI_SRC}/fieldparser.cpp
)
target_include_directories(tst_fieldparser PRIVATE ${MOJI_SRC})
target_link_libraries(tst_fieldparser Qt5::Core Qt5::Test)
add_test(NAME fieldparser COMMAND tst_fieldparser)

# Tap-to-extract: which word a point lands on, how far a fingertip may miss by,
# and what "the paragraph" means when the page has two of them. Needs Qt5::Gui for
# QRect and QPoint only - no widgets, no window.
add_executable(tst_textlayout
    ${CMAKE_CURRENT_LIST_DIR}/tst_textlayout.cpp
    ${MOJI_SRC}/textlayout.cpp
    ${MOJI_SRC}/ocrresult.cpp
)
target_include_directories(tst_textlayout PRIVATE ${MOJI_SRC})
target_link_libraries(tst_textlayout Qt5::Core Qt5::Gui Qt5::Test)
add_test(NAME textlayout COMMAND tst_textlayout)

# Preparing a photo for recognition, and - the part that matters - mapping boxes
# back from the scaled image to the original. An overlay drawn from unmapped
# boxes lands nearly right, which reads as a rendering quirk rather than a bug.
add_executable(tst_imageprep
    ${CMAKE_CURRENT_LIST_DIR}/tst_imageprep.cpp
    ${MOJI_SRC}/imageprep.cpp
)
target_include_directories(tst_imageprep PRIVATE ${MOJI_SRC})
target_link_libraries(tst_imageprep Qt5::Core Qt5::Gui Qt5::Test)
add_test(NAME imageprep COMMAND tst_imageprep)
