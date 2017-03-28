#!/bin/bash

if [[ x"$SCRIPT_HOME" == x"" ]]; then
  export SCRIPT_HOME=/local/
  source /local/env.sh
  cd $SCRIPT_HOME
fi
apt-get install faketime

git clone https://github.com/jerryz920/linux.git /openstack/linux
ln -s /openstack/linux/ $GOPATH/src/github.com/boot2docker/boot2docker/linux
cd $GOPATH/src/github.com/boot2docker/boot2docker/linux
git checkout -b dev-tapcon-v4.4 origin/dev-tapcon-v4.4
cp boot2docker_kern_config .config
source ./env
echo "$KBUILD_BUILD_VERSION" > .version
faketime "$KERNEL_DATE" make oldconfig
faketime "$KERNEL_DATE" make -j 14
faketime "$KERNEL_DATE" make -j 14 bzImage
faketime "$KERNEL_DATE" make -j 14 modules

