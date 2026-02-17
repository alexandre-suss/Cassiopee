/*
    Copyright 2013-2025 Onera.

    This file is part of Cassiopee.

    Cassiopee is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cassiopee is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cassiopee.  If not, see <http://www.gnu.org/licenses/>.
*/

// Convert file XMF-H5 ProLB / pyTree CGNS
#include <vector>
#include "converter.h"
#include "kcore.h"
#include "IO/GenIO.h"

// ============================================================================
/* Convert file to pyTree */
// ============================================================================
PyObject* K_CONVERTER::convertFile2PyTreeProlb(PyObject* self, PyObject* args)
{
  char* fileName; char* myFormat; char* fileName2; char* myFormat2;
  if (!PYPARSETUPLE_(args, SSSS_, &fileName, &myFormat, &fileName2, &myFormat2)) return NULL;

  // On commence par la lecture du fichier .xmf
  //*************************************************//
  std::vector<const char *> blockNames;
  std::vector<const char *> varInfos;
  std::vector<int> itInfos;
  PyObject* tree = NULL;
  printf("Reading %s (%s)...", fileName, myFormat);
  E_Int ret = K_IO::GenIO::getInstance()->prolbxmlread(fileName, blockNames, varInfos, itInfos);//, tree);
  // E_Int ret = K_IO::GenIO::getInstance()->prolbread(fileName, tree);
  printf("done.\n");

  if (ret == 1)
  {
    PyErr_SetString(PyExc_IOError, "convertFile2PyTree: failed to read .xmf file.");
    return NULL;
  }

  // printf("Found variables = %d\n", varInfos.size()); fflush(stdout);
  // for (int i = 0; i < varInfos.size()/2.; i++)
  // {
  //   printf("Variable %d: %s (%s)\n", i, varInfos[2 * i], varInfos[2 * i + 1]);
  // }

  // for (int i = 0; i < itInfos.size(); i++)
  // {
  //   printf("Snapshot %d -> Itération %d\n", i + 1, itInfos[i]);
  // }

  // Ensuite on lit le fichier .h5
  //*************************************************//
  printf("Reading %s (%s)...", fileName2, myFormat2); fflush(stdout);
  ret = K_IO::GenIO::getInstance()->prolbh5read(fileName2, blockNames, varInfos, itInfos, tree);
  printf("done.\n");

  if (ret == 1)
  {
   PyErr_SetString(PyExc_IOError, "convertFile2PyTree: failed to read .h5 file.");
   return NULL;
  }

  // On libere la memoire
  for (size_t i = 0; i < blockNames.size(); i++)
  {
    free((void *)blockNames[i]);
  }
  for (size_t i = 0; i < varInfos.size(); i++)
  {
    free((void *)varInfos[i]);
  }
  blockNames.clear();
  varInfos.clear();
  itInfos.clear();

  return tree;
}