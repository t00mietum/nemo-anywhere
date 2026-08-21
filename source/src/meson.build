dbusBuiltSources = gnome.gdbus_codegen(
  'nemo-freedesktop-generated',
  join_paths(meson.project_source_root(), 'data', 'freedesktop-dbus-interfaces.xml'),
  interface_prefix: 'org.freedesktop.',
  namespace: 'NemoFreedesktop',
  object_manager: true
)

nemoCommon_sources = [
  dbusBuiltSources,
  'nemo-action-config-widget.c',
  'nemo-archive-dialog.c',
  'nemo-application.c',
  'nemo-bookmark-list.c',
  'nemo-bookmarks-window.c',
  'nemo-config-base-widget.c',
  'nemo-connect-server-dialog-nonmain.c',
  'nemo-connect-server-dialog.c',
  'nemo-connect-server-operation.c',
  'nemo-desktop-item-properties.c',
  'nemo-dpi-win32.c',
  'nemo-error-reporting.c',
  'nemo-extension-config-widget.c',
  'nemo-file-management-properties.c',
  'nemo-floating-bar.c',
  'nemo-freedesktop-dbus.c',
  'nemo-icon-view-container.c',
  'nemo-icon-view-grid-container.c',
  'nemo-icon-view.c',
  'nemo-image-properties-page.c',
  'nemo-interesting-folder-bar.c',
  'nemo-list-model.c',
  'nemo-list-view.c',
  'nemo-location-bar.c',
  'nemo-location-entry.c',
  'nemo-mime-actions.c',
  'nemo-navigation-action.c',
  'nemo-navigation-state.c',
  'nemo-notebook.c',
  'nemo-pathbar.c',
  'nemo-places-sidebar.c',
  'nemo-plugin-manager.c',
  'nemo-previewer.c',
  'nemo-progress-info-widget.c',
  'nemo-progress-ui-handler.c',
  'nemo-properties-window.c',
  'nemo-query-editor.c',
  'nemo-template-config-widget.c',
  'nemo-self-check-functions.c',
  'nemo-statusbar.c',
  'nemo-thumbnail-problem-bar.c',
  'nemo-toolbar.c',
  'nemo-trash-bar.c',
  'nemo-tree-sidebar-model.c',
  'nemo-tree-sidebar.c',
  'nemo-view-dnd.c',
  'nemo-view-factory.c',
  'nemo-view-win32.c',
  'nemo-view.c',
  'nemo-window-bookmarks.c',
  'nemo-window-manage-views.c',
  'nemo-window-menus.c',
  'nemo-window-pane.c',
  'nemo-window-slot-dnd.c',
  'nemo-window-slot.c',
  'nemo-window.c',
  'nemo-x-content-bar.c',
]

nemoWindow_sources = [
  'nemo-main-application.c',
  'nemo-main.c',
  # Win32 only inside; compiles to nothing elsewhere. Its own file because
  # windows.h defines DELETE, which collides with identifiers in nemo-view.c
  # and would spread that problem into whatever included it.
  'nemo-splash-win32.c',
]
nemoWindow_headers = [
  'nemo-main-application.h',
]

if enableEmptyView
  nemoCommon_sources += 'nemo-empty-view.c'
endif

nemo_deps = [ gail, glib, gtk, math,
  egg, nemo_extension, nemo_private, win_shell ]

if exempi_enabled
  nemo_deps += exempi
endif

if libexif_enabled
  nemo_deps += libexif
endif

# Windows: embed a VERSIONINFO resource (generated from nemo-anywhere.rc.in) so the
# exe carries real publisher/version metadata. Empty list elsewhere (no-op).
nemo_win_res = []
if is_windows
  winmod = import('windows')
  ver = meson.project_version().split('.')
  # FILEVERSION fields must be numeric, so strip any prerelease suffix from the
  # patch (1.0.0-beta1 -> 0). The string fields below keep the full version.
  patch = ver[2].split('-')[0]
  rc_conf = configuration_data()
  rc_conf.set('VER_MAJOR', ver[0])
  rc_conf.set('VER_MINOR', ver[1])
  rc_conf.set('VER_PATCH', patch)
  rc_conf.set('VERSION',   meson.project_version())
  nemo_rc = configure_file(
    input:         'nemo-anywhere.rc.in',
    output:        'nemo-anywhere.rc',
    configuration: rc_conf,
  )
  # The .rc is generated into the build dir, so the manifest it names has to be
  # findable from the source dir as well.
  nemo_win_res = winmod.compile_resources(nemo_rc,
    include_directories: include_directories('.'),
    depend_files:        files('nemo-anywhere.manifest'),
  )
endif

# gresources rides with the executable on Windows, where the extension library is
# static and the linker would otherwise drop the self-registering resource object.
nemo = executable('nemo-anywhere',
  nemoCommon_sources + nemoWindow_sources + nemo_win_res +
    (is_windows ? gresources + themeGresources : []),
  include_directories: [ rootInclude ],
  c_args: nemo_definitions,
  dependencies: nemo_deps,
  # GUI subsystem so Windows doesn't spawn a console window (no-op elsewhere)
  win_subsystem: 'windows',
  install: true
)

# Helper exes are Unix-only. On Windows the connect + open-with dialogs run
# in-process and nothing spawns the standalone launchers; the extensions lister
# has nothing to enumerate with no external plugin loading. Keeps Windows to a
# single exe (the ultra-portable goal).
if not is_windows
  nemo_connect_server = executable('nemo-anywhere-connect-server',
    [
      'nemo-bookmark-list.c',
      'nemo-connect-server-dialog.c',
      'nemo-connect-server-dialog-main.c',
      'nemo-connect-server-operation.c'
    ],
    include_directories: [ rootInclude, ],
    c_args: nemo_definitions,
    dependencies: nemo_deps,
    install: true
  )

  nemo_open_with = executable('nemo-anywhere-open-with',
    [ 'nemo-open-with-main.c' ],
    include_directories: [ rootInclude, ],
    c_args: nemo_definitions,
    dependencies: nemo_deps,
    install: true
  )

  # LibExecs
  nemo_extensions_list = executable('nemo-anywhere-extensions-list',
    [ 'nemo-extensions-list.c' ],
    include_directories: [ rootInclude, ],
    c_args: nemo_definitions,
    dependencies: [ nemo_deps ],
    install: true,
    install_dir: libExecPath,
  )
endif
