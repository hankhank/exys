# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets)
find_package(Qt5Core)
find_package(Qt5Network)

#SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
#SET(BUILD_SHARED_LIBRARIES OFF)
#SET(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(exys_editor main.cc mainwindow.cc highlighter.cc linetextedit.cc)

# Use the Widgets module from Qt 5.
#target_link_libraries(exys_editor ${LIBPCRE_LIBRARIES})

target_link_libraries(exys_editor Qt5::Core Qt5::Widgets exys qtgv)

add_subdirectory(qtgv)
