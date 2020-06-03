# libpolycrypto 

## WARNING

There seems to be a bug when benchmarking both VSS and DKG at large scales ($t \ge 131072$) with `BenchVSS` and `BenchDKG`.
Specifically, a proof fails verification during the reconstruction (for VSS) or worst-case reconstruction for (DKG).

 - Cannot reproduce at small scales
 - It occurs for both AMT and for FK
 - It also occurred for AMT simulated mode ($t=524288$, $n=2^{20}$)
 - It does _not_ occur in `TestFk` for example
 - Tried debugging by enabling lots of compiler checks to ensure there are no out-of-bounds error, but no luck.

## Build on Linux

Step zero is to clone this repo and `cd` to the right directory:

    cd <wherever-you-cloned-libpolycrypto>

If you're running OS X, make sure you have the Xcode **and** the Xcode developer tools installed:

    xcode-select --install

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

