#pragma once

#include <memory>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/DkgCommon.h>
#include <polycrypto/AbstractKatePlayer.h>
#include <polycrypto/KateDkg.h>
#include <polycrypto/KatePublicParameters.h>
#include <polycrypto/RootsOfUnityEval.h>

#include <libff/common/utils.hpp> // libff::bitreverse(idx, numBits)

#include <xutils/Log.h>
#include <xutils/Timer.h>
#include <xassert/XAssert.h>

#include <boost/unordered_map.hpp>
#include <boost/range/join.hpp>

namespace Dkg {

using libpolycrypto::Fr;
using libpolycrypto::G1;
using libpolycrypto::G2;
using libpolycrypto::GT;
using libpolycrypto::ReducedPairing;
using libpolycrypto::RootsOfUnityEvaluation;
using libpolycrypto::AuthRootsOfUnityEvaluation;
using libpolycrypto::AuthAccumulatorTree;

/**
 * Represent a single AMT proof path.
 */
class AmtProof {
public:
    // NOTE: quoComms[0] stores the leaf quotient
    std::vector<G1> quoComms;

public:
    AmtProof() {}

public:
    /**
     * Combines this AMT proof for p(i) with another AMT proof for another q(i), obtaining
     * an AMT proof for (p+q)(i).
     */
    friend AmtProof operator+(const AmtProof& lhs, const AmtProof& rhs) {
        AmtProof res;
        if(lhs.quoComms.size() != 0 && rhs.quoComms.size() != 0) {
            testAssertEqual(lhs.quoComms.size(), rhs.quoComms.size());
            for(size_t i = 0; i < lhs.quoComms.size(); i++) {
                res.quoComms.push_back(lhs.quoComms[i] + rhs.quoComms[i]);
            }
        } else {
            assertFalse(rhs.quoComms.size() == 0 && lhs.quoComms.size() == 0);
            res = rhs.quoComms.size() == 0 ? lhs : rhs;
        }

        return res;
    }

public:
    friend std::ostream& operator<<(std::ostream& out, const AmtProof& pi);
};

/**
 * Stores all AMT proofs for shares p(w_N^id) on this players polynomial p(x).
 */
class AllAmtProofs {
public:
    const AuthAccumulatorTree& authAccs;
    
    std::vector<G2> accPathForId;

    size_t n;       // the number of players
    size_t numBits; // the number of bits in N
    size_t proofNumLevels; // the # of nodes/levels in an AMT proof
    
    // all proofs stored as an authenticated multipoint evaluation tree (AMT) of p(x) at all w_N^id
    std::unique_ptr<AuthRootsOfUnityEvaluation> authEval;

    // normal Kate et al proof for p(0)
    G1 f0proof;

public:
    AllAmtProofs(const DkgParams& params)
        : authAccs(*params.authAccs), n(params.n), numBits(params.numBits), proofNumLevels(params.maxLevel + 1)
    {
    }

    virtual ~AllAmtProofs() {}
    
public:
    void addVerificationHelpers(size_t id) {
        // Fetch the accumulators needed to verify proofs for p(w_N^id)
        accPathForId = authAccs.getPathFromLeaf(libff::bitreverse(id, numBits));
        accPathForId.resize(proofNumLevels);
    }
    
    void computeAllProofs(const RootsOfUnityEvaluation& eval, bool isSimulated) {
        authEval.reset(new AuthRootsOfUnityEvaluation(eval, authAccs.kpp, isSimulated));
    }

    void setZeroProof(const G1& proof) { f0proof = proof; }

    virtual const G1& getZeroProof() const { return f0proof; }

    /**
     * Returns the proof for p(w_N^id)
     * NOTE: The AMT proof for p(w_N^id) starts at the root and ends at leaf # bitreverse(id) (i.e., authEval.tree[0][bitreverse(id)])
     * NOTE: We return the proof by value here since it consists of a bunch of quotient commitments from the multipoint eval tree
     */
    virtual AmtProof getPlayerProof(size_t id) const {
        assertStrictlyLessThan(id, n);

        AmtProof proof;
        proof.quoComms = authEval->getPathFromLeaf(libff::bitreverse(id, numBits));
        proof.quoComms.resize(proofNumLevels);
        return proof;
    }

    /**
     * Verifies an AMT proof for p(id) = val relative to polyComm.
     */
    bool verifyAtId(const G1& polyComm, const AmtProof& proof, const Fr& val) const {
        G1 valComm = val * G1::one();
        logtrace << "Verifying at ID" << endl;
        return verifyCommLogSized(polyComm, proof, valComm, accPathForId);
    }

    /**
     * Verifies a normal Kate et al proof for valComm = g^p_j(0) where p_j is committed in polyComm.
     */
    bool verifyAtZero(const G1& polyComm, const G1& proof, const G1& valComm) const {
        return verifyCommConstSized(polyComm, proof, valComm, authAccs.kpp.getG2toS());
    }

