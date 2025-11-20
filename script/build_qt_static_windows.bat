cd qt6
CALL .\configure.bat -openssl-linked -release -static -prefix ../qt6_build -static-runtime -submodules qtbase,qtimageformats,qtsvg,qttranslations,qtgrpc -skip tests -skip examples -gui -widgets -- -D OPENSSL_ROOT_DIR="%VCPKG_ROOT%/installed/x64-mingw-static"

echo on branch %1
echo config complete, building...
cmake --build . --parallel
echo build one, installing...
ninja install
echo installed Qt %1 in static mode
