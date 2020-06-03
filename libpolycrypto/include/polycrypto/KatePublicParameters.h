#pragma once

#include <memory>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/NtlLib.h>

#include <xutils/Log.h>
#include <xutils/Timer.h>

// TODO: move to KZG.h? Or just add KZG namespace here?

namespace Dkg {

using libpolycrypto::Fr;
using libpolycrypto::G1;
using libpolycrypto::G2;
using libpolycrypto::ReducedPairing;
using libpolycrypto::multiExp;
using libpolycrypto::convNtlToLibff;

class KatePublicParameters {
public:
    size_t q;
    std::vector<G1> g1si;   // g1si[i] = g1^{s^i}
    std::vector<G2> g2si;   // g2si[i] = g2^{s^i}
    Fr s;

public:
    /**
     * Generates Kate public parameters on the fly. Used for testing.
     */
    KatePublicParameters(size_t q)
        : q(q)
    {
        resize(q);

        // pick trapdoor s
        s = Fr::random_element();

        Fr si = s^0;
        int prevPct = -1;
        size_t c = 0;

        loginfo << "Generating q-SDH params, q = " << q << endl;
        for (size_t i = 0; i <= q; i++) {
            g1si[i] = si * G1::one();
            g2si[i] = si * G2::one();
        
            si *= s;

            c++;

            int pct = static_cast<int>(static_cast<double>(c)/static_cast<double>(q + 1) * 100.0);
            if(pct > prevPct) {
                logdbg << pct << "% ... (i = " << i << " out of " << q << ")" << endl;
                prevPct = pct;
            }
        }
    }

    /**
     * Reads s, tau and q from trapFile.
     * Then reads the q-PKE parameters from trapFile + "-0", trapFile + "-1", ...
     * and so on, until q+1 parameters are read: g, g^s, \dots, g^{s^q}.
     */
    KatePublicParameters(const std::string& trapFile, size_t maxQ = 0, bool progress = true, bool verify = false);

public:
    static Fr generateTrapdoor(size_t q, const std::string& outFile);

    static void generate(size_t startIncl, size_t endExcl, const Fr& s, const std::string& outFile, bool progress);
    

public:
    /** 
     * Returns all the h[i]'s for precomputing Kate proofs fast 
     * but cheats by using trapdoor to compute them very fast.
     * (see "Fast amortized Kate proofs" by Feist and Khovratovich)
     */
    std::vector<G1> computeAllHisWithTrapdoor(const std::vector<Fr>& f) {
        // make sure the degree of f is <= q
        testAssertLessThanOrEqual(f.size(), q + 1);

        std::vector<G1> h;
        if(f.size() == 0)
            throw std::invalid_argument("f must not be empty");

        size_t deg = f.size() - 1;
        size_t k = deg;
        h.push_back(f[k] * g1si[0]);

        for(size_t i = 1; i < deg; i++) {
            k--;
            h.push_back(s * h.back() + f[k] * G1::one());
        }

        // we computed the h[i]'s in reverse
        std::reverse(h.begin(), h.end());

        assertEqual(h.size(), deg);

        return h;
    }
    
    /** 
     * Returns all the h[i]'s for precomputing Kate proofs fast 
     * without cheating by only using the g1^{s^i}'s.
     * (see "Fast amortized Kate proofs" by Feist and Khovratovich)
     */
    std::vector<G1> computeAllHis(const std::vector<Fr>& f) const;

    void resize(size_t q) {
        g1si.resize(q+1); // g1^{s^i} with i from 0 to q, including q
        g2si.resize(q+1); // g2^{s^i} with i from 0 to q, including q
    }

    G1 getG1toS() const {
        return g1si[1];
    }

    G2 getG2toS() const {
        assertStrictlyGreaterThan(g2si.size(), 1);
        return g2si[1];
    }
 
    const Fr& getTrapdoor() const {
        return s;
    }
};

}
