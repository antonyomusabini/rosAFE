./bootstrap.sh && mkdir build && cd build && ../configure --prefix=$ROBOTPKG_BASE --with-templates=ros/server,ros/client/c,ros/client/ros && make -j8 && sudo make install
