#include <polycrypto/Configuration.h>

#include <polycrypto/Lagrange.h>
#include <polycrypto/PolyOps.h>
#include <polycrypto/RootsOfUnityEval.h>

#include <vector>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <libfqfft/polynomial_arithmetic/basic_operations.hpp>

#include <xutils/Log.h>
#include <xassert/XAssert.h>

using namespace libfqfft;
using namespace std;

void compute_numerators(std::vector<Fr>& N0, const std::vector<Fr>& allOmegas, const std::vector<size_t>& T, const Fr& num0) {
    size_t N = allOmegas.size();
    assertTrue(Utils::isPowerOfTwo(N));

    N0.resize(T.size());
    size_t j = 0;
    for (auto i : T) {
        /**
         * Recall that: 
         *  a) Inverses can be computed fast as: (w_N^k)^{-1} = w_N^{-k} = w_N^N w_N^{-k} = w_N^{N-k}
         *  b) Negations can be computed fast as: -w_N^k = w_N^{k + N/2}
         *
         * So, (0 - w_N^i)^{-1} = (w_N^{i + N/2})^{-1} = w_N^{N - (i + N/2)} = w_N^{N/2 - i}
         * If N/2 < i, then you wrap around to N + N/2 - i.
         */
        size_t idx;

        if(N/2 < i) {
            idx = N + N/2 - i;
        } else {
            idx = N/2 - i; 
        }

        N0[j++] = num0 * allOmegas[idx];
    }

    assertEqual(j, N0.size());
}

void lagrange_coefficients(std::vector<Fr>& lagr, const std::vector<Fr>& allOmegas, const std::vector<Fr>& someOmegas, const std::vector<size_t>& T) {
    size_t N = allOmegas.size();

    // Num(x) = product of all (x - x_i), i.e., root of subproduct tree
    std::vector<Fr> Num;
    //{
    // NOTE: Using libff-based multiplication for interpolating N(x) is 8x slower than libntl
    //ScopedTimer<std::chrono::milliseconds> rt(std::cout, "N(x) from roots took: ", " ms\n");
    Num = poly_from_roots(someOmegas);
    //}

    // N0[i] = N_i(0) = Num(0) / (0 - x_i)
    std::vector<Fr> N0;
    compute_numerators(N0, allOmegas, T, Num[0]);

    // Ndiff = Num'(x)
    std::vector<Fr> Ndiff;
    poly_differentiate(Num, Ndiff);

    // D[i] = D_i = Num'(x_i), so we evaluate Num' at allOmegas and then return the values at someOmegas
    std::vector<Fr> D;
    poly_fft(Ndiff, N, D);

    // L[i] = L_i(0) = N_i(0) / D_i
    lagr.resize(T.size());
    for (size_t i = 0; i < lagr.size(); i++) {
        lagr[i] = N0[i] * D[T[i]].inverse();
    }
}

void lagrange_coefficients(std::vector<Fr>& lagr, const std::vector<Fr>& allOmegas, const std::vector<size_t>& T) {
    std::vector<Fr> someOmegas;
    for(size_t i : T) {
        assertStrictlyLessThan(i, allOmegas.size());
        someOmegas.push_back(allOmegas[i]);
    }

    return lagrange_coefficients(lagr, allOmegas, someOmegas, T);
}

void lagrange_coefficients_naive(std::vector<Fr>& L, const std::vector<Fr>& allOmegas, const std::vector<Fr>& someOmegas, const std::vector<size_t>& T) {
    size_t N = allOmegas.size();
    assertTrue(Utils::isPowerOfTwo(N));
    assertEqual(someOmegas.size(), T.size());
    assertStrictlyLessThan(someOmegas.size(), N);   // assuming N-1 out of N is "max" threshold, which we don't have to assume

    // WARNING: We want to make sure N*N fits in a size_t, so we can optimize the num0 code below to only compute % once
    testAssertStrictlyLessThan(N, 1u << 31);
    testAssertEqual(sizeof(size_t), 8);

    // Num(0) = \prod_{i\in T} (0 - w_N^i) = (-1)^|T| w_N^{(\sum_{i\in T} i) % N}
    Fr num0 = 1;
    vector<Fr> N0, D;

    L.resize(someOmegas.size());

    if(T.size() % 2 == 1)
        num0 = -Fr::one();

    size_t sum = 0;
    // NOTE: this sum is < 1 + 2 + ... + N < N*N. That's why we assert sizeof(N*N) < sizeof(size_t)
    for(auto i : T) {
        //sum = (sum + i) % N; // a bit slower, so should avoid
        sum += i;
    }
    sum %= N;
    assertStrictlyLessThan(sum, allOmegas.size());
    num0 *= allOmegas[sum];

    // Naively, we'd be doing a lot more field operations:
    //for(auto i : T) {
    //    size_t idx;
    //    if(i + N/2 >= N) {
    //        // idx = (i + N/2) - N
    //        idx = i - N/2;
    //    } else {
    //        idx = i + N/2;
    //    }
    //    num0 *= allOmegas[idx];
    //}

    // N0[i] = N_i(0) = Num(0) / (0 - x_i)
    compute_numerators(N0, allOmegas, T, num0);

    // D[i] = product (x_i - x_j), j != i
    D.resize(someOmegas.size(), 1);
    for (size_t i = 0; i < someOmegas.size(); i++) {
        for (size_t j = 0; j < someOmegas.size(); j++) {
            if (j != i) {
                D[i] *= (someOmegas[i] - someOmegas[j]);
            }
        }
    }

    // L[i] = L_i(0) = N_i(0) / D_i
    for (size_t i = 0; i < L.size(); i++) {
        L[i] = N0[i] * D[i].inverse();
    }
}
