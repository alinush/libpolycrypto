#include <polycrypto/Configuration.h>

#include <polycrypto/KatePublicParameters.h>
#include <polycrypto/FFT.h>

#include <fstream>

#include <libfqfft/evaluation_domain/domains/basic_radix2_domain.hpp>
#include <libff/algebra/fields/field_utils.hpp>

using libpolycrypto::FFT;
using libpolycrypto::invFFT;

namespace Dkg { 

KatePublicParameters::KatePublicParameters(const std::string& trapFile, size_t maxQ, bool progress, bool verify) {
    ifstream tin(trapFile);
    if(tin.fail()) {
        throw std::runtime_error("Could not open trapdoor file for reading");
    }

    loginfo << "Reading back q-PKE parameters... (verify = " << verify << ")" << endl;

    tin >> s;
    tin >> q;       // we read the q before so as to preallocate the std::vectors
    libff::consume_OUTPUT_NEWLINE(tin);

    loginfo << "available q = " << q << ", s = " << s << endl;

    if(maxQ > 0 && maxQ > q) {
        logerror << "You asked to read " << maxQ << " public parameters, but there are only " << q << " in the '" << trapFile << "-*' files" << endl;
        throw std::runtime_error("maxQ needs to be <= q");
    }

    if(tin.fail() || tin.bad()) {
        throw std::runtime_error("Error reading full trapdoor file");
    }
    
    tin.close();

    // if maxQ is set to the default 0 value, read all q public params from the file
    q = maxQ == 0 ? q : maxQ;
    loginfo << "wanted q = " << maxQ << endl;
    resize(q);
        
    G1 g1 = G1::one();
    G2 g2 = G2::one();
    Fr si = Fr::one();  // will store s^i

    size_t i = 0;
    int iter = 0;
    G1 tmpG1si;
    G2 tmpG2si;
    int prevPct = -1;
    while(i <= q) {
        std::string inFile = trapFile + "-" + std::to_string(iter);
        loginfo << q - i << " params left to read, looking at " << inFile << " ..." << endl;
        ifstream fin(inFile);

        if(fin.fail()) {
            throw std::runtime_error("Could not open public parameters file for reading");
        }
        
        size_t line = 0;
        while(i <= q) {
            line++;

            // read g1^{s^i}
            fin >> tmpG1si;
            if(fin.eof()) {
                logdbg << "Reached end of '" << inFile << "' at line " << line << endl;
                break;
            }

            g1si[i] = tmpG1si;
            libff::consume_OUTPUT_NEWLINE(fin);

            // read g2^{s^i}
            fin >> tmpG2si;
            if(fin.eof()) {
                logdbg << "Reached end of '" << inFile << "' at line " << line << endl;
                break;
            }

            g2si[i] = tmpG2si;
            libff::consume_OUTPUT_NEWLINE(fin);

            // Check what we're reading against trapdoors

            //logtrace << g1si[i] << endl;
            //logtrace << si*g1 << endl << endl;
            //logtrace << g2si[i] << endl;
            //logtrace << si*g2 << endl << endl;

            // Fully verify the parameters, if verify is true
            if(verify) {
                testAssertEqual(g1si[i], si*g1);
                testAssertEqual(g2si[i], si*g2);
            }
        
            if(progress) {
                //int pct = static_cast<int>(static_cast<double>(c)/static_cast<double>(endExcl-startIncl) * 10000.0);
                int pct = static_cast<int>(static_cast<double>(i)/static_cast<double>(q+1) * 100.0);
                if(pct > prevPct) {
                    logdbg << pct << "% ... (i = " << i << " out of " << q+1 << ")" << endl;
                    prevPct = pct;
                    
                    // Occasionally check the parameters
                    testAssertEqual(g1si[i], si*g1);
                    testAssertEqual(g2si[i], si*g2);
                }
            }

            si = si * s;
            i++;

        }

        fin.close();
        iter++; // move to the next file
    }

    if(i < q) {
        throw std::runtime_error("Did not read all parameters.");
    }

    if(i != q+1) {
        throw std::runtime_error("Expected to read exactly q parameters.");
    }
}

Fr KatePublicParameters::generateTrapdoor(size_t q, const std::string& outFile)
{
    ofstream fout(outFile);

    if(fout.fail()) {
        throw std::runtime_error("Could not open trapdoors file for writing");
    }

    // pick trapdoor s
    Fr s = Fr::random_element();

    loginfo << "Writing q-SDH trapdoor..." << endl;
    loginfo << "q = " << q << ", s = " << s << endl;
    // WARNING: SECURITY: Insecure implementation currently outputs the
    // trapdoors!
    fout << s << endl;
    fout << q << endl;

    fout.close();

    return s;
}

void KatePublicParameters::generate(size_t startIncl, size_t endExcl, const Fr& s, const std::string& outFile, bool progress)
{
    if(startIncl >= endExcl) {
        logwarn << "Nothing to generate in range [" << startIncl << ", " << endExcl << ")" << endl;
        return;
    }

    ofstream fout(outFile);

    if(fout.fail()) {
        throw std::runtime_error("Could not open public parameters file for writing");
    }

    //logdbg << "i \\in [" << startIncl << ", " << endExcl << "), s = " << s << endl;

    // generate q-SDH powers and write to file
    Fr si = s ^ startIncl;
    int prevPct = -1;
    size_t c = 0;
    for (size_t i = startIncl; i < endExcl; i++) {
        G1 g1si = si * G1::one();
        G2 g2si = si * G2::one();

        fout << g1si << "\n";
        fout << g2si << "\n";
    
        si *= s;

        if(progress) {
            int pct = static_cast<int>(static_cast<double>(c)/static_cast<double>(endExcl-startIncl) * 100.0);
            if(pct > prevPct) {
                loginfo << pct << "% ... (i = " << i << " out of " << endExcl-1 << ")" << endl;
                prevPct = pct;
                
                fout << std::flush;
            }
        }

        c++;
    }
    
    fout.close();
}

std::vector<G1> KatePublicParameters::computeAllHis(const std::vector<Fr>& f) const {
    // make sure the degree of f is <= q
    testAssertLessThanOrEqual(f.size(), q + 1);

    if(f.size() == 0)
        throw std::invalid_argument("f must not be empty");

    size_t m = f.size() - 1; // the degree of f
    size_t M = Utils::smallestPowerOfTwoAbove(m);

    /**
     * Recall that for all i\in[1, m]:
     *   h_i = \prod_{j = 0}^{m - i} (g^{s^{m-(i+j)})^f_{m-j}
     *
     * For i > m:
     *   h_i = G1::zero()
     *
     * Also recall that the upper-triangular m\times m Toeplitz matrix has:
     *   f_m on the diagonal
     *   f_{m-1} on the diagonal above it
     *   ...
     *   f_1 in the upper right corner
     *
     * And the size-m vector we're multiplying it by is:
     *   [ g^{s^{m-1}, g^{s^{m-2}, \dots, g^s, g ]^T
     *
     * The circulant matrix which we'll embed the Toeplitz matrix in has vector representative c:
     *   [ f_m, \vec{0_{m-1}}, f_m, f_1, f_2, f_3, \dots, f_{m-1} ]^T
     */

    // Note: We handle non-powers of two by making the Toeplitz matrix size a power of two (and padding the vector we're multiplying it by with zeros).

    std::vector<Fr> c(2*M, Fr::zero());
    if(m == M) {
        // if no padding, then f_m lands in top-left corner of Toeplitz matrix
        c[0] = f[m];
        c[m] = f[m];
    }

    size_t i = M+1;
    size_t len = (m == M) ? m-1 : m;
    for(size_t j = 1; j <= len; j++)
        c.at(i++) = f[j];

    std::vector<G1> x(2*M, G1::zero());
    size_t shift = M - m;
    for(size_t i = 0; i < m; i++)
        // x[m-1] = g1si[0]
        // x[m-2] = g1si[1]
        // ..
        // x[1] = g1si[m-2]
        // x[0] = g1si[m-1]
        x[shift + (m-1) - i] = g1si[i];

    // FFT on x
    FFT<G1, Fr>(x);
    
    // FFT on c
    Fr omega = libff::get_root_of_unity<Fr>(2*M);
    // TODO: libfqfft supports other fft's of different sizes too
    libfqfft::_basic_serial_radix2_FFT(c, omega);

    // Hadamard (entry-wise) product of the two
    for(size_t i = 0; i < 2*M; i++) {
        x[i] = c[i] * x[i];
    }

    // Inverse FFT of the result
    invFFT<G1, Fr>(x);

    x.resize(m);

    return x;
}

}
