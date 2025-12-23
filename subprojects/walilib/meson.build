project(
  'walilib',
  'cpp',
  meson_version: '>= 1.1', # for meson.options
  version: '0.1',
  default_options: ['cpp_std=c++20'],
) # TODO warning_level

if build_machine.system() != 'linux'
  # could check for Arch: cat /etc/os-release | grep ^ID=
  error('OS is not Linux')
endif

blkid_dep = dependency('blkid', required: true)
libmount_dep = dependency('mount', required: true)

sources = ['src/disk_utils.cpp']
includes = include_directories('include')

lib_wali = static_library(
  meson.project_name(),
  sources,
  include_directories: includes,
  dependencies: [blkid_dep, libmount_dep],
)

# this is used by the parent project
libwali_dep = declare_dependency(include_directories: includes, link_with: lib_wali)
