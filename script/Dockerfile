FROM opensuse/tumbleweed
WORKDIR /app
RUN zypper --non-interactive install patterns-kde-devel_qt6 libthrift-devel thrift patterns-devel-C-C++-devel_C_C++ cmake ninja golang
RUN chmod 777 /app
CMD 'ls' '/app/'
