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

find_package(Qt5 REQUIRED COMPONENTS Core Gui Sql Test)

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

# Correcting a misread word, and the chain it has to travel: the raw text, any
# selection containing it, and the checksummed fields. The case worth having is a
# misread IBAN turning from "does not match" to "verified".
add_executable(tst_correction
    ${CMAKE_CURRENT_LIST_DIR}/tst_correction.cpp
    ${MOJI_SRC}/ocrresult.cpp
    ${MOJI_SRC}/textlayout.cpp
    ${MOJI_SRC}/fieldparser.cpp
)
target_include_directories(tst_correction PRIVATE ${MOJI_SRC})
target_link_libraries(tst_correction Qt5::Core Qt5::Gui Qt5::Test)
add_test(NAME correction COMMAND tst_correction)

# What has been read before. Run against a real SQLite database in memory: the
# interesting rules are SQL ones - re-reading a page replaces its entry rather
# than adding a second, and the list stays bounded.
add_executable(tst_historystore
    ${CMAKE_CURRENT_LIST_DIR}/tst_historystore.cpp
    ${MOJI_SRC}/historystore.cpp
    ${MOJI_SRC}/logging.cpp
)
target_include_directories(tst_historystore PRIVATE ${MOJI_SRC})
target_link_libraries(tst_historystore Qt5::Core Qt5::Sql Qt5::Test)
add_test(NAME historystore COMMAND tst_historystore)

# Laying the recognised text invisibly over the photo. The geometry is the whole
# test: a PDF that is slightly wrong here looks perfect - it is the photograph -
# and only misbehaves when somebody searches it.
add_executable(tst_pdfexport
    ${CMAKE_CURRENT_LIST_DIR}/tst_pdfexport.cpp
    ${MOJI_SRC}/pdfexport.cpp
    ${MOJI_SRC}/ocrresult.cpp
    ${MOJI_SRC}/logging.cpp
)
target_include_directories(tst_pdfexport PRIVATE ${MOJI_SRC})
target_link_libraries(tst_pdfexport Qt5::Core Qt5::Gui Qt5::Test)
add_test(NAME pdfexport COMMAND tst_pdfexport)

# Reading a table out of a block that never declared itself one: the column
# boundaries are the vertical channels no word crosses. The interesting cases are
# the refusals - a paragraph is not a table - and CSV escaping, where an
# unescaped European price silently splits a column.
add_executable(tst_tableextract
    ${CMAKE_CURRENT_LIST_DIR}/tst_tableextract.cpp
    ${MOJI_SRC}/tableextract.cpp
    ${MOJI_SRC}/ocrresult.cpp
)
target_include_directories(tst_tableextract PRIVATE ${MOJI_SRC})
target_link_libraries(tst_tableextract Qt5::Core Qt5::Gui Qt5::Test)
add_test(NAME tableextract COMMAND tst_tableextract)

# Mapping a field's offset in the joined text back to the words it came from,
# which is what lets a black box land on the right part of the photo. The join
# inserts separators that exist in the string and nowhere in the image, so an
# off-by-one puts the box on the neighbouring word - a redaction failure that
# looks like a success.
add_executable(tst_redact
    ${CMAKE_CURRENT_LIST_DIR}/tst_redact.cpp
    ${MOJI_SRC}/ocrresult.cpp
    ${MOJI_SRC}/fieldparser.cpp
)
target_include_directories(tst_redact PRIVATE ${MOJI_SRC})
target_link_libraries(tst_redact Qt5::Core Qt5::Gui Qt5::Test)
add_test(NAME redact COMMAND tst_redact)

# Naming a Tesseract language code. The picker listed "fra", "ces", "ell" and
# "chi_sim" because QLocale cannot resolve ISO 639-2/T, and the same hole made a
# French phone default to reading English. One assertion here tests the premise
# rather than the code: if a future Qt learns those codes, the table can go.
add_executable(tst_languagenames
    ${CMAKE_CURRENT_LIST_DIR}/tst_languagenames.cpp
    ${MOJI_SRC}/languagenames.cpp
)
target_include_directories(tst_languagenames PRIVATE ${MOJI_SRC})
target_link_libraries(tst_languagenames Qt5::Core Qt5::Test)
add_test(NAME languagenames COMMAND tst_languagenames)

# Turning a text detector's probability map into boxes: the first piece of the
# scene-text engine, and the piece with no neural network in it. Every decision
# the detector's output stage makes is here - which pixels are text, how far a box
# is grown back past the blob DBNet was trained to shrink, and the scaling from
# the model's fixed input size to the photograph, which is the one that fails by
# landing every box slightly wrong rather than by failing.
add_executable(tst_scenetext
    ${CMAKE_CURRENT_LIST_DIR}/tst_scenetext.cpp
    ${MOJI_SRC}/scenetext.cpp
)
target_include_directories(tst_scenetext PRIVATE ${MOJI_SRC})
target_link_libraries(tst_scenetext Qt5::Core Qt5::Gui Qt5::Test)
add_test(NAME scenetext COMMAND tst_scenetext)

# Choosing the resolution to rasterise a PDF page at. Invented from the page's
# size in points, and quiet in both directions when wrong: too low reads as a bad
# PDF, too high reads as a crash. The table case asserts the property that
# actually protects the app - whatever page it is handed, the render fits.
add_executable(tst_pdfpage
    ${CMAKE_CURRENT_LIST_DIR}/tst_pdfpage.cpp
    ${MOJI_SRC}/pdfpage.cpp
)
target_include_directories(tst_pdfpage PRIVATE ${MOJI_SRC})
target_link_libraries(tst_pdfpage Qt5::Core Qt5::Gui Qt5::Test)
add_test(NAME pdfpage COMMAND tst_pdfpage)
