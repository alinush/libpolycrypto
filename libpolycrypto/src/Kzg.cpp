#include <polycrypto/Configuration.h>

#include <polycrypto/KZG.h>

#include <libfqfft/polynomial_arithmetic/basic_operations.hpp>

namespace libpolycrypto {
namespace KZG {

    void naiveEval(const Fr& point, const std::vector<Fr>& f, std::vector<Fr>& q, Fr& eval) {
        std::vector<Fr> monom(2), rem(1);

        // set the (x - point) divisor
        monom[0] = -point;
        monom[1] = Fr::one();

        // divides f by (x - point)
        libfqfft::_polynomial_division(q, rem, f, monom);
        assertEqual(q.size(), f.size() - 1);
        assertEqual(rem.size(), 1);

        eval = rem[0];
    }

    std::tuple<G1, Fr> naiveProve(const KatePublicParameters& kpp, const std::vector<Fr>& f, const Fr& point) {
        std::vector<Fr> q;
        Fr r;

        naiveEval(point, f, q, r);

        // commit to quotient polynomial
        auto proof = commit<G1>(kpp, q);

        return std::make_tuple(proof, r);
    }

    bool verifyProof(const G1& polyComm, const G1& proof, const Fr& value, const Fr& point, const G2& g2toS) {
        return verifyProof(polyComm, proof, value * G1::one(), g2toS - point * G2::one());
    }

    bool verifyProof(const G1& polyComm, const G1& proof, const G1& valComm, const G2& acc) {
        return ReducedPairing(polyComm - valComm, G2::one()) ==
               ReducedPairing(proof, acc);
    }
} // end of KZG
} // end of libpolycrypto
