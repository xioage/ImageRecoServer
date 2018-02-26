# Install script for directory: /home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

if(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Devel")
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/eigen3/Eigen" TYPE FILE FILES
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/StdVector"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/Eigenvalues"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/Householder"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/StdList"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/Eigen"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/Core"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/IterativeLinearSolvers"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/SparseLU"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/SVD"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/SparseCore"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/LU"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/Dense"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/CholmodSupport"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/PaStiXSupport"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/QR"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/Sparse"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/PardisoSupport"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/QtAlignedMalloc"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/OrderingMethods"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/Jacobi"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/SparseQR"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/MetisSupport"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/UmfPackSupport"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/SuperLUSupport"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/Geometry"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/StdDeque"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/SparseCholesky"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/Cholesky"
    "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/SPQRSupport"
    )
endif()

if(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Devel")
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/eigen3/Eigen" TYPE DIRECTORY FILES "/home/symlab/Downloads/eigen-eigen-67e894c6cd8f/Eigen/src" FILES_MATCHING REGEX "/[^/]*\\.h$")
endif()

