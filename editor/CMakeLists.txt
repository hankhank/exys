# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets)
find_package(Qt5Core)
find_package(Qt5Network)

add_executable(exys_editor main.cc mainwindow.cc highlighter.cc )

target_link_libraries(exys_editor exys qtgv)

# Use the Widgets module from Qt 5.
target_link_libraries(exys_editor Qt5::Widgets)
target_link_libraries(exys_editor Qt5::Core)

add_subdirectory(qtgv)
