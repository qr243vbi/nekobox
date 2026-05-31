#!/bin/bash
shopt -s extglob dotglob nullglob

mkdir lmdbxx ||:
curl -L -o lmdbxx/lmdb++.h https://raw.githubusercontent.com/qr243vbi/lmdbxx/refs/heads/master/include/lmdbxx/lmdb++.h

rm -rf QHotkey ||:
git clone --depth 1 https://github.com/qr243vbi/QHotkey
pushd QHotkey
rm -rf -- !("CMakeLists.txt"|"QHotkey")
popd

rm -rf quirc ||:
git clone --depth 1 https://github.com/dlbeer/quirc.git
pushd quirc
rm -rf -- !("lib")
popd

mkdir qrcodegen ||:
curl -L -o qrcodegen/qrcodegen.hpp https://raw.githubusercontent.com/nayuki/QR-Code-generator/refs/heads/master/cpp/qrcodegen.hpp
curl -L -o qrcodegen/qrcodegen.cpp https://raw.githubusercontent.com/nayuki/QR-Code-generator/refs/heads/master/cpp/qrcodegen.cpp

mkdir URLParser ||:
curl -L -o URLParser/url_parser.h https://raw.githubusercontent.com/dongbum/URLParser/refs/heads/master/src/url_parser.h
curl -L -o URLParser/url_parser_function.h https://raw.githubusercontent.com/dongbum/URLParser/refs/heads/master/src/url_parser_function.h

mkdir LRUCache ||:
curl -L -o LRUCache/LRUCache11.hpp https://raw.githubusercontent.com/mohaps/lrucache11/refs/heads/master/LRUCache11.hpp

mkdir ini ||:
curl -L -o ini/ini.h https://raw.githubusercontent.com/mcmtroffaes/inipp/refs/heads/develop/inipp/inipp.h
