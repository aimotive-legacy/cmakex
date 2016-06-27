add_pkg(aw-sx
    GIT_URL https://scm.adasworks.com/r/frameworks/adasworks-std-extras
    GIT_SHALLOW 0
    CMAKE_ARGS -DBUILD_TESTING=0)

add_pkg(yaml-cpp
    GIT_URL https://github.com/adasworks/yaml-cpp
    CMAKE_ARGS -DYAML_CPP_BUILD_TOOLS=0)

add_pkg(nowide
    GIT_URL https://github.com/adasworks/nowide-standalone
    CMAKE_ARGS -DCMAKE_BUILD_TESTING=0)

add_pkg(Poco
    GIT_URL https://github.com/pocoproject/poco GIT_TAG master
    CMAKE_ARGS
        -DPOCO_STATIC=1 -DENABLE_MSVC_MP=1
        -DENABLE_XML=0 -DENABLE_JSON=0 -DENABLE_MONGODB=0 -DENABLE_UTIL=0
        -DENABLE_NET=0 -DENABLE_NETSSL=0 -DENABLE_NETSSL_WIN=0 -DENABLE_CRYPTO=0
        -DENABLE_DATA=0 -DENABLE_DATA_SQLITE=0 -DENABLE_DATA_MYSQL=0
        -DENABLE_DATA_ODBC=0 -DENABLE_SEVENZIP=0 -DENABLE_ZIP=0
        -DENABLE_PAGECOMPILER=0 -DENABLE_PAGECOMPILER_FILE2PAGE=0)

add_pkg(cereal
    GIT_URL https://github.com/adasworks/cereal
    CMAKE_ARGS -DJUST_INSTALL_CEREAL=1)

