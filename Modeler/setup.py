#!/usr/bin/env python
from distutils.core import setup, Extension
import os, sys

#=============================================================================
# Modeler requires:
# ELSAPROD variable defined in environment
# C++ compiler
# Fortran compiler: defined in config.py
# Numpy
# KCore, Converter, Generator, Transform
#=============================================================================

# Write setup.cfg file
import KCore.Dist as Dist
Dist.writeSetupCfg()

# Test if numpy exists =======================================================
(numpyVersion, numpyIncDir, numpyLibDir) = Dist.checkNumpy()

# Test if kcore exists =======================================================
(kcoreVersion, kcoreIncDir, kcoreLibDir) = Dist.checkKCore()

from KCore.config import *

prod = os.getenv("ELSAPROD")
if prod is None: prod = 'xx'

# Setting libraryDirs, include dirs and libraries =============================
libraryDirs = ["build/"+prod, kcoreLibDir]
includeDirs = [numpyIncDir, kcoreIncDir]
libraries = ["kcore"]
(ok, libs, paths) = Dist.checkFortranLibs([], additionalLibPaths)
libraryDirs += paths; libraries += libs
(ok, libs, paths) = Dist.checkCppLibs([], additionalLibPaths)
libraryDirs += paths; libraries += libs
    
# setup ======================================================================
setup(
    name="Modeler",
    version="2.7",
    description="Modeler module.",
    author="Onera",
    package_dir={"":"."},
    packages=['Modeler']
    )

# Check PYTHONPATH ===========================================================
Dist.checkPythonPath(); Dist.checkLdLibraryPath()