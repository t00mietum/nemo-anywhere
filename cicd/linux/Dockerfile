# nemo-build-jammy: the Linux RELEASE build box.
#
# Day-to-day Linux dev builds run in nemo-build (Debian 13, matches the host).
# Release tarballs must not: a binary linked against trixie's glibc 2.41 only
# runs on distros from 2025 onward. Ubuntu 22.04 LTS gives glibc 2.35, which
# covers Ubuntu 22.04+, Debian 12+, Mint 21+, Fedora 36+ - and a binary built
# against an older glibc/GTK keeps working on newer ones, not the reverse.
#
# The release prefix links against the distro's own GTK3 (thin prefix - the
# runtime is not bundled on Linux), so the GTK version here is the floor too:
# 3.24.33, and every 3.24.x since is compatible.
#
# Source tree is mounted at /src (like nemo-build); builds go to /build-release.

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
		meson ninja-build gcc pkg-config gettext intltool itstool \
		gobject-introspection libgirepository1.0-dev python3-gi \
		libgtk-3-dev libglib2.0-dev libglib2.0-bin libglib2.0-dev-bin \
		libpango1.0-dev libatk1.0-dev libgail-3-dev \
		libjson-glib-dev libgsf-1-dev libexempi-dev libexif-dev 		libarchive-dev \
		libx11-dev libxext-dev libxrender-dev \
		desktop-file-utils tar xz-utils \
		xvfb \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /src
