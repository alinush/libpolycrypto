#pragma once

#include <memory>
#include <algorithm>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/DkgCommon.h>
#include <polycrypto/AbstractPlayer.h>
#include <polycrypto/KatePublicParameters.h>
#include <polycrypto/NizkPok.h>

#include <xutils/Log.h>
#include <xutils/Timer.h>
#include <xassert/XAssert.h>

namespace Dkg {

using libpolycrypto::Fr;
using libpolycrypto::G1;
using libpolycrypto::G2;
using libpolycrypto::GT;
using libpolycrypto::multiExp;
using libpolycrypto::NizkPok;

/**
 * Both KatePlayer and AmtPlayer inherit from this, since they all use
 * Kate et al polynomial commitments.
 */
template<class AllProofsType, class ProofType, class ParentPlayerClass>
class AbstractKatePlayer : public AbstractPlayer {
public:
    G1 comm, f0comm, pkProof;// Kate commitment g^f_id(s) to f_id(.) and Feldman commitment g^{f_id(0)} to f_id(0)
    G1 commFinal;            // final commitment to f(x) = \sum_{j \in IDs} f_j(x)
    ProofType proofFinal;    // this player's final proof for his share f(id) of f(x) above
    NizkPok nizkpok;         // NIZKPoK for g^{f_id(0)}

    const KatePublicParameters& kpp;

    std::unique_ptr<AllProofsType> allProofs; // allProofs[i] stores proof for f_id(j) for all j and proof[0] stores the proof for the secret f_id(0)

    using AbstractKatePlayerType = AbstractKatePlayer<AllProofsType, ProofType, ParentPlayerClass>;

public:
    /**
     * A player in a Kate DKG.
     */
    AbstractKatePlayer(const DkgParams& params, const KatePublicParameters& kpp, size_t id, bool isSimulated, bool isDkgPlayer)
        : AbstractPlayer(params, id, isSimulated, isDkgPlayer), commFinal(G1::zero()), kpp(kpp)
    {
        size_t maxPolyDeg = params.t - 1;

        // the polynomials being secret shared have degree = # of players who can reconstruct - 1
        if(kpp.g1si.size() < maxPolyDeg + 1) {
            throw std::runtime_error("Need more public parameters for the specified threshold");
        }
    }

public:
    virtual void evaluate() {
    }

    /**
     * Evaluates f_id(x) at x = point by dividing f_id / (x - point),
     * returning the quotient in q and the evaluation eval = f_id(point).
     */
    void kateEval(const Fr& point, std::vector<Fr>& q, Fr& eval) {
        // WARNING: Not thread-safe unless we remove 'static'
        static std::vector<Fr> monom(2), rem(1);

        // set the (x - point) divisor
        monom[0] = -point;
        monom[1] = Fr::one();

        // divides f_id by (x - point)
        libfqfft::_polynomial_division(q, rem, f_id, monom);
        assertEqual(q.size(), f_id.size() - 1);
        assertEqual(rem.size(), 1);

        eval = rem[0];
    }

    /**
     * Returns the proof for f_id(point) and f_id(point) itself.
     */
    std::tuple<G1, Fr> kateProve(const Fr& point) {
        // WARNING: Not thread-safe unless we remove 'static'
        static std::vector<Fr> q;
        static Fr r;

        kateEval(point, q, r);

        // commit to quotient polynomial
        auto proof = multiExp<G1>(
            kpp.g1si.begin(),
            kpp.g1si.begin() + static_cast<long>(q.size()),
            q.begin(),
            q.end());

        return std::make_tuple(proof, r);
    }

    /**
     * Subclasses like the MultipointDKG will override this with their own proofs
     */
    virtual void computeRealProofs() = 0; 

    virtual void computeSimulatedProofs() = 0;
    
    void commitAndNizkPok() {
        // commit to polynomial using Kate commitments
        // TODO: can be multithreaded
        comm = multiExp<G1>(
            kpp.g1si.begin(),
            kpp.g1si.begin() + static_cast<long>(f_id.size()),
            f_id.begin(),
            f_id.end());
        

        if(isDkgPlayer) {
            // commit to f_id(0), needed for computing final PK
            Fr f0 = getSecret();
            f0comm = f0 * G1::one();
            // compute NIZKPoK for f_id(0)
            nizkpok = NizkPok::prove(G1::one(), f0, f0comm);
        }
    }

