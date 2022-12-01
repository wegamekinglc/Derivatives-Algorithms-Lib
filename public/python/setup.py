# -*- coding: utf-8 -*-
"""
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 StatPro Italia srl

 This file is part of DAL, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 DAL is free software: you can redistribute it and/or modify it
 under the terms of the DAL license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
"""

import os, sys, math
from setuptools import setup, Extension, find_packages
from setuptools import Command
from distutils.command.build_ext import build_ext
from distutils.command.build import build
from distutils.ccompiler import get_default_compiler


class my_wrap(Command):
    description = "generate Python wrappers"
    user_options = []

    def initialize_options(self): pass

    def finalize_options(self): pass

    def run(self):
        print('Generating Python bindings for DAL ...')
        swig_version = os.popen("swig -version").read().split()[2]
        major_swig_version = swig_version[0]
        if major_swig_version < '4':
            print('Warning: You have SWIG {} installed, but at least SWIG 4.0.1'
                  ' is recommended. \nSome features may not work.'
                  .format(swig_version))
        os.system('swig -python -py3 -c++ ' +
                  '-outdir dal -o dal/dal_wrap.cpp ' +
                  '../swig/dal.i')
        print("finished wrap for dal/dal_wrap.cpp")


class my_build(build):
    user_options = build.user_options + [
        ('static', True,
         "link against static CRT libraries on Windows")
    ]
    boolean_options = build.boolean_options + ['static']

    def initialize_options(self):
        build.initialize_options(self)
        self.static = True

    def finalize_options(self):
        build.finalize_options(self)


class my_build_ext(build_ext):
    user_options = build_ext.user_options + [
        ('static', True,
         "link against static CRT libraries on Windows")
    ]
    boolean_options = build.boolean_options + ['static']

    def initialize_options(self):
        build_ext.initialize_options(self)
        self.static = True

    def finalize_options(self):
        build_ext.finalize_options(self)
        self.set_undefined_options('build', ('static', 'static'))

        self.include_dirs = self.include_dirs or []
        self.library_dirs = self.library_dirs or []
        self.define = self.define or []
        self.libraries = self.libraries or []

        extra_compile_args = []
        extra_link_args = []

        compiler = self.compiler or get_default_compiler()

        if compiler == 'msvc':
            try:
                DAL_INSTALL_DIR = os.environ['DAL_DIR']
                self.include_dirs += [os.path.join(DAL_INSTALL_DIR, 'include')]
                self.library_dirs += [os.path.join(DAL_INSTALL_DIR, 'lib')]

            except KeyError:
                print('warning: unable to detect DAL installation')

            if 'INCLUDE' in os.environ:
                dirs = [dir for dir in os.environ['INCLUDE'].split(';')]
                self.include_dirs += [d for d in dirs if d.strip()]
            if 'LIB' in os.environ:
                dirs = [dir for dir in os.environ['LIB'].split(';')]
                self.library_dirs += [d for d in dirs if d.strip()]
            dbit = round(math.log(sys.maxsize, 2) + 1)
            if dbit == 64:
                machinetype = '/machine:x64'
            else:
                machinetype = '/machine:x86'
            self.define += [('__WIN32__', None), ('WIN32', None),
                            ('NDEBUG', None), ('_WINDOWS', None),
                            ('NOMINMAX', None)]
            extra_compile_args = ['/GR', '/FD', '/Zm250', '/EHsc', '/bigobj', '/std:c++17']
            extra_link_args = ['/subsystem:windows', machinetype]

            if self.debug:
                if self.static:
                    extra_compile_args.append('/MTd')
                else:
                    extra_compile_args.append('/MDd')
            else:
                if self.static:
                    extra_compile_args.append('/MT')
                else:
                    extra_compile_args.append('/MD')

        elif compiler == 'unix':
            DAL_INSTALL_DIR = os.environ['DAL_DIR']
            ql_compile_args = [f"-I{DAL_INSTALL_DIR}/include"]
            ql_link_args = [f"-L{DAL_INSTALL_DIR}/lib", "-ldal", "-ldal_public"]

            self.define += [(arg[2:], None) for arg in ql_compile_args
                            if arg.startswith('-D')]
            self.define += [('NDEBUG', None)]
            self.include_dirs += [arg[2:] for arg in ql_compile_args
                                  if arg.startswith('-I')]
            self.library_dirs += [arg[2:] for arg in ql_link_args
                                  if arg.startswith('-L')]
            self.libraries += [arg[2:] for arg in ql_link_args
                               if arg.startswith('-l')]

            extra_compile_args = [arg for arg in ql_compile_args
                                  if not arg.startswith('-D')
                                  if not arg.startswith('-I')] \
                                 + ['-Wno-unused', '-std=c++17']
            if 'CXXFLAGS' in os.environ:
                extra_compile_args += os.environ['CXXFLAGS'].split()

            extra_link_args = [arg for arg in ql_link_args
                               if not arg.startswith('-L')
                               if not arg.startswith('-l')]
            if 'LDFLAGS' in os.environ:
                extra_link_args += os.environ['LDFLAGS'].split()

        else:
            pass

        for ext in self.extensions:
            ext.extra_compile_args = ext.extra_compile_args or []
            ext.extra_compile_args += extra_compile_args

            ext.extra_link_args = ext.extra_link_args or []
            ext.extra_link_args += extra_link_args


classifiers = [
    'Development Status :: 5 - Production/Stable',
    'Environment :: Console',
    'Intended Audience :: Developers',
    'Intended Audience :: Science/Research',
    'Intended Audience :: End Users/Desktop',
    'License :: OSI Approved :: BSD License',
    'Natural Language :: English',
    'Programming Language :: C++',
    'Programming Language :: Python',
    'Topic :: Scientific/Engineering',
    'Operating System :: Microsoft :: Windows',
    'Operating System :: POSIX',
    'Operating System :: Unix',
    'Operating System :: MacOS',
]

setup(name="dal-python",
      version="0.1.2",
      description="Python bindings for the DAL library",
      author="cheng li",
      author_email="wegamekinglc@hotmail.copm",
      license="BSD 3-Clause",
      classifiers=classifiers,
      include_package_data=True,
      packages=find_packages(),
      py_modules=['dal.__init__', 'dal.dal'],
      ext_modules=[Extension("dal._dal",
                             ["dal/dal_wrap.cpp"])
                   ],
      cmdclass={'wrap': my_wrap,
                'build': my_build,
                'build_ext': my_build_ext
                }
      )
