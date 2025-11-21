cd qt6
CALL .\configure.bat -openssl-linked -release -static -prefix ../qt6_build -static-runtime -submodules qtdeclarative,qtbase,qtimageformats,qtsvg,qttranslations,qtgrpc -skip tests -skip examples -gui -widgets -- -D CMAKE_TOOLCHAIN_FILE=%CD%/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=%1 -DVCPKG_MANIFEST_MODE=OFF

echo config complete, building...
cmake --build . --parallel
echo build one, installing...
ninja install
echo installed Qt in static mode
