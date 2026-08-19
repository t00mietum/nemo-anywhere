#!/usr/bin/python3

# Install the bundled theme set: the vendored widget and icon themes plus our
# own first-party icon sets. They land in share/themes and share/icons rather
# than a folder of our own, because that is where GTK resolves a theme by name,
# and the installed prefix's share/ is already on the data-dir search path.
#
# A post-install script rather than install_subdir(): both trees live outside
# the meson project root (the whole GTK project is wrapped under source/), and
# install_subdir cannot reach across that.
#
# Syntax: meson_install_themes.py <repo-root> <true|false>

import os
import shutil
import subprocess
import sys


def copy_themes(src_root, dst_root):
	if not os.path.isdir(src_root):
		return []

	installed = []
	for name in sorted(os.listdir(src_root)):
		src = os.path.join(src_root, name)
		if not os.path.isdir(src):
			continue
		dst = os.path.join(dst_root, name)
		if os.path.isdir(dst):
			shutil.rmtree(dst)
		os.makedirs(dst_root, exist_ok=True)
		shutil.copytree(src, dst)
		installed.append(dst)
	return installed


def main(argv):
	if len(argv) < 3 or argv[2] != 'true':
		return 0

	repo = argv[1]
	prefix = os.environ.get('MESON_INSTALL_DESTDIR_PREFIX') \
		or os.environ['MESON_INSTALL_PREFIX']

	themes = copy_themes(os.path.join(repo, 'vendor', 'themes'),
			     os.path.join(prefix, 'share', 'themes'))

	icons = copy_themes(os.path.join(repo, 'vendor', 'icons'),
			    os.path.join(prefix, 'share', 'icons'))
	icons += copy_themes(os.path.join(repo, 'assets', 'icons'),
			     os.path.join(prefix, 'share', 'icons'))

	print('Installed %d widget themes and %d icon themes' % (len(themes), len(icons)))

	# The cache is an optimisation; a theme works without one, so a missing
	# tool or a cross build that cannot run it is not a failure.
	if not os.environ.get('DESTDIR'):
		for path in icons:
			try:
				subprocess.call(['gtk-update-icon-cache', '-q', '-f', '-t', path])
			except OSError:
				break

	return 0


if __name__ == '__main__':
	sys.exit(main(sys.argv))
