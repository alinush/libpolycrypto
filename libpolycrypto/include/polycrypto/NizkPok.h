#pragma once

#include <polycrypto/PolyCrypto.h>

namespace libpolycrypto {

    class NizkPok {
    public:
        Fr h;
        Fr s;

    public:
        NizkPok() {}
        NizkPok(const Fr& h, const Fr& s) : h(h), s(s) {}

    public:
        /**
         * Computes a NIZKPoK for x that verifies against g^x.
         * i.e., just a Schnorr signature s = k + H(m|g^k)x under x, where m = g|g^x
         */
        static NizkPok prove(const G1& g, const Fr& x, const G1& gToX);
        static NizkPok getDummyProof();

        /**
         * Verifies the NIZKPoK for g^x.
         * i.e., just a Schnorr signature verification: 1 hash + 2 exps + 1 group op
         *
         * @param   pi      the NIZKPoK
         * @param   gToX    g^x
         */
        static bool verify(const NizkPok& pi, const G1& g, const G1& gToX);
    };

}
