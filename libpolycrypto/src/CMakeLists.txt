#
# Targets
#

add_library(polycrypto 
    AmtDkg.cpp
    PolyCrypto.cpp
    FFThresh.cpp
    KateDkg.cpp
    KatePublicParameters.cpp
    Lagrange.cpp
    NizkPok.cpp
    PolyOps.cpp
    Utils.cpp
)

target_include_directories(polycrypto PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>"
    "$<INSTALL_INTERFACE:${INSTALL_CMAKE_DIR}/include>"
)

target_link_libraries(polycrypto PUBLIC
    ntl ff pthread zm gmp gmpxx xutils xassert
)

include(FindOpenMP)
if(OPENMP_FOUND)
    target_link_libraries(polycrypto PUBLIC gomp)
endif()

#target_link_libraries(polycrypto PUBLIC Threads::Threads)

#
# Installation
# TODO: Add <Package>Config[Version].cmake files so this package can be easily imported?
# (See https://cmake.org/cmake/help/git-master/manual/cmake-packages.7.html#creating-packages)
#

# This creates the <Package>Config.cmake file and installs it
install(TARGETS polycrypto EXPORT polycryptoConfig 
   ARCHIVE DESTINATION lib)
install(EXPORT polycryptoConfig DESTINATION lib/cmake/polycrypto)

# This installs the static or (/and?) dynamic library
install(TARGETS polycrypto 
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# This installs the headers
# WARNING: Don't add / at the end. No slash means polycrypto/ directory is created in the destination path
install(DIRECTORY include/polycrypto DESTINATION include)
