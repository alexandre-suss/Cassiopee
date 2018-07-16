#!/usr/bin/env python
from distutils.core import setup, Extension
import os, sys
import KCore.Dist as Dist

#=============================================================================
# Apps requires:
# ELSAPROD variable defined in environment
# CASSIOPEE
#=============================================================================

prod = os.getenv("ELSAPROD")
if prod is None: prod = 'xx'
    
# setup ======================================================================
setup(
    name="Apps",
    version="2.7",
    description="Application module (layer1).",
    author="C. Benoit",
    package_dir={"":"."},
    packages=['Apps', 'Apps.Chimera']
    )

# Check PYTHONPATH ===========================================================
Dist.checkPythonPath(); Dist.checkLdLibraryPath()