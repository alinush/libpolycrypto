set(polycrypto_app_sources
    BandwidthCalc.cpp
    ParamsGenTrapdoors.cpp
    ParamsGenPowers.cpp
    ParamsValidate.cpp
    PolylogDkg.cpp
)

foreach(appSrc ${polycrypto_app_sources})
    get_filename_component(appName ${appSrc} NAME_WE)
    set(appDir ../bin/app)

    add_executable(${appName} ${appSrc})
    target_link_libraries(${appName} PUBLIC polycrypto)

    set_target_properties(${appName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${appDir})
endforeach()

#install(TARGETS PlayApp DESTINATION bin)
