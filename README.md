# libpolycrypto 

## Build on Linux

Step zero is to clone this repo and `cd` to the right directory:

    cd <wherever-you-cloned-libpolycrypto>

First, install deps using:

    scripts/linux/install-libs.sh
    scripts/linux/install-deps.sh

Then, set up the environment. This **will store the built code in ~/builds/polycrypto/**:

    . scripts/linux/set-env.sh release

...you can also use `debug`, `relwithdebug` or `trace` as an argument for `set-env.sh`.

To build:

    make.sh

..tests, benchmarks and any other binaries are automatically added to `PATH` but can also be found in

    cd ~/builds/polycrypto/master/release/libpolycrypto/bin/

(or replace `release` with `debug` or whatever you used in `set-env.sh`)

## Useful scripts

There's a bunch of useful scripts in `scripts/linux/`:

 - `plot-*.py` for generating plots
 - `cols.sh, dkg-cols.sh, vss-cols.sh` for viewing CSV data in the terminal
 - `generate-qsbdh-params.sh` for generating public parameters for the VSS/DKG code

## Git submodules

This is just for reference purposes. 
No need to execute these.
To fetch the submodules, just do:

    git submodule init
    git submodule update

For historical purposes, (i.e., don't execute these), when I set up the submodules, I did:
    
    cd depends/
    git submodule add git://github.com/herumi/ate-pairing.git
    git submodule add git://github.com/herumi/xbyak.git
    git submodule add git://github.com/scipr-lab/libff.git
    git submodule add https://github.com/scipr-lab/libfqfft.git

To update your submodules with changes from their upstream github repos, do:

    git submodule foreach git pull origin master

