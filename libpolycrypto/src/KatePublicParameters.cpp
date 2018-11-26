#include <polycrypto/Configuration.h>

#include <fstream>

#include <polycrypto/KatePublicParameters.h>

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

    // generate q-PKE powers and write to file
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

}
