set(polycrypto_test_sources
    TestAMT.cpp
    TestDKGandVSS.cpp
    TestKatePublicParams.cpp
    TestPolyOps.cpp
    TestLagrange.cpp
    TestLibff.cpp
    TestNizkPok.cpp
    TestParallelPairing.cpp
    TestPolyDivideXnc.cpp
    TestRootsOfUnity.cpp
    TestRootsOfUnityEval.cpp
)

foreach(appSrc ${polycrypto_test_sources})
    get_filename_component(appName ${appSrc} NAME_WE)
    set(appDir ../bin/test)

    add_executable(${appName} ${appSrc})
    target_link_libraries(${appName} PRIVATE polycrypto)

    add_test(NAME ${appName} COMMAND ${appName})
    set_target_properties(${appName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${appDir})
endforeach()
