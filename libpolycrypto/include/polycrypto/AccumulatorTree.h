#pragma once

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/PolyOps.h>
#include <polycrypto/BinaryTree.h>
#include <polycrypto/KatePublicParameters.h>

#include <vector>
#include <utility>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <boost/unordered_map.hpp>

#include <libff/common/utils.hpp> // libff::bitreverse(idx, numBits)
#include <libfqfft/polynomial_arithmetic/basic_operations.hpp>
#include <libfqfft/polynomial_arithmetic/naive_evaluate.hpp>

#include <xutils/Log.h>
#include <xutils/Utils.h>
#include <xassert/XAssert.h>

using namespace std;
using namespace libfqfft;
using Dkg::KatePublicParameters;
using libpolycrypto::multiExp;
using libpolycrypto::Fr;

namespace libpolycrypto {

/**
 * Let \omega denote an Nth primitive root of unity, where N is 2^k for some k.
 * Let n <= N be an integer.
 * This class stores an (unauthenticated) tree over "accumulator monomials."
 * It is used for evaluating a polynomial f at \omega^i for i = 0, ..., n-1.
 * Leaves are (x-\omega^0), (x-\omega^1), ..., (x-\omega^N) but in 'bitreverse(i)' order.
 * Parents are products of children.
 * Root is (x-\omega^0)(x-\omega^1)...(x-\omega^N).
 *
 * Recall that BinaryTree has a 'std::vector<std::vector<XncPoly>> tree' member
 *
 * tree[k][i] is the ith node at level k in the tree (k = 0 is the last level)
 *
 * The accumulators in the tree (let a[k][i] denote tree[k][i]):
 *  a[0][i] = x - \omega^{bitreverse(i)}
 *  a[k][i] = a[k-1][2*i] * a[k-1][2*i+1]
 *
 * TODO: can be multithreaded
 */
class AccumulatorTree : public BinaryTree<XncPoly> {
public:

protected:
    size_t n;               // number of points to evaluate at (see description above)
    size_t numBits;         // log2ceil(N), where N is min 2^k such that n <= N
    std::vector<Fr> omegas; // all Nth roots of unity (minimum N = 2^k such that n <= N)

public:
    /**
     * Creates accumulators for evaluating the polynomial at all \omega^i for i = {0, ..., n-1}.
     */
    AccumulatorTree(size_t n)
        : n(n)
    {
        if(n < 2) {
            throw std::runtime_error("If you want to do a single point evaluation, just evaluate the polynomial directly.");
        }

        // round n to the smallest N = 2^i such that n <= N
        size_t N = Utils::smallestPowerOfTwoAbove(n);
        numBits = Utils::log2ceil(N);

        // creates a full tree, with a single root node
        // WARNING: AuthAccumulatorTree uses this to deduce numBits, so do not make the max level smaller.
        size_t rootLevel = numBits;
        allocateTree(N, rootLevel);
        computeAccumulators(N);
    }

protected:
    /**
     * @param   N   must always be a power of two
     */
    void computeAccumulators(size_t N) {
        // initialize array of roots of unity
        omegas = get_all_roots_of_unity(N);

        // Initialize (x - \omega^i) monomials.
        // The ith leaf will store (x-\omega^{bitreverse(i)})
        for (size_t i = 0; i < omegas.size(); i++) {
            auto &monom = tree[0][i];

            monom.n = 1;

            size_t rev = libff::bitreverse(i, numBits);
            //logdbg << "Rev of " << i << " is " << rev << endl;
            assertStrictlyLessThan(rev, omegas.size());
            monom.c = -omegas[rev];

            //logdbg << "acc[" << 0 << "][" << i << "] = ";
            //poly_print_wnk(monom, omegas);
            //std::cout << endl;

            //assertEqual(
            //    libfqfft::evaluate_polynomial(monom.size(), monom, omegas[rev]),
            //    Fr::zero());
        }

        // NOTE: already initialized accumulator monomials in last level 0 above
        size_t numLevels = getNumLevels();
        for (size_t k = 1; k < numLevels; k++) {
            for (size_t i = 0; i < tree[k].size(); i++) {
                assertValidIndex(2*i + 1, tree[k - 1]);

                // NOTE: This is slower than using libntl's multiplication, but we don't care because
                // accumulators are only computed at startup.
                tree[k][i] = tree[k - 1][2*i] * tree[k - 1][2*i + 1];

                //logdbg << "acc[" << k << "][" << i << "] = ";
                //poly_print_wnk(tree[k][i].toLibff(), omegas);
                //std::cout << endl;
            }
        }

#ifndef NDEBUG
        auto& p = getRootPoly();
        assertEqual(p.n, N);
        assertEqual(p.c, -Fr::one());
#endif
    }

public:
    std::string toString() const {
        std::string s;

        for(size_t k = 0; k < getNumLevels(); k++) {
            s += "Level " + std::to_string(k) + ": ";
            for(size_t i = 0; i < tree[k].size(); i++) {
                s += tree[k][i].toString(omegas, true) + ", ";
            }
            s += "\n";
        }
        
        return s;
    }

