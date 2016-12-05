FROM debian:jessie

RUN apt-get update && apt-get -y install  unzip \
                        xz-utils \
                        curl \
                        bc \
                        git \
                        build-essential \
                        golang \
                        cpio \
                        gcc libc6 libc6-dev \
                        kmod \
                        squashfs-tools \
                        genisoimage \
                        xorriso \
                        syslinux \
                        isolinux \
                        automake \
                        pkg-config \
                        p7zip-full

# https://www.kernel.org/
ENV KERNEL_VERSION  4.4.31

# Fetch the kernel sources
RUN curl --retry 10 https://www.kernel.org/pub/linux/kernel/v${KERNEL_VERSION%%.*}.x/linux-$KERNEL_VERSION.tar.xz | tar -C / -xJ && \
    mv /linux-$KERNEL_VERSION /linux-kernel

# http://aufs.sourceforge.net/
ENV AUFS_REPO       https://github.com/sfjro/aufs4-standalone
ENV AUFS_BRANCH     aufs4.4
ENV AUFS_COMMIT     7d174ae40b4c9c876ee51aa50fa4ee1f3747de23
# we use AUFS_COMMIT to get stronger repeatability guarantees

# Download AUFS and apply patches and files, then remove it
RUN git clone -b "$AUFS_BRANCH" "$AUFS_REPO" /aufs-standalone && \
    cd /aufs-standalone && \
    git checkout -q "$AUFS_COMMIT" && \
    cd /linux-kernel && \
    cp -r /aufs-standalone/Documentation /linux-kernel && \
    cp -r /aufs-standalone/fs /linux-kernel && \
    cp -r /aufs-standalone/include/uapi/linux/aufs_type.h /linux-kernel/include/uapi/linux/ && \
    set -e && for patch in \
        /aufs-standalone/aufs*-kbuild.patch \
        /aufs-standalone/aufs*-base.patch \
        /aufs-standalone/aufs*-mmap.patch \
        /aufs-standalone/aufs*-standalone.patch \
        /aufs-standalone/aufs*-loopback.patch \
    ; do \
        patch -p1 < "$patch"; \
    done

COPY kernel_config /linux-kernel/.config

RUN jobs=$(nproc); \
    cd /linux-kernel && \
    make -j ${jobs} oldconfig && \
    make -j ${jobs} bzImage && \
    make -j ${jobs} modules

# The post kernel build process

ENV ROOTFS /rootfs

# Make the ROOTFS
RUN mkdir -p $ROOTFS

# Prepare the build directory (/tmp/iso)
RUN mkdir -p /tmp/iso/boot

# Install the kernel modules in $ROOTFS
RUN cd /linux-kernel && \
    make INSTALL_MOD_PATH=$ROOTFS modules_install firmware_install

