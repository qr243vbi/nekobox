FROM opensuse/tumbleweed
WORKDIR /app
RUN zypper --non-interactive install patterns-kde-devel_qt6 libthrift-devel thrift patterns-devel-C-C++-devel_C_C++ cmake ninja golang
RUN zypper --non-interactive install qt6-widgets-devel qt6-linguist-devel qt6-qml-devel
RUN chmod 777 /app
CMD 'bash' '-x' '/app/script/build_docker.sh'
