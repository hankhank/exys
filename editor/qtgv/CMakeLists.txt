
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets)
find_package(Qt5Core)
find_package(GraphViz)

SET(TARGET_NAME "qtgv")

INCLUDE_DIRECTORIES(${GRAPHVIZ_INCLUDE_DIRS})
# Activate Export macro for building the library itself
# if QtGvCORE_LIB is not set or equal 0 (null), import macro is used

ADD_LIBRARY(qtgv STATIC 
  qtgv_edge.cc
  qtgv_node.cc
  qtgv_scene.cc
)

TARGET_LINK_LIBRARIES(qtgv Qt5::Widgets Qt5::Core 
    ${GRAPHVIZ_GVC_LIBRARY}
    ${GRAPHVIZ_CGRAPH_LIBRARY}
    ${GRAPHVIZ_CDT_LIBRARY}
)

target_include_directories (qtgv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${GRAPHVIZ_INCLUDE_DIRS})