    virtual void dealImpl() {
        commitAndNizkPok();
        // compute proofs for f_id(0) and f_id(j) for all players j
        computeRealProofs();
    }

    virtual void simulatedDealImpl() {
        commitAndNizkPok();
        computeSimulatedProofs();
    }

    bool verifyAllNizkPok(const std::vector<AbstractPlayer*>& allPlayers) {
        //ScopedTimer<std::chrono::microseconds> t(std::cout, "Verify all NIZK: ", " mus\n");
        assertTrue(isDkgPlayer);
        // TODO: can use DJB's fast batch verification method from the "high-speed high-security signatures" paper
        for(auto p : allPlayers) {
            assertNotNull(p);
            auto pp = dynamic_cast<ParentPlayerClass*>(p);
            assertNotNull(pp);

            if(isDkgPlayer && p->id == id) {
                logdbg << "Skipping verifying my own NIZKPoK (id = " << id << ")..." << endl;
                continue;
            }

            if(verifyNizkPok(*pp) == false)
                return false;
        }

        return true;
    }

    bool verifyNizkPok(const AbstractKatePlayer& p) {
        //ScopedTimer<std::chrono::microseconds> t(std::cout, "Verify NIZK: ", " mus\n");
        if(NizkPok::verify(p.nizkpok, G1::one(), p.f0comm) == false) {
            logerror << "Player " << id << " couldn't verify NIZKPoK for g^f_" << p.id << "(0)" << endl;
            return false;
        }

        return true;
    }

    /**
     * @param   p   the dealer
     */
    bool verifyShareFromDealer(const ParentPlayerClass& p) {
        //ScopedTimer<std::chrono::microseconds> t(std::cout, "Verify share: ", " mus\n");
        assertNotNull(p.allProofs);
        assertGreaterThanOrEqual(id, 0);

        // Let j = p.id denote the other player's ID
        // Verifies this player share on player j's f_j(x)
        const Fr& myShare = p.shares[id];  // recall that share[id] = share of this player on player j's f_j(x)
        ProofType myProof = p.allProofs->getPlayerProof(id);

        // Verify j's f_j(id)
        if(allProofs->verifyAtId(p.comm, myProof, myShare) == false) {
            logerror << "Proof for f_" << p.id << "(" << id << ") didn't verify" << endl;
            logerror << " - proof: " << myProof << endl;
            logerror << " - comm: " << p.comm << endl;
            logerror << " - share: " << myShare << endl;
            return false;
        }

        if(isDkgPlayer) {
            // Verify player j's g^f_j(0)
            if(allProofs->verifyAtZero(p.comm, p.allProofs->getZeroProof(), p.f0comm) == false) {
                logerror << "Proof for f_" << p.id << "(" << 0 << ") didn't verify" << endl;
                if(p.isSimulated()) {
                    logwarn << " + Player " << p.id << " is simulated!" << endl;
                }
                return false;
            }
        }

        return true;
    }
    
