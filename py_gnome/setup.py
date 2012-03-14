CPP_CODE_DIR = "../lib_gnome"
import numpy as np
import os
import sys
from sysconfig import get_config_var

#from setuptools import setup
from Cython.Distutils import build_ext 
from distutils.core import setup
from distutils.extension import Extension

files = ['MemUtils.cpp', 'Mover_c.cpp', 'Replacements.cpp']
files += ['Map_c.cpp', 'CATSMover_c.cpp', 'GEOMETRY.cpp']
files += ['ShioCurrent1.cpp', 'ShioCurrent2.cpp', 'ShioHeight.cpp',]
files += ['OSSMTimeValue_c.cpp', 'TimeValue_c.cpp']
files += ['DagTreeIO.cpp', 'RectUtils.cpp', 'ShioTimeValue_c.cpp']
files += ['Random_c.cpp', 'WindMover_c.cpp', 'CurrentMover_c.cpp']
files += ['CompFunctions.cpp', 'CMYLIST.cpp']
files += ['TriGridVel_c.cpp', 'DagTree.cpp', 'StringFunctions.cpp']

temp_list = ['cyGNOME/c_gnome.pyx']
for file in files:
    temp_list.append(os.path.join(CPP_CODE_DIR ,file))
files = temp_list

extra_includes="."
compile_args=None
macros = [('pyGNOME', 1),]

#if sys.platform == "darwin":
#    macros += [('MAC', 1), ('TARGET_CARBON', 1),]
#    if get_config_var('UNIVERSALSDK') != None:
#        extra_includes=get_config_var('UNIVERSALSDK')+'/Developer/Headers/FlatCarbon'
#    else:
#        print 'UNIVERSALSDK not set. aborting.'
#        exit(-1)
#elif sys.platform == "win32":
#	compile_args = ['/W0',]
#	macros += [('IBM', 1),]

setup(name='python gnome',
      version='beta', 
      requires=['numpy'],
      cmdclass={'build_ext': build_ext },
      packages=['gnome','gnome.utilities',],
      ext_modules=[Extension('gnome.c_gnome',
                             files, 
                             language="c++",
			     define_macros = macros,
                             extra_compile_args=compile_args,
			     #extra_link_args=['-Wl,-framework', '-Wl,Carbon',],
			     include_dirs=[CPP_CODE_DIR,
                                           np.get_include(),
                                           'cyGNOME',
                                           extra_includes,
                                           ],
                             )
                   ]


     )

