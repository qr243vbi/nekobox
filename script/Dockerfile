FROM opensuse/tumbleweed
WORKDIR /app
RUN zypper --non-interactive install patterns-kde-devel_qt6 libthrift-devel thrift patterns-devel-C-C++-devel_C_C++ cmake ninja golang
COPY * /app/