    /**
     * PERF: The performance of this call depends on the signer ID of the current player.
     * This is because the exponents used to verify the share will be smaller when the signer ID
     * is smaller.
     *
     * @param aggregate     true if proofs (for shares, for f(0), and for NIZKPoKs) should be aggregated and then verified (optimistic case)
     *                      false if proofs should be verified individually (approximation of worst-case; does not account for complaint broadcasting cost)
     */
    virtual bool verifyOtherPlayers(const std::vector<AbstractPlayer*>& allPlayers, bool aggregate) {
        assertNotNull(allProofs);

        // compute the final share, final poly commitment and final public key
        pk = G1::zero();
        pkProof = G1::zero();
        commFinal = G1::zero();
        proofFinal = ProofType();
        share = Fr::zero();

        // NOTE: Cannot be multithreaded unless we make these variables local I think
        // need to aggregate always, so any DKG player can have his final commitment/pk/share/proof and be able to reconstruct
        if(isDkgPlayer) {
            //ScopedTimer<std::chrono::microseconds> t(std::cout, "Aggregation: ", " mus\n");
            for(auto ap : allPlayers) {
                auto pp = dynamic_cast<ParentPlayerClass*>(ap);
                assertNotNull(pp);

                // TODO: but when aggregate == false, we'll be aggregating bad player shares here
                // however our code never tests/uses this case, so we're good for now.
                // (same problem in FeldmanDkg.h I think)
                const ParentPlayerClass& p = *pp;
                // aggregate proofs for this player's share on the final polynomial
                assertStrictlyLessThan(id, p.shares.size());
                share = share + p.shares[id];
                proofFinal = proofFinal + p.allProofs->getPlayerProof(id);
                commFinal = commFinal + p.comm;
                pk = pk + p.f0comm;
                pkProof = pkProof + p.allProofs->getZeroProof();
            }
        } else {
            assertEqual(allPlayers.size(), 1);
        }

        if(aggregate) {
            if(isDkgPlayer) {
                if(verifyAllNizkPok(allPlayers) == false)
                    return false;

                // verify \sum_j f_j(id) and \sum_j f_j(0)
                if(verifyFinalShareProof() == false)
                    return false;
            } else {
                throw std::runtime_error("What are you aggregating for if you're doing VSS?");
            }
        } else {
            // verify shares of players individually (meant to approximate worst-case DKG cost)
            // NOTE: Cannot be multithreaded because libff's reduced_pairing does not seem to be parallelizable
            for(auto ap : allPlayers) {
                auto p = dynamic_cast<ParentPlayerClass*>(ap);
                assertNotNull(p);

                // Skip verifying our own proofs
                if(isDkgPlayer && p->id == id) {
                    logdbg << "Skipping verifying my own poly proofs (id = " << id << ")..." << endl;
                    continue;
                }

                // If DKG, verify NIZKPoK for player j's f_j(0)
                if(isDkgPlayer) {
                    if(verifyNizkPok(*p) == false)
                        return false;
                }

                if(verifyShareFromDealer(*p) == false)
                    return false;
            }
        }

        return true;
    }

    virtual bool verifySharesReconstruction(const std::vector<size_t>& subset, bool fastTrack) = 0;

    /**
     * For AMT/Kate VSS fast-track, verifies the subset of t shares + interpolates from the subset.
     *  + Uses memoization for AMT
     * For AMT/Kate DKG fast-track, interpolates from the subset + one exp to check against final PK.
     * For AMT/Kate VSS/DKG worst-case, verifies all n shares + interpolates from the subset.
     *
     * Reconstruction is done using this player's shares, even though in a DKG it would be done using
     * the sum of all player shares.
     */
    virtual bool reconstructionVerify(const std::vector<size_t>& subset, bool fastTrack) {
        // this is the subset of player IDs that will be verified and used for interpolation
        assertEqual(subset.size(), params.t);
        // need this to be sorted to iterate faster in AMT worst-case
        assertTrue(std::is_sorted(subset.begin(), subset.end()));

        if(fastTrack) {
            if(!isDkgPlayer) {
                // will verify t shares
                return verifySharesReconstruction(subset, fastTrack);
            }
            return true;
        } else {
            //ScopedTimer<microseconds> t(std::cout, "AbstractKatePlayer::verifySharesReconstruction: ");

            // will verify n shares (with some optimizations for AMTs), since fastTrack = false
            return verifySharesReconstruction(subset, fastTrack);
        }
    }

    /**
     * Called during testing to verify that this player's share of the final agreed-upon secret has a valid proof.
     */
    // TODO: make sure callers check bool value
    virtual bool verifyFinalShareProof() {
        //ScopedTimer<std::chrono::microseconds> t(std::cout, "Verify final share: ", " mus\n");

        if(allProofs->verifyAtId(commFinal, proofFinal, share) == false) {
            logerror << "Proof for final \\sum_p p.f(" << id << ") didn't verify" << endl;
            return false;
        }

        // also verify final g^f(0) for DKGs
        if(isDkgPlayer) {
            if(allProofs->verifyAtZero(commFinal, pkProof, pk) == false) {
                logerror << "Proof for \\sum_p p.f(0) didn't verify for player " << id << endl;
                return false;
            }
        }

        return true;
    }
};


}
