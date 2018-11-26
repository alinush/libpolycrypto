set(polycrypto_bench_sources
    BenchApproxVerifyKateProofs.cpp 
    BenchAMT.cpp
    BenchConvertAndMultiexp.cpp
    BenchDKG.cpp
    BenchExp.cpp
    BenchFFT.cpp
    BenchVSS.cpp
    BenchMultiexp.cpp
    BenchNizkPok.cpp
    BenchNtlConversion.cpp
    BenchPairing.cpp
    BenchPolyDivideXnc.cpp
    BenchPolynomialOps.cpp
    BenchRootsOfUnityEval.cpp
    BenchSecretReconstruction.cpp
    BenchThresholdSig.cpp
)

foreach(appSrc ${polycrypto_bench_sources})
    get_filename_component(appName ${appSrc} NAME_WE)
    set(appDir ../bin/bench)

    add_executable(${appName} ${appSrc})
    target_link_libraries(${appName} PRIVATE polycrypto)

    set_target_properties(${appName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${appDir})
endforeach()
