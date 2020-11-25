from setuptools import setup, find_packages, Extension
from setuptools.command.test import test as TestCommand
from distutils.command.build_ext import build_ext
from distutils.command.clean import clean
from distutils import log
from distutils.errors import LibError
import versioneer

import os
import glob
import sys
import subprocess
import shlex

# Extract the stub-only argument from the command line if present
stub_only = False
if '--stub-only' in sys.argv:
    stub_only = True
    sys.argv.remove('--stub-only')

# Build the command class for setup, merging in versioneer, build, clean and test commands
merged_cmdclass = versioneer.get_cmdclass()

# Define the required odin-control package version
required_odin_version='0.3.1'

# Define installation requirements based on odin version 
install_requires = [
    'odin=={:s}'.format(required_odin_version)
]

# Define installation requirements based on odin version 
dependency_links = [
    'https://github.com/odin-detector/odin-control/zipball/{0}#egg=odin-{0}'.format(
        required_odin_version
    )
]


setup(
    name='xspress-detector',
    version=versioneer.get_version(),
    cmdclass=merged_cmdclass,
    description='Simple Odin control server (for xspress)',
    author='Bryan Tester',
    author_email='bryan.tester@diamond.ac.uk',
    packages=find_packages(),
    #install_requires=['odin-control', 'odin-data', 'configparser', 'enum34'],
    entry_points={
        'console_scripts': [
            'xspress_odin  = xspress.server:main',
        ]
    },
)
