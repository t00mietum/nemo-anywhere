
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

test('User input parse test',
  executable('test-eel-user-input',
    [ 'test-eel-user-input.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Symlink recurse premise test',
  executable('test-nemo-symlink-recurse',
    [ 'test-nemo-symlink-recurse.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Search invalid regex test',
  executable('test-nemo-search-regex',
    [ 'test-nemo-search-regex.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('DnD selection parser test',
  executable('test-nemo-dnd',
    [ 'test-nemo-dnd.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

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

test('Trash win32 test',
  executable('test-nemo-trash-win32',
    [ 'test-nemo-trash-win32.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
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
