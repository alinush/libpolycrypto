#!/bin/bash
set -e

scriptdir=$(cd $(dirname $0); pwd -P)
sourcedir=$(cd $scriptdir/../..; pwd -P)

. $scriptdir/shlibs/os.sh

CMAKE_CXX_FLAGS=
if [ "$OS" = "OSX" -a "$OS_FLAVOR" = "Catalina" ]; then
    echo "Setting proper env. vars for compiling against OpenSSL in OS X Catalina"
    export PKG_CONFIG_PATH="/usr/local/opt/openssl@1.1/lib/pkgconfig"

    # NOTE: None of these seem to help cmake find OpenSSL
    #export LDFLAGS="-L/usr/local/opt/openssl@1.1/lib"
    #export CXXFLAGS="-I/usr/local/opt/openssl@1.1/include"
    #CMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS -I/usr/local/opt/openssl@1.1/include"
fi

CLEAN_BUILD=0
if [ "$1" == "debug" ]; then
    echo "Debug build..."
    echo
    ATE_PAIRING_FLAGS="DBG=on"
elif [ "$1" == "clean" ]; then
    CLEAN_BUILD=1
fi

# NOTE: Seemed like setting this to OFF caused a simple cout << G1::zero(); to segfault but it was just that I forgot to call init_public_params()
BINARY_OUTPUT=OFF
NO_PT_COMPRESSION=ON
MULTICORE=ON
if [ "$OS" == "OSX" ]; then
    # libff's multicore implementation uses OpenMP which clang on OS X apparently doesn't support
    MULTICORE=OFF
fi

(
    cd $sourcedir/
    git submodule init
    git submodule update
)

# install libxassert and libxutils

(
    cd /tmp
    mkdir -p libpolycrypto-deps/
    cd libpolycrypto-deps/

    for lib in `echo libxassert; echo libxutils`; do
        if [ ! -d $lib ]; then
            git clone https://github.com/alinush/$lib.git
        fi

        echo
        echo "Installing $lib..."
        echo

        (
            cd $lib/
            mkdir -p build/
            cd build/
            cmake -DCMAKE_BUILD_TYPE=Release ..
            cmake --build .
            sudo cmake --build . --target install
        )
    done
    
)

cd $sourcedir/depends

# NOTE TO SELF: After one day of trying to get CMake to add these using ExternalProject_Add (or add_subdirectory), I give up.

echo "Installing ate-pairing..."
(
    cd ate-pairing/
    if [ $CLEAN_BUILD -eq 1 ]; then
        echo "Cleaning previous build..."
        echo
        make clean
    fi
    make -j $NUM_CPUS -C src \
        SUPPORT_SNARK=1 \
        $ATE_PAIRING_FLAGS

    INCL_DIR=/usr/local/include/ate-pairing
    sudo mkdir -p "$INCL_DIR"
    sudo cp include/bn.h  "$INCL_DIR"
    sudo cp include/zm.h  "$INCL_DIR"
    sudo cp include/zm2.h "$INCL_DIR"
    # WARNING: Need this due to a silly #include "depends/[...]" directive from libff
    # (/usr/local/include/libff/algebra/curves/bn128/bn128_g1.hpp:12:44: fatal error: depends/ate-pairing/include/bn.h: No such file or directory)
    sudo mkdir -p "$INCL_DIR/../depends/ate-pairing/"
    sudo ln -sf "$INCL_DIR" "$INCL_DIR/../depends/ate-pairing/include"

    LIB_DIR=/usr/local/lib
    sudo cp lib/libzm.a "$LIB_DIR"
    # NOTE: Not sure why, but getting error at runtime that this cannot be loaded. Maybe it should be zm.so?
    #sudo cp lib/zm.so "$LIB_DIR/libzm.so"
)

echo "Installing libff..."
(
    cd libff/
    if [ $CLEAN_BUILD -eq 1 ]; then
        echo "Cleaning previous build..."
        echo
        rm -rf build/
    fi
    mkdir -p build/
    cd build/
    # WARNING: Does not link correctly with -DPERFORMANCE=ON
    cmake \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DIS_LIBFF_PARENT=OFF \
        -DBINARY_OUTPUT=$BINARY_OUTPUT \
        -DNO_PT_COMPRESSION=$NO_PT_COMPRESSION \
        -DCMAKE_CXX_FLAGS="-Wno-unused-parameter -Wno-unused-value -Wno-unused-variable -I$sourcedir $CMAKE_CXX_FLAGS" \
        -DUSE_ASM=ON \
        -DPERFORMANCE=OFF \
        -DMULTICORE=$MULTICORE \
        -DCURVE="BN128" \
        -DWITH_PROCPS=OFF ..

    make -j $NUM_CPUS
    sudo make install
)

echo "Installing libfqfft..."
(
    # NOTE: Headers-only library
    cd libfqfft/
    
    INCL_DIR=/usr/local/include/libfqfft
    sudo mkdir -p "$INCL_DIR"
    sudo cp -r libfqfft/* "$INCL_DIR/"
    sudo rm "$INCL_DIR/CMakeLists.txt"
)