# Remove useless kernel modules, based on unclejack/debian2docker
RUN cd $ROOTFS/lib/modules && \
    rm -rf ./*/kernel/sound/* && \
    rm -rf ./*/kernel/drivers/gpu/* && \
    rm -rf ./*/kernel/drivers/infiniband/* && \
    rm -rf ./*/kernel/drivers/isdn/* && \
    rm -rf ./*/kernel/drivers/media/* && \
    rm -rf ./*/kernel/drivers/staging/lustre/* && \
    rm -rf ./*/kernel/drivers/staging/comedi/* && \
    rm -rf ./*/kernel/fs/ocfs2/* && \
    rm -rf ./*/kernel/net/bluetooth/* && \
    rm -rf ./*/kernel/net/mac80211/* && \
    rm -rf ./*/kernel/net/wireless/*

# Install libcap
RUN curl -fL http://http.debian.net/debian/pool/main/libc/libcap2/libcap2_2.22.orig.tar.gz | tar -C / -xz && \
    cd /libcap-2.22 && \
    sed -i 's/LIBATTR := yes/LIBATTR := no/' Make.Rules && \
    make && \
    mkdir -p output && \
    make prefix=`pwd`/output install && \
    mkdir -p $ROOTFS/usr/local/lib && \
    cp -av `pwd`/output/lib64/* $ROOTFS/usr/local/lib

# Make sure the kernel headers are installed for aufs-util, and then build it
RUN cd /linux-kernel && \
    make INSTALL_HDR_PATH=/tmp/kheaders headers_install && \
    cd / && \
    git clone https://github.com/Distrotech/aufs-util.git && \
    cd /aufs-util && \
    git checkout 5e0c348bd8b1898beb1e043b026bcb0e0c7b0d54 && \
    CPPFLAGS="-I/tmp/kheaders/include" CLFAGS=$CPPFLAGS LDFLAGS=$CPPFLAGS make && \
    DESTDIR=$ROOTFS make install && \
    rm -rf /tmp/kheaders

# Prepare the ISO directory with the kernel
RUN cp -v /linux-kernel/arch/x86_64/boot/bzImage /tmp/iso/boot/vmlinuz64

ENV TCL_REPO_BASE   http://tinycorelinux.net/7.x/x86_64
# Note that the ncurses is here explicitly so that top continues to work
ENV TCZ_DEPS        iptables \
                    iproute2 \
                    openssh openssl \
                    tar \
                    gcc_libs \
                    ncurses \
                    acpid \
                    xz liblzma \
                    git expat2 libgpg-error libgcrypt libssh2 \
                    nfs-utils tcp_wrappers portmap rpcbind libtirpc \
                    rsync attr acl \
                    curl ntpclient \
                    procps glib2 libtirpc libffi fuse pcre \
                    udev-lib udev-extra \
                    liblvm2 \
                    parted

# Download the rootfs, don't unpack it though:
RUN curl -fL -o /tcl_rootfs.gz $TCL_REPO_BASE/release/distribution_files/rootfs64.gz

# Install the TCZ dependencies
RUN set -ex && \
    for dep in $TCZ_DEPS; do \
        echo "Download $TCL_REPO_BASE/tcz/$dep.tcz"; \
        curl -fL -o /tmp/$dep.tcz $TCL_REPO_BASE/tcz/$dep.tcz; \
        unsquashfs -f -d $ROOTFS /tmp/$dep.tcz; \
        rm -f /tmp/$dep.tcz; \
    done

# Install Tiny Core Linux rootfs
RUN cd $ROOTFS && zcat /tcl_rootfs.gz | cpio -f -i -H newc -d --no-absolute-filenames

# Extract ca-certificates
RUN set -x \
	&& chroot "$ROOTFS" sh -xc 'ldconfig && /usr/local/tce.installed/openssl' \
	&& ln -sT ../usr/local/etc/ssl "$ROOTFS/etc/ssl" \
	&& cp "$ROOTFS/etc/resolv.conf" resolv.conf.bak \
	&& cp /etc/resolv.conf "$ROOTFS/etc/resolv.conf" \
	&& chroot "$ROOTFS" curl -fsSL 'https://www.google.com' -o /dev/null \
	&& mv resolv.conf.bak "$ROOTFS/etc/resolv.conf"

# Apply horrible hacks
RUN cd $ROOTFS && ln -s lib lib64

# get generate_cert
RUN curl -fL -o $ROOTFS/usr/local/bin/generate_cert https://github.com/SvenDowideit/generate_cert/releases/download/0.2/generate_cert-0.2-linux-amd64 && \
    chmod +x $ROOTFS/usr/local/bin/generate_cert


# TODO figure out how to make this work reasonably (these tools try to read /proc/self/exe at startup, even for a simple "--version" check)
## verify that all the above actually worked (at least producing a valid binary, so we don't repeat issue #1157)
#RUN set -x && \
#    chroot "$ROOTFS" VBoxControl --version && \
#    chroot "$ROOTFS" VBoxService --version

# Install build dependencies for VMware Tools
RUN apt-get update && apt-get install -y \
        autoconf \
        libdumbnet-dev \
        libdumbnet1 \
        libfuse-dev \
        libfuse2 \
        libglib2.0-0 \
        libglib2.0-dev \
        libmspack-dev \
        libssl-dev \
        libtirpc-dev \
        libtirpc1 \
        libtool \
    && rm -rf /var/lib/apt/lists/*



# Make sure that all the modules we might have added are recognized (especially VBox guest additions)
RUN depmod -a -b $ROOTFS $KERNEL_VERSION-boot2docker

COPY VERSION $ROOTFS/etc/version
RUN cp -v $ROOTFS/etc/version /tmp/iso/version

# Get the Docker binaries with version that matches our boot2docker version.
RUN curl -fSL -o /tmp/dockerbin.tgz https://get.docker.com/builds/Linux/x86_64/docker-$(cat $ROOTFS/etc/version).tgz && \
    tar -zxvf /tmp/dockerbin.tgz -C "$ROOTFS/usr/local/bin" --strip-components=1 && \
    rm /tmp/dockerbin.tgz && \
    chroot "$ROOTFS" docker -v

# Copy our custom rootfs
COPY rootfs/rootfs $ROOTFS

# setup acpi config dir &
# tcl6's sshd is compiled without `/usr/local/sbin` in the path
# Boot2Docker and Docker Machine need `ip`, so I'm linking it in here
RUN cd $ROOTFS \
    && ln -s /usr/local/etc/acpi etc/ \
    && ln -s /usr/local/sbin/ip usr/sbin/

# These steps can only be run once, so can't be in make_iso.sh (which can be run in chained Dockerfiles)
# see https://github.com/boot2docker/boot2docker/blob/master/doc/BUILD.md

# Make sure init scripts are executable
RUN find $ROOTFS/etc/rc.d/ $ROOTFS/usr/local/etc/init.d/ -exec chmod +x '{}' ';'

# move dhcp.sh out of init.d as we're triggering it manually so its ready a bit faster
RUN mv $ROOTFS/etc/init.d/dhcp.sh $ROOTFS/etc/rc.d/

# Change MOTD
RUN mv $ROOTFS/usr/local/etc/motd $ROOTFS/etc/motd

# Make sure we have the correct bootsync
RUN mv $ROOTFS/boot*.sh $ROOTFS/opt/ && \
	chmod +x $ROOTFS/opt/*.sh

# Make sure we have the correct shutdown
RUN mv $ROOTFS/shutdown.sh $ROOTFS/opt/shutdown.sh && \
	chmod +x $ROOTFS/opt/shutdown.sh

# Add serial console
RUN echo "#!/bin/sh" > $ROOTFS/usr/local/bin/autologin && \
	echo "/bin/login -f docker" >> $ROOTFS/usr/local/bin/autologin && \
	chmod 755 $ROOTFS/usr/local/bin/autologin && \
	echo 'ttyS0:2345:respawn:/sbin/getty -l /usr/local/bin/autologin 9600 ttyS0 vt100' >> $ROOTFS/etc/inittab && \
	echo 'ttyS1:2345:respawn:/sbin/getty -l /usr/local/bin/autologin 9600 ttyS1 vt100' >> $ROOTFS/etc/inittab

# fix "su -"
RUN echo root > $ROOTFS/etc/sysconfig/superuser

# add some timezone files so we're explicit about being UTC
RUN echo 'UTC' > $ROOTFS/etc/timezone \
	&& cp -L /usr/share/zoneinfo/UTC $ROOTFS/etc/localtime

# make sure the "docker" group exists already
RUN chroot "$ROOTFS" addgroup -S docker

# set up subuid/subgid so that "--userns-remap=default" works out-of-the-box
# (see also rootfs/rootfs/etc/sub{uid,gid})
RUN set -x \
	&& chroot "$ROOTFS" addgroup -S dockremap \
	&& chroot "$ROOTFS" adduser -S -G dockremap dockremap

# Get the git versioning info
COPY .git /git/.git
RUN cd /git && \
    GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD) && \
    GITSHA1=$(git rev-parse --short HEAD) && \
    DATE=$(date) && \
    echo "${GIT_BRANCH} : ${GITSHA1} - ${DATE}" > $ROOTFS/etc/boot2docker

# Copy boot params
COPY rootfs/isolinux /tmp/iso/boot/isolinux

COPY rootfs/make_iso.sh /tmp/make_iso.sh

RUN /tmp/make_iso.sh

CMD ["sh", "-c", "[ -t 1 ] && exec bash || exec cat boot2docker.iso"]
