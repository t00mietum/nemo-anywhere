
test('Search Engine test',
  executable('test-nemo-search-engine',
    [ 'test-nemo-search-engine.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Directory Async test',
  executable('test-nemo-directory-async',
    [ 'test-nemo-directory-async.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Config store test',
  executable('test-nemo-config',
    [ 'test-nemo-config.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Favorites test',
  executable('test-nemo-favorites',
    [ 'test-nemo-favorites.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Thumbnail factory test',
  executable('test-nemo-thumbnail',
    [ 'test-nemo-thumbnail.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: [],
  timeout: 180        # the hung-helper case waits out the thumbnailer timeout
)

test('Metadata store test',
  executable('test-nemo-metadata-store',
    [ 'test-nemo-metadata-store.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

# Needs the slug define: the point of the test is the path the app really builds.
test('Config root test',
  executable('test-nemo-config-root',
    [ 'test-nemo-config-root.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
    c_args: nemo_definitions,
  ),
  args: []
)

test('User input parse test',
  executable('test-eel-user-input',
    [ 'test-eel-user-input.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

# symlink(2) and the guard-page mmap below are POSIX; neither test cross-builds.
if not is_windows
  test('Symlink recurse premise test',
    executable('test-nemo-symlink-recurse',
      [ 'test-nemo-symlink-recurse.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )
endif

test('Search invalid regex test',
  executable('test-nemo-search-regex',
    [ 'test-nemo-search-regex.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

if not is_windows
  test('DnD selection parser test',
    executable('test-nemo-dnd',
      [ 'test-nemo-dnd.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )
endif

test('Terminal fallback test',
  executable('test-eel-terminal',
    [ 'test-eel-terminal.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Extension column test',
  executable('test-nemo-column',
    [ 'test-nemo-column.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_extension ],
  ),
  args: []
)

# The recycle-bin monitor is polled, so the same-count case has to sit through
# a few turns of it - well past the 30s meson allows by default.
test('Trash win32 test',
  executable('test-nemo-trash-win32',
    [ 'test-nemo-trash-win32.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: [],
  timeout: 120
)

test('Shortcut win32 test',
  executable('test-nemo-shortcut-win32',
    [ 'test-nemo-shortcut-win32.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private, win_shell ],
  ),
  args: []
)

test('GIO fileops probe',
  executable('test-gio-fileops',
    [ 'test-gio-fileops.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('DBus manager no-bus test',
  executable('test-nemo-dbus-manager',
    [ 'test-nemo-dbus-manager.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Copy test',
  executable('test-nemo-copy',
    [ 'test-copy.c', 'test.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
    c_args: nemo_definitions,
  ),
  args: []
)

# The resource has to be compiled straight in for the same reason the exe does:
# nothing else references the self-registering object, so the linker drops it.
test('Preferences widgets test',
  executable('test-nemo-prefs-widgets',
    [ 'test-nemo-prefs-widgets.c' ] + gresources,
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

if is_windows
  test('Path separator test',
    executable('test-nemo-path-separator-win32',
      [ 'test-nemo-path-separator-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )
endif

if is_windows
  test('Dot-file switch test',
    executable('test-nemo-dot-files-win32',
      [ 'test-nemo-dot-files-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )
endif

# The win32 helper lives in src/, not a library, so it is compiled straight in.
if is_windows
  test('Win32 argument quoting test',
    executable('test-nemo-view-win32',
      [ 'test-nemo-view-win32.c', '../src/nemo-view-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
      c_args: nemo_definitions,
    ),
    args: []
  )
endif

if is_windows
  test('DPI win32 test',
    executable('test-nemo-dpi-win32',
      [ 'test-nemo-dpi-win32.c', '../src/nemo-dpi-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
      c_args: nemo_definitions,
    ),
    args: []
  )
endif

if is_windows
  test('Network win32 test',
    executable('test-nemo-network-win32',
      [ 'test-nemo-network-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )
endif

if is_windows
  test('Properties fallback win32 test',
    executable('test-nemo-properties-win32',
      [ 'test-nemo-properties-win32.c', '../src/nemo-properties-win32.c' ],
      include_directories: [ rootInclude, '../src' ],
      dependencies: [ gtk, nemo_private, win_shell ],
      c_args: nemo_definitions,
    ),
    args: []
  )
endif

if is_windows
  test('Drive root name test',
    executable('test-nemo-drive-root-name',
      [ 'test-nemo-drive-root-name.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
      c_args: nemo_definitions,
    ),
    args: []
  )
endif

if is_windows
  test('File refresh churn test',
    executable('test-nemo-file-win32-churn',
      [ 'test-nemo-file-win32-churn.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
      c_args: nemo_definitions,
    ),
    args: []
  )
endif

if is_windows
  test('Thumbnail win32 type test',
    executable('test-nemo-thumbnail-win32',
      [ 'test-nemo-thumbnail-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
      c_args: nemo_definitions,
    ),
    args: []
  )
endif

test('Appearance themes test',
  # The theme resource is linked into the app, not into libnemo-private, so a
  # test that checks it has to carry its own copy.
  executable('test-nemo-appearance',
    [ 'test-nemo-appearance.c' ] + themeGresources,
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Column layout test',
  executable('test-nemo-column-layout',
    [ 'test-nemo-column-layout.c', '../src/nemo-column-layout.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
    c_args: nemo_definitions,
  ),
  args: []
)

test('Filename extension test',
  executable('test-nemo-filename-extension',
    [ 'test-nemo-filename-extension.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Command template test',
  executable('test-nemo-command-template',
    [ 'test-nemo-command-template.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Archive options test',
  executable('test-nemo-archive',
    [ 'test-nemo-archive.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

# Reads the archives it wrote back with libarchive, so it needs that library.
test('Archive job test',
  executable('test-nemo-archive-job',
    [ 'test-archive-job.c', 'test.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ archive, gtk, nemo_private ],
    c_args: nemo_definitions,
  ),
  args: []
)

test('Extract naming test',
  executable('test-nemo-extract',
    [ 'test-nemo-extract.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

# Writes its own zip with libarchive, so it needs the library the app links.
test('Extract job test',
  executable('test-nemo-extract-job',
    [ 'test-extract-job.c', 'test.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ archive, gtk, nemo_private ],
    c_args: nemo_definitions,
  ),
  args: []
)

test('Path list text test',
  executable('test-nemo-path-list',
    [ 'test-nemo-path-list.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Directory load clean test',
  executable('test-nemo-directory-load-clean',
    [ 'test-nemo-directory-load-clean.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)
