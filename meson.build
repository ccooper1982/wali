project(
  'wali',
  'cpp',
  version: '0.1',
  meson_version: '>= 1.1', # for meson.options
  default_options: ['cpp_std=c++26'],
) # TODO warning_level/werror

# plog logging library
subproject('sergiusthebest-plog')
plog_dep = dependency('plog') #header only, but points meson to the include dir

# dependency() does not find wthttp lib
wthttp_dep = meson.get_compiler('cpp').find_library('wthttp', required: true)
wt_dep = dependency('wt', required: true)

# libmount and libmount
blkid_dep = dependency('blkid', required: true)
libmount_dep = dependency('mount', required: true)

includes = include_directories(['include'])
sources = ['src/Wali.cpp', 'src/DiskUtils.cpp', 'src/Install.cpp']

executable(
  meson.project_name(),
  sources,
  include_directories: includes,
  dependencies: [wthttp_dep, wt_dep, plog_dep, blkid_dep, libmount_dep],
)
