libgsf = dependency('libgsf-1', required: true)

# One converter per family, all plain C on libgsf so the same set builds and
# ships on every platform. mso-to-txt covers every zip-of-xml format: OOXML,
# OpenDocument and EPUB.
search_helper_exes = []

foreach name : ['mso', 'xls', 'ppt', 'doc']
  search_helper_exes += executable('nemo-anywhere-' + name + '-to-txt',
    'nemo-' + name + '-to-txt.c',
    dependencies: [libgsf, gio, glib],
    install: true
  )
endforeach

search_helper_dir = meson.current_build_dir()

install_data(
    'epub2text.nemo_search_helper',
    'mso.nemo_search_helper',
    'mso-word.nemo_search_helper',
    'mso-ppt.nemo_search_helper',
    'mso-xls.nemo_search_helper',
    'odf.nemo_search_helper',
    install_dir: join_paths(nemoDataPath, 'search-helpers')
)

subdir('third-party')
