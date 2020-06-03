#pragma once

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/KatePublicParameters.h>

using Dkg::KatePublicParameters;

namespace libpolycrypto {
namespace KZG {
    /**
     * Commits to the polynomial 'f' in the group 'Group' using KZG public params 'kpp'.
     */
    template<typename Group>
    Group commit(const KatePublicParameters& kpp, std::vector<Fr>& f) { 
        return multiExp<Group>(
            kpp.g1si.begin(),
            kpp.g1si.begin() + static_cast<long>(f.size()),
            f.begin(),
            f.end());
    }

    /**
     * Evaluates f(x) at x = point by dividing:
     *
     *    q = f / (x - point),
     *    r = f mod (x - point) = f(point)
     *
     * Returns the quotient polynomial in q and the evaluation in r
     */
    void naiveEval(const Fr& point, const std::vector<Fr>& f, std::vector<Fr>& q, Fr& eval);

    /**
     * Returns the proof for f_id(point) and f_id(point) itself.
     */
    std::tuple<G1, Fr> naiveProve(const KatePublicParameters& kpp, const std::vector<Fr>& f, const Fr& point);

    /**
     * Verifies that value = p(point), where p is committed in polyComm.
     *
     * @param g2toS     g2^s verification key for reconstructing g2^{s - point}
     */
    bool verifyProof(const G1& polyComm, const G1& proof, const Fr& value, const Fr& point, const G2& g2toS);

    /**
     * Verifies that valComm = g^p_j(z), where p_j is committed in polyComm and z is committed in acc = g2^{s - z}.
     */
    bool verifyProof(const G1& polyComm, const G1& proof, const G1& valComm, const G2& acc);

} // end of KZG
} // end of libpolycrypto
