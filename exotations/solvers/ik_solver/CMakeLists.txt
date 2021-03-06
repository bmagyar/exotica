cmake_minimum_required(VERSION 2.8.3)
project(ik_solver)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
set(CMAKE_EXE_LINKER_FLAGS -Wl,--no-as-needed)
set(CMAKE_SHARED_LINKER_FLAGS -Wl,--no-as-needed)
set(CMAKE_MODULE_LINKER_FLAGS -Wl,--no-as-needed)

find_package(catkin REQUIRED COMPONENTS
  exotica
  roscpp
  roslib
  task_definition
  task_map
)


find_package(Boost REQUIRED COMPONENTS signals)

catkin_package(
INCLUDE_DIRS include
LIBRARIES ik_solver
CATKIN_DEPENDS exotica roscpp roslib task_definition task_map
DEPENDS system_lib
)

include_directories(include)
include_directories(
  ${catkin_INCLUDE_DIRS}
)

add_library(ik_solver
  src/ik_solver/ik_solver.cpp
  src/ik_solver/ik_problem.cpp
)

target_link_libraries(ik_solver
  ${catkin_LIBRARIES} ${Boost_LIBRARIES} ${LAPACK_LIBRARIES}
)

install(TARGETS ik_solver
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})
install(DIRECTORY include/ DESTINATION include)
