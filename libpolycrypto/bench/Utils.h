#pragma once

#include <polycrypto/Dkg.h>

#include <string>
#include <stdexcept>

#include <boost/algorithm/string/predicate.hpp> // for boost::starts_with(str, pattern)

Dkg::AbstractPlayer* createPlayer(const std::string& dkgType, const Dkg::DkgParams& params, size_t id, const KatePublicParameters* kpp, const Dkg::FeldmanPublicParameters* fpp, bool isSimulated, bool isDkgPlayer) {
    assertInclusiveRange(0, id, params.n - 1);

    if(dkgType == "feld") {
        return new Dkg::FeldmanPlayer(params, *fpp, id, isSimulated, isDkgPlayer);
    } else if(boost::starts_with(dkgType, "kate")) {
        return new Dkg::KatePlayer(params, *kpp, id, isSimulated, isDkgPlayer);
    } else if(boost::starts_with(dkgType, "amt")) {
        return new Dkg::MultipointPlayer(params, *kpp, id, isSimulated, isDkgPlayer);
    } else {
        logerror << "Unknown type of DKG/VSS: '" << dkgType << "'. Exiting..." << endl;
        throw std::runtime_error("Invalid DKG/VSS type");
    }
}

bool needsKatePublicParams(const std::vector<std::string>& types) {
    bool needsKpp = false;

    for(auto& t : types) {
        if(boost::starts_with(t, "amt") || boost::starts_with(t, "kate")) {
            needsKpp = true;
            break;
        }
    }

    return needsKpp;
}

bool needsFeldmanPublicParams(const std::vector<std::string>& types) {
    bool needsFpp = false;

    for(auto& t : types) {
        if(t == "feld") {
            needsFpp = true;
            break;
        }
    }

    return needsFpp;
}

std::string sumAvgTimeOrNan(const std::vector<AveragingTimer>& timers, bool humanize) {
    microseconds::rep sum = 0;

    for(const auto& t : timers) {
        if(t.numIterations() > 0) {
            sum += t.averageLapTime();
        } else {
            return "nan";
        }
    }

    if(humanize)
        return Utils::humanizeMicroseconds(sum, 2);
    else
        return std::to_string(sum);
}

std::string avgTimeOrNan(const AveragingTimer& t, bool humanize) {
    auto v = {t};
    return sumAvgTimeOrNan(v, humanize);
}

std::string stddevOrNan(const AveragingTimer& t) {
    return t.numIterations() > 0 ? std::to_string(t.stddev()) : "nan";
}