    /**
     * Verifies an AMT proof for valComm = g^p_j(z), where p_j is committed in polyComm and z is a root of a(x), which is committed in accs.
     */
    bool verifyCommLogSized(const G1& polyComm, const AmtProof& proof, const G1& valComm, const std::vector<G2>& accs) const {
        GT lhs = ReducedPairing(polyComm - valComm, G2::one());
        GT rhs = GT::one(); // GT uses multiplicative notation

        testAssertEqual(accs.size(), proof.quoComms.size());

        // TODO: could multithread this verification, but libff crashes in ReducedPairing for some reason
        logtrace << "Path height: " << accs.size() << endl;
        for(size_t i = 0; i < accs.size(); i++) {
            logtrace << "q[" << i << "] = " << proof.quoComms[i] << endl;
            logtrace << "a[" << i << "] = " << accs[i] << endl;

            // we remove g^{q(s)}) commitments from the proof when q(s) = 0
            testAssertFalse(proof.quoComms[i] == G1::zero());

            rhs = rhs * ReducedPairing(proof.quoComms[i], accs[i]);
        }

        return lhs == rhs;
    }

    /**
     * Verifies a normal Kate et al proof for valComm = g^p_j(z), where p_j is committed in polyComm and z is committed in acc = g^{s - z}
     */
    bool verifyCommConstSized(const G1& polyComm, const G1& proof, const G1& valComm, const G2& acc) const {
        return ReducedPairing(polyComm - valComm, G2::one()) ==
               ReducedPairing(proof, acc);
    }
};

/**
 * An AMT DKG/VSS player.
 */
class MultipointPlayer : public AbstractKatePlayer<AllAmtProofs, AmtProof, MultipointPlayer> {
public:
    // for evaluating polynomial p(.) fast at all w_n^j, for j = 0, ..., n-1
    std::unique_ptr<RootsOfUnityEvaluation> eval;

public:
    MultipointPlayer(const DkgParams& params, const KatePublicParameters& kpp, size_t id, bool isSimulated, bool isDkgPlayer)
        : AbstractKatePlayer<AllAmtProofs, AmtProof, MultipointPlayer>(params, kpp, id, isSimulated, isDkgPlayer)
    {
        assertInclusiveRange(0, id, params.n - 1);

        // the accumulators in the multipoint evaluation tree have max degree N = 2^k, where N is the smallest value such that n <= N
        if(kpp.g2si.size() < params.t - 1) {
            throw std::runtime_error("Need more public parameters for the specified number of players (for accumulators)");
        }

        allProofs.reset(new AllAmtProofs(params));
        allProofs->addVerificationHelpers(id);
    }

public:
    /**
     * Performs a multipoint evaluation at w_N^i for all i\in {0, ..., n-1} (instead of a classic DFT) since 
     * we need to authenticate the resulting multipoint eval tree to obtain all AMT proofs.
     */
    virtual void evaluate() {
        eval.reset(new RootsOfUnityEvaluation(f_id, *params.accs));
        shares = eval->getEvaluations();
        assertEqual(shares.size(), params.n);
    }

    virtual void computeRealProofs() {
        assertNotNull(eval);
        assertNotNull(allProofs);

        auto proofs = dynamic_cast<AllAmtProofs*>(allProofs.get());
        proofs->computeAllProofs(*eval, isSimulated());

        eval.reset(nullptr); // don't need the multipoint eval after

        // compute constant-sized proof for p(0)
        proofs->setZeroProof(
            std::get<0>(
                kateProve(Fr::zero())
            )
        );
    }

    virtual void computeSimulatedProofs() {
        assertNotNull(eval);
        assertNotNull(allProofs);

        auto proofs = dynamic_cast<AllAmtProofs*>(allProofs.get());
        proofs->computeAllProofs(*eval, isSimulated());

        eval.reset(nullptr); // don't need the multipoint eval after

        // compute constant-sized proof for p(0)
        Fr s = kpp.getTrapdoor();
        Fr pOfS = libfqfft::evaluate_polynomial(f_id.size(), f_id, s);
        assertEqual(comm, pOfS * G1::one());
        allProofs->setZeroProof(
            ( (pOfS - f_id[0])*s.inverse() ) * G1::one());
    }

