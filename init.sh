#!/bin/bash

if [[ x"$SCRIPT_HOME" == x"" ]]; then
  export SCRIPT_HOME=/local/
  source /local/env.sh
  cd $SCRIPT_HOME
fi
apt-get install -y faketime

go get github.com/jerryz920/linux
ln -s $GOPATH/src/github.com/jerryz920/linux/ $GOPATH/src/github.com/jerryz920/boot2docker/linux
mkdir $GOPATH/src/github.com/jerryz920/boot2docker/kernel
cd $GOPATH/src/github.com/jerryz920/boot2docker/linux
git checkout -b dev-tapcon-v4.4 origin/dev-tapcon-v4.4
cp boot2docker-app-selinux .config
source ./env
echo "$KBUILD_BUILD_VERSION" > .version
faketime "$KERNEL_DATE" make oldconfig
faketime "$KERNEL_DATE" make -j 14
faketime "$KERNEL_DATE" make -j 14 bzImage
faketime "$KERNEL_DATE" make -j 14 modules

sleep 5
source adminrc
# sometimes it bugs out... Just give it some time
openstack user list -v
openstack user list -v
openstack user list -v
