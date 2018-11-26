#include <polycrypto/Configuration.h>

#include <polycrypto/NizkPok.h>

#include <polycrypto/internal/PicoSha2.h>

namespace libpolycrypto {

    /**
     * Returns H(g, g^x, g^k) as a field element.
     *
     * @param   g       g
     * @param   gToX    g^x
     * @param   r       r = g^k 
     */
    static Fr nizkHash(const G1& g, const G1& gToX, const G1& gToK) {
        // convert (g, g^x, g^k) to a string
        std::stringstream ss;
        ss << g << "|" << gToX << "|" << gToK;

        // hash string, but output a hex string not bytes
        std::string hex;
        picosha2::hash256_hex_string(ss.str(), hex);
        hex.pop_back(); // small enough for BN-P254 field

        // convert hex to mpz_t
        mpz_t rop;
        mpz_init(rop);
        mpz_set_str(rop, hex.c_str(), 16);

        // convert mpz_t to Fr
        Fr fr = libff::bigint<Fr::num_limbs>(rop);
        mpz_clear(rop);

        return fr;
    }

    NizkPok NizkPok::prove(const G1& g, const Fr& x, const G1& gToX) {
        /**
         * Picks random k \in Fr
         * Sets r = g^k
         * Sets h = H(g, g^x, r)
         * Sets s = k - h*x
         */
        Fr k = Fr::random_element();
        G1 r = k * g;
        Fr h = nizkHash(g, gToX, r);

        return NizkPok(h, k - h*x);
    }

    NizkPok NizkPok::getDummyProof() {
        return NizkPok(Fr::random_element(), Fr::random_element());
    }

    bool NizkPok::verify(const NizkPok& pi, const G1& g, const G1& gToX) {
        auto lhs = nizkHash(g, gToX, (pi.s * g) + (pi.h * gToX));

        return lhs == pi.h;
    }
}