    // always verify t correct shares with memoization
    // if fast-track == false, then also verify the remaining n-t shares without memo
    virtual bool verifySharesReconstruction(const std::vector<size_t>& subset, bool fastTrack) {
        assertNotNull(params.authAccs);
        size_t numBits = params.numBits;
        size_t proofNumLevels = params.maxLevel + 1;

        // NOTE: We verify as e(g^p(s), g) = [ \prod_w e(g^q_w(s), g^a_w(s)) ] e(g,g)^p(i)
        // because computing e(g,g)^p(i) is faster than computing e(g^p(s)/g^p(i), g)

        // memoizes e(g^q_w(s), g^a_w(s)) by w = [level][idx]
        class PairingMemo {
        public:
            boost::unordered_map<std::pair<size_t, size_t>, GT> m;

        public:
            GT& at(size_t level, size_t idx) {
                return m[std::make_pair(level, idx)];
            }
        } memo;
        GT gtDefault;
        assertEqual(memo.at(0,0), gtDefault);

        // Step 1: Whether fast-track or not, use memoization to verify t proofs
        
        // so, compute e(g^p(s),g) LHS
        GT lhs = ReducedPairing(comm, G2::one());

        auto postMortem = [](const std::vector<G2>& accs, const std::vector<G1>& quo) {
            loginfo << "Path lengths: " << accs.size() << " and " << quo.size() << endl;

            for(size_t i = 0; i < accs.size(); i++) {
                loginfo << "accs[" << i << "]: " << accs[i] << endl;
            }

            for(size_t i = 0; i < quo.size(); i++) {
                loginfo << "quo[" << i << "] : " << quo[i] << endl;
            }
        };

        // ...and then compute [ \prod_w e(g^q_w(s), g^a_w(s)) ] e(g,g)^p(i) RHS
        std::vector<std::tuple<size_t, size_t, GT>> pairings;
        for(size_t pid : subset) {
            size_t i = libff::bitreverse(pid, numBits);

            GT rhs = GT::one();
            auto accs = params.authAccs->getPathFromLeaf(i);
            accs.resize(proofNumLevels);
            auto quo = allProofs->getPlayerProof(pid).quoComms;
            testAssertEqual(accs.size(), quo.size());

            pairings.clear();
            for(size_t k = 0; k < quo.size(); k++) {
                // don't pair e(g^{q(s)}, g^{a(s)}) when q(s) = 0
                testAssertFalse(quo[k] == G1::zero());

                // check if we've already memoized this pairing and, if not, queue it for memoizing
                // if the proof verifies
                GT& pair = memo.at(k, i);
                if(pair == gtDefault) {
                    pairings.push_back(
                        std::make_tuple(
                            k, i,
                            ReducedPairing(quo[k], accs[k])));

                    rhs = rhs * std::get<2>(pairings.back());
                } else {
                    rhs = rhs * pair;
                }

                // move up a level
                i /= 2;
            }

            rhs = rhs * (params.gt ^ shares[pid]);

            if(lhs == rhs) {
                // if the proof verified, then memoize its pairings for later
                for(auto& pair : pairings) {
                    memo.at(std::get<0>(pair), std::get<1>(pair)) = std::get<2>(pair);
                }
            } else {
                logerror << "AMT proof of player " << pid << " did not verify during reconstruction" << endl;
                postMortem(accs, quo);
                return false;
            }
        }
        
        if(!fastTrack) {
            // Step 2: If worst-case, verify remaining n-t proofs without memoization
            // each element of 'subset' is the end 'b' of a range of player IDs [start, end)
            // initially start = 0, and at the next iteration it's set to end+1
            size_t start = 0;

            // if params.n - 1 is NOT in 'subset', then we add it (efficiently via boost::join?), since it's the last range's endpoint
            std::vector<size_t> last;
            if(subset.back() < params.n - 1) {
                last.push_back(params.n);
            }

            lhs = ReducedPairing(comm, G2::one());
            // joins the two ranges into one (efficiently, I hope?)
            auto range = boost::join(subset, last);
#ifndef NDEBUG
            std::vector<size_t> pidsLeft;
#endif
            for(size_t end : range) {
                for(size_t pid = start; pid < end; pid++) {
#ifndef NDEBUG
                    pidsLeft.push_back(pid);
#endif
                    size_t leafIdx = libff::bitreverse(pid, numBits);

                    GT rhs = GT::one();
                    auto accs = params.authAccs->getPathFromLeaf(leafIdx);
                    accs.resize(proofNumLevels);
                    auto quo = allProofs->getPlayerProof(pid).quoComms;
                    testAssertEqual(accs.size(), quo.size());

                    for(size_t k = 0; k < quo.size(); k++) {
                        // don't pair e(g^{q(s)}, g^{a(s)}) when q(s) = 0
                        testAssertFalse(quo[k] == G1::zero());

                        rhs = rhs * ReducedPairing(quo[k], accs[k]);
                    }

                    rhs = rhs * (params.gt ^ shares[pid]);

                    if(lhs != rhs) {
                        logerror << "AMT proof of player " << pid << " did not verify during reconstruction" << endl; 
                        postMortem(accs, quo);
                        return false;
                    }
                }

                // move on to the next range of player IDs [start, end)
                start = end + 1;
            }

#ifndef NDEBUG
            std::vector<size_t> allPids(subset.begin(), subset.end());
            allPids.insert(allPids.end(), pidsLeft.begin(), pidsLeft.end());
            std::sort(allPids.begin(), allPids.end()); // should be 0, 1, ..., n-1

            std::vector<size_t> expected(params.n);
            std::iota(expected.begin(), expected.end(), 0); // 0, 1, ..., n-1

            if(allPids != expected) {
                testAssertFail("Not all PIDs were checked in AMT verifySharesReconstruction()");
            }
#endif
        }   // end of if(!fastTrack)

        return true;
    }
};

}
