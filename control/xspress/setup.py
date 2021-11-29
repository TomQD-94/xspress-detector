import os
import sys
from setuptools import find_packages, setup

# Place the directory containing _version_git on the path
for path, _, filenames in os.walk(os.path.dirname(os.path.abspath(__file__))):
    if "_version_git.py" in filenames:
        sys.path.append(path)
        break

from _version_git import __version__, get_cmdclass  # noqa


setup(

    name="xspress-odin",
    cmdclass=get_cmdclass(),
    version=__version__,
    python_requires=">=3.6",
    package_dir={"": "src"},
    packages=find_packages(where="src"),
    include_package_data=True,
    install_requires=[
        # "odin-data",
        # "odin-control",
        "pyzmq",
        "tornado",
    ],
    entry_points={
        'console_scripts': [
            'xspress_odin  = xspress_odin.server:main',
        ]
    },
    # metadata to display on PyPI
    author="Omar Elamin",
    author_email="omar.elamin@diamond.ac.uk",
    description="Odin Server for Xspress 4 detector",
    classifiers=[
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
    ],
)