    size_t getNumLevels() const { return tree.size(); }

    size_t getNumBits() const { return numBits; }

    /**
     * Returns the number of points the polynomial is being evaluated at.
     * This is NOT big N. It's the small n.
     */
    size_t getNumPoints() const {
        return n;
    }

    /**
     * Returns all Nth roots of unity, where N = 2^k is the smallest number such that n <= N.
     */
    const std::vector<Fr>& getAllNthRootsOfUnity() const {
        return omegas;
    }

    /**
     * Get root accumulator polynomial
     */
    const XncPoly& getRootPoly() const {
        return getPoly(getNumLevels() - 1, 0);
    }

    /**
     * Returns the ith accumulator polynomial at level k.
     */
    const XncPoly& getPoly(size_t k, size_t i) const {
        return tree[k][i];
    }

    bool operator==(const AccumulatorTree& other) const {
        for(size_t i = 0; i < getNumPoints(); i++) {
            if(tree[0][i] != other.tree[0][i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const AccumulatorTree& other) const {
        return !operator==(other);
    }
};

/**
 * Authenticated version of the accumulator tree: all accumulator polynomials are 
 * committed to using Kate et al commitments.
 *
 * TODO: can be multithreaded
 */
class AuthAccumulatorTree : public BinaryTree<G2> {
public:
    const AccumulatorTree& accs;
    const KatePublicParameters& kpp;

public:
    AuthAccumulatorTree(const AccumulatorTree& accs, const KatePublicParameters& kpp, size_t t)
        : accs(accs), kpp(kpp)
    {
        size_t maxLevel = Utils::log2floor(t-1);
        size_t N = accs.getNumLeaves();
        allocateTree(N, maxLevel);

        logdbg << "N = " << N << endl;
        logdbg << "numBits = " << accs.getNumBits() << endl;

        authenticate();
    }

public:
    /**
     * Returns the number of points the polynomial is being evaluated at.
     * This is NOT big N. It's the small n.
     */
    //size_t getNumPoints() const {
    //    return accs.getNumPoints();
    //}
    
    const G2& getLeaf(size_t leafIdx) const {
        assertStrictlyLessThan(leafIdx, tree[0].size());
        return tree[0][leafIdx];
    }

protected:
    void authenticate() {
        // caches g2^{w_N^j} given j
        size_t N = accs.getNumLeaves();
        std::vector<G2> exp(N, G2::zero());

        traversePreorder([this, N, &exp](size_t k, size_t idx) {
            size_t numBits = accs.getNumBits(); // log2ceil(N)
            auto& acc = accs.getPoly(k, idx);   // the accumulator poly

            //loginfo << "Committing to acc of degree " << acc.size() - 1 << " at level " << k << endl;

            if(acc.n > kpp.g2si.size() - 1) {
                logerror << "At level " << k << ", need q-SDH params with q = " << acc.n << " but only have q = " << kpp.g2si.size() - 1 << endl;
                throw std::runtime_error("Not enough public params to commit to accumulators");
            }

            // we memoize g2^{w_N^j} to speed this up!
            // if we are at idx, then it stores (x - w_N^{bitrev(idx)}) = (x + w_N^{(bitrev(idx) + N/2) % N})
            size_t rev = libff::bitreverse(idx, numBits);
            size_t j = (rev + N/2) % N;

            // print the AccumulatorTree in bench/BenchAMT.cpp to see the beautiful structure of
            // these accumulator polynomials that we leverage here
            G2& e = exp[j];
            if(e == G2::zero()) {
                e = acc.c * G2::one();
                exp[rev] = -e;
            } else {
                assertEqual(e, acc.c * G2::one());
                //logperf << "Re-using exp!" << endl;
            }

            // commit to a(x)  = x^n - w_N^j
            //     as g^{a(s)} = g^{s^n} * g^{-w_N^j}
            tree[k][idx] = kpp.g2si[acc.n] + e;
        });
    }

public:
    void _assertValid() {
        traversePreorder([this](size_t k, size_t idx) {
            auto& a = tree[k][idx];   // the accumulator poly commitment
            auto& monom = accs.tree[k][idx];
            size_t numBits = accs.getNumBits();

            const auto& omegas = accs.getAllNthRootsOfUnity();
            testAssertEqual(
                a,
                kpp.g2si[monom.n] - omegas[libff::bitreverse(idx, numBits)] * G2::one());
        });
    }
};

} // end of namespace libpolycrypto
