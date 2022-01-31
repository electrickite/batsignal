project('batsignal', 'c',
  license : 'ISC',
  version : '1.3.2',
  default_options : ['c_std=c99']
)

cc = meson.get_compiler('c')
os = host_machine.system()

libm_args = [] # ['-D', '_POSIX_C_SOURCE=200112L']
libm = cc.find_library('m',
  required : false)
libm_ok = cc.has_function('round',
  args: libm_args,
  dependencies : [libm],
  prefix : '#include <math.h>')
assert(libm_ok)

config = configuration_data()
config.set_quoted('VERSION', meson.project_version())
configure_file(
  output : 'config.h',
  configuration : config)

executable('batsignal', files('batsignal.c'),
  c_args: [libm_args],
  dependencies : [libm, dependency('libnotify')],
  install : true)

configure_file(
  configuration : {'VERSION': meson.project_version()},
  input : files('batsignal.1.in'),
  output: 'batsignal.1',
  install : true,
  install_dir : get_option('mandir') / 'man1')
