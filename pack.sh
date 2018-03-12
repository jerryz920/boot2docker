
mkdir -p docker
cp /usr/bin/docker* docker/
cp /usr/bin/strace docker/
cp $GOPATH/src/github.com/docker/docker/bundles/latest/dynbinary-client/* docker/
cp $GOPATH/src/github.com/docker/docker/bundles/latest/dynbinary-daemon/* docker/
rm docker/docker-1.14.0-dev
rm docker/dockerd-1.14.0-dev
tar czf tmpdocker.tgz docker/docker* docker/strace docker/attguard*
tar czf tmpdockerlib.tgz docker/lib*

