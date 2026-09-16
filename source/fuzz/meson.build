# Fuzz targets for the parsers that read text from outside the program.
#
# The same files build two ways. Ordinarily fuzz-main.c is compiled in and each
# target is registered as a test that replays its seed corpus, so the targets
# keep building and the seeds keep meaning something on every suite run - a
# fuzz target nobody runs is a file that rots. With -Dfuzzing=true, clang and
# libFuzzer take over: libFuzzer brings its own main, so the driver is left
# out, and cicd/linux/fuzz.bash runs the real search.

# Replaying seeds through a plain build only proves the targets still run: an
# over-read walks off the end of the buffer and nothing says so. Backing the
# drag parser's bookkeeping fix out was seen to pass clean without this and to
# be caught with it, so the corpus tests carry AddressSanitizer themselves
# rather than waiting for somebody to set -Db_sanitize for the whole tree. The
# instrumented target is enough: ASan's allocator sees the overflow wherever the
# read happens. Dropped where the toolchain has no ASan, which leaves the
# targets as compile-and-run checks there.
fuzzSan = []
if cc.has_link_argument('-fsanitize=address')
  fuzzSan = [ '-fsanitize=address', '-fno-omit-frame-pointer' ]
endif

fuzzCArgs    = fuzzSan
fuzzLinkArgs = fuzzSan
fuzzDriver   = [ 'fuzz-main.c' ]

if get_option('fuzzing')
  # libFuzzer's own flag replaces the above; fuzz.bash asks for ASan alongside it.
  fuzzCArgs    = [ '-fsanitize=fuzzer' ]
  fuzzLinkArgs = [ '-fsanitize=fuzzer' ]
  fuzzDriver   = []
endif

# The settings parser is vendored and header-only, so this one links nothing
# but libm, which its integer parsing reaches for.
fuzzShcl = executable('fuzz-shcl',
  [ 'fuzz-shcl.c' ] + fuzzDriver,
  include_directories: [ shclInclude, ],
  dependencies: [ math ],
  c_args:    fuzzCArgs,
  link_args: fuzzLinkArgs,
)

fuzzDnd = executable('fuzz-dnd',
  [ 'fuzz-dnd.c' ] + fuzzDriver,
  include_directories: [ rootInclude, ],
  dependencies: [ gtk, nemo_private ],
  c_args:    fuzzCArgs,
  link_args: fuzzLinkArgs,
)

fuzzCommandTemplate = executable('fuzz-command-template',
  [ 'fuzz-command-template.c' ] + fuzzDriver,
  include_directories: [ rootInclude, ],
  dependencies: [ gtk, nemo_private ],
  c_args:    fuzzCArgs,
  link_args: fuzzLinkArgs,
)

# Listed rather than globbed: a seed earns its place by standing for a case, so
# adding one is worth a line here saying it was meant.
fuzzShclCorpus = files(
  'corpus/shcl/empty',
  'corpus/shcl/basic.shcl',
  'corpus/shcl/junk.shcl',
  'corpus/shcl/repeats.shcl',
  'corpus/shcl/deep.shcl',
  'corpus/shcl/binary',
)

fuzzDndCorpus = files(
  'corpus/dnd/empty',
  'corpus/dnd/one-plain',
  'corpus/dnd/two-plain',
  'corpus/dnd/geometry',
  'corpus/dnd/truncated',
  'corpus/dnd/no-cr',
  'corpus/dnd/mixed',
)

fuzzCommandTemplateCorpus = files(
  'corpus/command-template/empty',
  'corpus/command-template/plain',
  'corpus/command-template/unbalanced-quote',
  'corpus/command-template/list-inside',
  'corpus/command-template/unknown-token',
  'corpus/command-template/drops-out',
  'corpus/command-template/backslashes',
)

# Under -Dfuzzing there is no main to hand a file list to; the targets are
# driven by fuzz.bash instead.
#
# No colons in these names: meson reads "a:b" as suite:test, so a name carrying
# one cannot be picked out on the command line.
#
# GLib and GTK hold allocations for the life of the process by design, so leak
# checking is off - these targets are looking for a bad access, not a leak.
if not get_option('fuzzing')
  fuzzTestEnv = [ 'ASAN_OPTIONS=detect_leaks=0' ]

  test('Settings parser fuzz corpus test',  fuzzShcl,            args: fuzzShclCorpus,            env: fuzzTestEnv)
  test('Drag payload fuzz corpus test',     fuzzDnd,             args: fuzzDndCorpus,             env: fuzzTestEnv)
  test('Command template fuzz corpus test', fuzzCommandTemplate, args: fuzzCommandTemplateCorpus, env: fuzzTestEnv)
endif
