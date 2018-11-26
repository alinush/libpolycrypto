#!/bin/bash

if [ $# -lt 1 ]; then
    name=${BASH_SOURCE[@]}
    echo "Usage: source $name <buildtype> [<extra-cmake-args> ...]"
    echo
    echo "Examples:"
    echo
    echo "source $name debug"
    echo "source $name trace"
    echo "source $name release"
    echo "source $name relwithdebug"
    echo
    if [ -n "$POLYCRYPTO_HAS_ENV_SET" ]; then
        echo "Currently-set environment"
        echo "========================="
        echo "Build directory: $POLYCRYPTO_BUILD_DIR"
        echo "CMake args: $POLYCRYPTO_CMAKE_ARGS"
    fi
else
    # WARNING: Need to exit using control-flow rather than 'exit 1' because this script is sourced
    invalid_buildtype=0

    buildtype=`echo "$1" | tr '[:upper:]' '[:lower:]'`
    shift;

    extra_cmake_args=$@

    scriptdir=$(cd $(dirname ${BASH_SOURCE[@]}); pwd -P)
    sourcedir=$(cd $scriptdir/../..; pwd -P)
    #echo "Source dir: $sourcedir"

    branch=`git branch | grep "\*"`
    branch=${branch/\* /}
    builddir_base=~/builds/polycrypto/$branch
    case "$buildtype" in
    trace)
        builddir=$builddir_base/trace
        cmake_args="-DCMAKE_BUILD_TYPE=Trace"
        ;;
    debug)
        builddir=$builddir_base/debug
        cmake_args="-DCMAKE_BUILD_TYPE=Debug"
        ;;
    release)
        builddir=$builddir_base/release
        cmake_args="-DCMAKE_BUILD_TYPE=Release"
        ;;
    relwithdebug)
        builddir=$builddir_base/relwithdebug
        cmake_args="-DCMAKE_BUILD_TYPE=RelWithDebInfo"
        ;;
    *)
        invalid_buildtype=1
        ;;
    esac

    cmake_args="$cmake_args $extra_cmake_args"

    #
    # grep-code alias
    #
    alias grep-code='grep --exclude-dir=.git --exclude=".gitignore" --exclude="*.html"'

    if [ $invalid_buildtype -eq 0 ]; then
        echo "Configuring for build type '$buildtype' ..."
        echo

        export PATH="$scriptdir:$PATH"
        export PATH="$builddir/libpolycrypto/bin:$PATH"
        export PATH="$builddir/libpolycrypto/bin/test:$PATH"
        export PATH="$builddir/libpolycrypto/bin/bench:$PATH"
        export PATH="$builddir/libpolycrypto/bin/app:$PATH"
        export PATH="$builddir/libpolycrypto/bin/examples:$PATH"

        echo "Build directory: $builddir"
        echo "CMake args: $cmake_args"
        echo "PATH envvar: $PATH" 

        export POLYCRYPTO_BUILD_DIR=$builddir
        export POLYCRYPTO_CMAKE_ARGS=$cmake_args
        export POLYCRYPTO_HAS_ENV_SET=1
        export POLYCRYPTO_SOURCE_DIR=$sourcedir
    else
        echo "ERROR: Invalid build type '$buildtype'"
    fi
fi
