
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

test('Drag drop confirm test',
  executable('test-nemo-drag-confirm',
    [ 'test-nemo-drag-confirm.c' ],
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

test('Treeview hit test',
  executable('test-eel-treeview-hit',
    [ 'test-eel-treeview-hit.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Search content test',
  executable('test-nemo-search-content',
    [ 'test-nemo-search-content.c' ],
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
# a few turns of it - well past the 30s meson allows by default. On a machine
# with a full bin the whole run has been seen to take four and a half minutes.
test('Trash win32 test',
  executable('test-nemo-trash-win32',
    [ 'test-nemo-trash-win32.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: [],
  timeout: 600
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

link_copy_job = executable('test-nemo-link-copy-job',
  [ 'test-nemo-link-copy-job.c', 'test.c' ],
  include_directories: [ rootInclude, ],
  dependencies: [ gtk, nemo_private ],
  c_args: nemo_definitions,
)

test('Link copy keeps the link', link_copy_job, args: [ 'keep' ])
test('Link copy follows the link', link_copy_job, args: [ 'copy' ])

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

test('Link copy test',
  executable('test-nemo-link-copy',
    [ 'test-nemo-link-copy.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Shortcut name test',
  executable('test-nemo-shortcut-name',
    [ 'test-nemo-shortcut-name.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

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

test('Search group test',
  executable('test-nemo-search-group',
    [ 'test-nemo-search-group.c', '../src/nemo-list-model.c' ],
    include_directories: [ rootInclude, include_directories('../src') ],
    dependencies: [ gtk, nemo_private, egg ],
    c_args: nemo_definitions,
  ),
  args: []
)

test('New process argv test',
  executable('test-nemo-new-process',
    [ 'test-nemo-new-process.c', '../src/nemo-new-process.c' ],
    include_directories: [ rootInclude, include_directories('../src') ],
    dependencies: [ gtk, nemo_private ],
    c_args: nemo_definitions,
  ),
  args: []
)

if not is_windows
  test('Instances test',
    executable('test-nemo-instances',
      [ 'test-nemo-instances.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: [ nemo ],
    timeout: 120
  )
endif

test('Filename extension test',
  executable('test-nemo-filename-extension',
    [ 'test-nemo-filename-extension.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Path shorten test',
  executable('test-nemo-path-shorten',
    [ 'test-nemo-path-shorten.c' ],
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

test('Config schema drift test',
  executable('test-nemo-config-schema',
    [ 'test-nemo-config-schema.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: [ meson.current_source_dir() / '..' / 'data' / 'nemo-anywhere.schema.shcl' ]
)

if is_windows
  # The exe needs the app's own manifest or every long-path call fails on the
  # file not being there, which is not what is under test.
  testWinMod = import('windows')
  longPathManifest = testWinMod.compile_resources('test-manifest.rc',
    include_directories: include_directories('../src'),
    depend_files:        files('../src/nemo-anywhere.manifest'),
  )

  test('Long path enumeration test',
    executable('test-nemo-dir-enum-win32',
      [ 'test-nemo-dir-enum-win32.c' ] + longPathManifest,
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )
endif

# The bookmark list lives in src/, not a library, so it is compiled straight in.
if is_windows
  test('First run test',
    executable('test-nemo-first-run-win32',
      [ 'test-nemo-first-run-win32.c', '../src/nemo-bookmark-list.c' ],
      include_directories: [ rootInclude, include_directories('../src') ],
      dependencies: [ gtk, nemo_private ],
      c_args: nemo_definitions,
    ),
    args: []
  )
endif

# The document converters, run on files written for the purpose.
test('Search helpers test',
  executable('test-nemo-search-helpers',
    [ 'test-nemo-search-helpers.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private, libgsf ],
    c_args: nemo_definitions,
  ),
  args: [],
  env: [ 'NEMO_SEARCH_HELPER_DIR=' + search_helper_dir ],
  depends: search_helper_exes,
)

if is_windows
  test('Windows Search test',
    executable('test-nemo-search-win32',
      [ 'test-nemo-search-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )

  test('File associations test',
    executable('test-nemo-associations-win32',
      [ 'test-nemo-associations-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )

  test('Program launch test',
    executable('test-nemo-launch-win32',
      [ 'test-nemo-launch-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )

  test('Shell icon test',
    executable('test-nemo-shell-icon-win32',
      [ 'test-nemo-shell-icon-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )

  test('Link type test',
    executable('test-nemo-link-info-win32',
      [ 'test-nemo-link-info-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )

  test('Clipboard win32 test',
    executable('test-nemo-clipboard-win32',
      [ 'test-nemo-clipboard-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )

  test('Drag source win32 test',
    executable('test-nemo-dnd-win32',
      [ 'test-nemo-dnd-win32.c' ],
      include_directories: [ rootInclude, ],
      dependencies: [ gtk, nemo_private ],
    ),
    args: []
  )
endif
