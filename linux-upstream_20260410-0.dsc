Format: 3.0 (quilt)
Source: linux-upstream
Binary: linux-image-6.14.0+, linux-libc-dev, linux-headers-6.14.0+
Architecture: arm64
Version: 20260410-0
Maintainer: kester <kester@ubuntu>
Homepage: https://www.kernel.org/
Build-Depends: debhelper-compat (= 12)
Build-Depends-Arch: bc, bison, flex, gcc-aarch64-linux-gnu <!pkg.linux-upstream.nokernelheaders>, kmod, libelf-dev:native, libssl-dev:native, libssl-dev <!pkg.linux-upstream.nokernelheaders>, python3:native, rsync
Package-List:
 linux-headers-6.14.0+ deb kernel optional arch=arm64 profile=!pkg.linux-upstream.nokernelheaders
 linux-image-6.14.0+ deb kernel optional arch=arm64
 linux-libc-dev deb devel optional arch=arm64
Checksums-Sha1:
 d52b98e87107bbb84c6e4c3197be94fac6a67ef4 248182625 linux-upstream_20260410.orig.tar.gz
 8e5f84e3e3f54a795ed82d3df0fe81c73c520813 62047 linux-upstream_20260410-0.debian.tar.gz
Checksums-Sha256:
 8011bb3562ca7659e77c9481830146937ed0cf70eba46d2b8be93c271e574e04 248182625 linux-upstream_20260410.orig.tar.gz
 767cfc849d8d6920cb43171e328d7ecb2b1863937af4a08a0adc0ac5a664d6d9 62047 linux-upstream_20260410-0.debian.tar.gz
Files:
 fe6e0e63e6be3dc5194c4e2f5ea2cf74 248182625 linux-upstream_20260410.orig.tar.gz
 bd3777fbd4ec82aba8f38a6e74f3d385 62047 linux-upstream_20260410-0.debian.tar.gz
