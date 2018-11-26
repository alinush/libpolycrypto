#pragma once

#include <polycrypto/PolyCrypto.h>

#include <vector>

using namespace libpolycrypto;

/*
 * Let Num(x) = \prod_{i\in T} (x - w_N^i).
 * Computes N0[i] = N_i(0) = Num(0) / (0 - w_N^i) for all i \in T
 *
 * @param[out] N0           this is where N_i(0)'s are outputted
 * @param      allOmegas    all N Nth roots of unity
 * @param      T            the set of signer IDs in {0, ... N-1}
 * @param      num0         Num(x) evaluated at x = 0
 */
void compute_numerators(std::vector<Fr>& N0, const std::vector<Fr>& allOmegas, const std::vector<size_t>& T, const Fr& num0);

void lagrange_coefficients(std::vector<Fr>& lagr, const std::vector<Fr>& allOmegas, const std::vector<size_t>& T);

/**
 * Computes Lagrange coefficients L_i^T(0) = \prod_{j\in T, j \ne i} (0 - x_j) / (x_i - x_j)
 * for x_i = w_N^i where i \in T in O(k \log^2{k}) time, where k = |T|
 *
 * @param[out] lagr     the output Lagrange coefficients
 * @param allOmegas     contains all N Nth roots of unity w_N^k
 *                      Let N = allOmegas.size()
 * @param someOmegas    the signer IDs as roots of unity
 * @param T             the actual signer IDs: i.e., i such that allOmegas[i] \in someOmegas
 */
void lagrange_coefficients(std::vector<Fr>& lagr, const std::vector<Fr>& allOmegas, const std::vector<Fr>& someOmegas, const std::vector<size_t>& T);

/**
 * Computes Lagrange coefficients L_i(0) for set of points in 'x' in O(k^2) time, where k = |pts|
 *
 * @param[out] L        the output Lagrange coefficients
 * @param allOmegas     all N Nth roots of unity
 * @param someOmegas    the signer IDs as roots of unity
 * @param T             the actual signer IDs: i.e., i such that allOmegas[i] \in someOmegas
 */
void lagrange_coefficients_naive(std::vector<Fr>& L, const std::vector<Fr>& allOmegas, const std::vector<Fr>& someOmegas, const std::vector<size_t>& T);
