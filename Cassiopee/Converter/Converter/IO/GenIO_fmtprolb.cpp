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

// ProLB (.xmf / .h5) file support

# include <stdio.h>
# include <stdlib.h>
# include <vector>
# include <string.h>

# include "GenIO.h"
# include "Array/Array.h"
# include "Def/DefFunction.h"
# include "Connect/connect.h"
# include "CompGeom/compGeom.h"
#include "hdf5.h"
#include "GenIO_hdfcgns.h"

using namespace K_FLD;
using namespace std;

/* ------------------------------------------------------------------------- */
// Extrait la valeur d'un attribut key="..."
/* ------------------------------------------------------------------------- */
bool getAttributeValue(const char* buf, const char* key, char* out)//, int outSize)
{
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "%s=\"", key);

  const char* start = strstr(buf, pattern);
  if (!start) return false;

  start += strlen(pattern);
  const char *end = strchr(start, '"');
  if (!end) return false;

  int len = end - start;
  // if (len >= outSize) len = outSize - 1;

  strncpy(out, start, len);
  out[len] = '\0';
  return true;
}

int getValueFromName(const char* buf, bool blockFlag, const char* name, char* out_val)
{
  if (blockFlag && strstr(buf, name))
  {
    if (getAttributeValue(buf, "Value", out_val))
      return 0; // Trouve et stocke dans out
    else
      return -1; // pas trouve
  }
  return -1; // hors bloc ou mauvais nom
}

int getValueFromName(const char* buf, bool blockFlag, const char* name, E_Int* out_val)
{
  if (blockFlag && strstr(buf, name))
  { 
    char out_str[256];
    if (getAttributeValue(buf, "Value", out_str))
    {
      *out_val = atoi(out_str);
      return 0; // Trouve et stocke dans out
    }
    else
      return -1; // pas trouve
  }
  return -1; // hors bloc ou mauvais nom
}

// ------------------------------------------------------------------
// Retourne le nom de la variable à partir d'un nom de frame.
// Exemple : "Wake_t000000952_Pressure" -> "Pressure"
// Attention : le pointeur renvoyé pointe dans la chaîne d'entrée !
// ------------------------------------------------------------------
// const char *getVariableFromFrame(const char *frameName)
// {
//   if (frameName == nullptr)
//     return nullptr;

//   const char *lastUnderscore = strrchr(frameName, '_');
//   if (lastUnderscore != nullptr && *(lastUnderscore + 1) != '\0')
//   {
//     return lastUnderscore + 1; // juste après le dernier "_"
//   }

//   return nullptr; // pas trouvé
// }

const char* getVariableFromFrame(const char *frameName)
{
    if (frameName == nullptr) return nullptr;

    // On cherche "_t" qui introduit l'horodatage
    const char *tPos = strstr(frameName, "_t");
    if (tPos == nullptr) return nullptr;

    // Avance après "_t"
    const char *p = tPos + 2;

    // Skip digits (le numéro du timestep)
    while (*p && isdigit(*p)) {
        ++p;
    }

    // On s’attend à trouver un "_" juste après les digits
    if (*p == '_') {
        return p + 1; // retourne tout ce qu’il y a après
    }

    return nullptr; // format inattendu
}

int getTimeFromFrame(const char *frameName)
{
    if (!frameName) return -1;

    const char *tPos = strstr(frameName, "_t");
    if (!tPos) return -1;

    const char *p = tPos + 2; // avance après "_t"

    // Déterminer la fin de la partie chiffres
    const char *end = p;
    while (*end && isdigit(*end)) {
        ++end;
    }

    if (p == end) return -1; // pas de chiffres trouvés

    // On copie dans un buffer temporaire pour atoi
    char buf[32];
    size_t len = end - p;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    strncpy(buf, p, len);
    buf[len] = '\0';

    return atoi(buf); // convertit en entier
}

/* ------------------------------------------------------------------------- */
// Cree un nouveau chemin pour le fichier .h5
/* ------------------------------------------------------------------------- */
char* makeH5Path(const char *inputPath, const char *recordingName)
{
  if (!recordingName)
    return nullptr;

  const char *lastSlash = strrchr(inputPath, '/');
  size_t dirLen = lastSlash ? (lastSlash - inputPath + 1) : 0;

  // longueur totale : dossier + recordingName + ".h5" + '\0'
  size_t totalLen = dirLen + strlen(recordingName) + 3 + 1; // 3 = "h5", 1 = '\0'
  char *newPath = (char *)malloc(totalLen);
  if (!newPath)
    return nullptr;

  // copier le dossier si présent
  if (dirLen > 0)
  {
    strncpy(newPath, inputPath, dirLen);
  }
  newPath[dirLen] = '\0';

  // ajouter recordingName et extension
  strcat(newPath, recordingName);
  strcat(newPath, ".h5");

  return newPath;
}

//=============================================================================
/* 
   prolbread
*/
//=============================================================================
E_Int K_IO::GenIO::prolbread(char *file, PyObject *&tree)
{
  
  /* File Opening */
  FILE* ptrFile;
  ptrFile = fopen(file, "r");

  if (ptrFile == NULL)
  {
    printf("Warning: prolbread: cannot open file %s.\n", file);
    return 1;
  }

  // varString = new char [8];
  // strcpy(varString, "x,y,z");

  // Lecture de l'entete
  E_Int res;
  // char* prevData = new char[BUFSIZE];
  // char* keyword = new char[BUFSIZE];
  // E_Float zlayer = 0.;
  // E_Float stack[3];
  // vector<E_Float> stackX;
  // vector<E_Float> stackY;
  // E_Int command = 0; E_Int commandP = 0;
  // E_Float rx0, ry0;

  res = readGivenKeyword(ptrFile, "<?");
  if (res == 0)
  {
    printf("Warning: prolbread: cannot find xml header.\n");
    fclose(ptrFile);
    return 1;
  }
  res = readGivenKeyword(ptrFile, ">");
  if (res == 0)
  {
    printf("Warning: prolbread: file seems to be corrupted.\n");
    fclose(ptrFile);
    return 1;
  }
  
  // Lecture du fichier .xmf
  char* buf = new char[BUFSIZE];

  vector<const char*> blockNames;
  char* recordingName = new char[256];
  int startIteration = 0;
  int endIteration = 0;
  int periodicity = 1;
  int numberOfBlocks = 0;

  bool inMetaData = false;
  bool inSimInfo = false, inSimParams = false;
  bool inRecording = false, inBlock = false;

  while ((res = readline(ptrFile, buf, BUFSIZE)) == 0)
  {

    // On supprime les espaces en debut de ligne
    int i = 0; while (buf[i] == ' ' || buf[i] == '\t') i++;
    if (i > 0) memmove(buf, buf + i, strlen(buf + i) + 1);
    //printf("Ligne lue : %s", buf);
    
    // Peut etre pratique de faire un test sur la version de ProLB 
    
    //-----------------------------------
    // Balises de bloc pour le fichier xml RecordingParameters
    //-----------------------------------
    // if (strstr(buf, "<Information") && strstr(buf, "MetaData"))
    // {
    //   inMetaData = true; // Debut du bloc MetaData
    // }
    // else if (strstr(buf, "<Information") && strstr(buf, "SimulationInformations"))
    // {
    //   inSimInfo = true; 
    // }
    // else if (strstr(buf, "<Information") && strstr(buf, "SimulationParameters"))
    // {
    //   inSimInfo = true; 
    // }

    if (strstr(buf, "<Information") && strstr(buf, "RecordingParameters"))
    {
      inRecording = true; // Debut du bloc RecordingParameters
    }
    else if (inRecording && strstr(buf, "</Information>"))
    {
      if (inBlock) // Si dans un block, on en sort d'abord
      {
        // nbBlocksRead++;
        inBlock = false;
      }
      else // Si plus de blocks, on sort de RecordingParameters
      {
        //inRecording = false;
        break; // fin du bloc RecordingParameters
      }
    }

    // -----------------------------------
    // Champs de RecordingParameters
    //-----------------------------------
    // else if (inRecording && strstr(buf, "RecordingName"))
    if (inRecording && strstr(buf, "RecordingName"))
    {
      getAttributeValue(buf, "Value", recordingName); //, sizeof(recordingName));
    }
    else if (inRecording && strstr(buf, "StartIteration"))
    {
      char val[64];
      getAttributeValue(buf, "Value", val);//, sizeof(val));
      startIteration = atoi(val);
    }
    else if (inRecording && strstr(buf, "EndIteration"))
    {
      char val[64];
      getAttributeValue(buf, "Value", val);//, sizeof(val));
      endIteration = atoi(val);
    }
    else if (inRecording && strstr(buf, "Periodicity"))
    {
      char val[64];
      getAttributeValue(buf, "Value", val);//, sizeof(val));
      periodicity = atoi(val);
    }
    else if (inRecording && strstr(buf, "NumberOfBlocks"))
    {
      char val[64];
      getAttributeValue(buf, "Value", val);//, sizeof(val));
      numberOfBlocks = atoi(val);
      inBlock = true;
    }

    //-----------------------------------
    // Champs de Block
    //-----------------------------------
    // else if (inBlock && strstr(buf, "BlockId"))
    // if (inBlock && strstr(buf, "BlockId"))
    // {
    //   char val[64];
    //   getAttributeValue(buf, "Value", val);//, sizeof(val));
    //   // blockIds[nbBlocksRead] = atoi(val);
    // }
    // else if (inBlock && strstr(buf, "BlockName"))
    if (inBlock && strstr(buf, "BlockName"))
    {
      char val[64];
      getAttributeValue(buf, "Value", val);//, sizeof(blockNames[nbBlocksRead]));
      blockNames.push_back(val);
    }
    // else if (inBlock && strstr(buf, "NumberOfNodes"))
    // {
    //   char val[64];
    //   getAttributeValue(buf, "Value", val, sizeof(val));
    //   blockNodes[nbBlocksRead] = atol(val);
    // }
    // else if (inBlock && strstr(buf, "NumberOfElements"))
    // {
    //   char val[64];
    //   getAttributeValue(buf, "Value", val, sizeof(val));
    //   blockElems[nbBlocksRead] = atol(val);
    // }
  } // Fin lecture fichier .xmf
  fclose(ptrFile);

  //-----------------------------------
  // Affichage des résultats
  //-----------------------------------
  // printf("Recording name: %s\n", recordingName);
  // printf("Iterations: %d → %d step %d\n", startIteration, endIteration, periodicity);

  // int snapshots = ((endIteration - startIteration) / periodicity) + 1;
  // printf("Snapshots: %d\n", snapshots);

  // printf("Blocks declared: %d\n", numberOfBlocks);
  // for (int b = 0; b < numberOfBlocks; b++)
  // {
  //   printf("  Block %d: %s\n", //, Nodes=%ld, Elements=%ld\n",
  //                             //  blockIds[b], blockNames[b], blockNodes[b], blockElems[b]);
  //           b, blockNames[b]);
  //       }

  // Lecture du fichier .h5
  char* h5_file = makeH5Path(file, recordingName);

  /* Open file */
  hid_t fapl, fid;
  fapl = H5Pcreate(H5P_FILE_ACCESS);
  fid = H5Fopen(h5_file, H5F_ACC_RDONLY, fapl);
  H5Pclose(fapl);
  if (fid < 0)
  {
    printf("Warning: prolbread: can not open file %s.\n", file);
    return 1;
  }

  // Create tree
  PyObject *children1 = PyList_New(0);
  tree = Py_BuildValue("[sOOs]", "tree", Py_None, children1, "CGNSTree");
  
  // Create version
  std::vector<npy_intp> npy_dim_vals(2);
  npy_dim_vals[0] = 1;
  PyObject *children2 = PyList_New(0);
  PyArrayObject *r2 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT32, 1);
  float *pf2 = (float *)PyArray_DATA(r2);
  pf2[0] = 4.0;
  PyObject *version = Py_BuildValue("[sOOs]", "CGNSLibraryVersion", (PyObject *)r2, children2, "CGNSLibraryVersion_t");
  PyList_Append(children1, version);
  Py_DECREF(version);

  // Create Base
  npy_dim_vals[0] = 2;
#ifdef E_DOUBLEINT
  PyArrayObject *r3 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT64, 1);
  int64_t *pp3 = (int64_t *)PyArray_DATA(r3);
#else
  PyArrayObject *r3 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT32, 1);
  int32_t *pp3 = (int32_t *)PyArray_DATA(r3);
#endif
  pp3[0] = 3; pp3[1] = 3;
  PyObject *children3 = PyList_New(0);
  PyObject *base = Py_BuildValue("[sOOs]", "Base", r3, children3, "CGNSBase_t");
  PyList_Append(children1, base);
  Py_DECREF(base);

  for (int b = 0; b < numberOfBlocks; b++)
  {

    // Create Zone
    npy_dim_vals[0] = 1; npy_dim_vals[1] = 3;
#ifdef E_DOUBLEINT
    PyArrayObject *r4 = (PyArrayObject *)PyArray_EMPTY(2, &npy_dim_vals[0], NPY_INT64, 1);
    int64_t *pp4 = (int64_t *)PyArray_DATA(r4);
#else
    PyArrayObject *r4 = (PyArrayObject *)PyArray_EMPTY(2, &npy_dim_vals[0], NPY_INT32, 1);
    int32_t *pp4 = (int32_t *)PyArray_DATA(r4);
#endif
    pp4[0] = 0; pp4[1] = 0; pp4[2] = 0;
    PyObject *children4 = PyList_New(0);
    PyObject *zone = Py_BuildValue("[sOOs]", blockNames[b], r4, children4, "Zone_t");
    PyList_Append(children3, zone);
    Py_DECREF(zone);

    // Create ZoneType
    PyObject *children9 = PyList_New(0);
    npy_dim_vals[0] = 12;
    PyArrayObject *r9 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_STRING, 1);
    char *pp9 = (char *)PyArray_DATA(r9);
    K_STRING::cpy(pp9, "Unstructured", 12, false);
    PyObject *zoneType = Py_BuildValue("[sOOs]", "ZoneType", r9, children9, "ZoneType_t");
    PyList_Append(children4, zoneType);
    Py_DECREF(zoneType);

    // Get nvertex
    E_Int nvertex = 0; // nbre de vertex

    // Open Coordinates node
    const char* prefix = "/Coordinates_";
    size_t len = strlen(prefix) + strlen(blockNames[b]) + 1; // +1 pour '\0'
    char *coordName = (char *)malloc(len);
    if (!coordName) continue;
    strcpy(coordName, prefix); strcat(coordName, blockNames[b]);
    //printf("%s\n", coordName); // affiche "Coordinates_SliceZ_cropped"

    hid_t dset = H5Dopen(fid, coordName, H5P_DEFAULT);
    if (dset < 0)
    {
      printf("Warning: prolbread: no Coordinates node for %s.\n", blockNames[b]);
      return 1;
    }
    hid_t dataspace = H5Dget_space(dset);
    hsize_t dims[2];
    H5Sget_simple_extent_dims(dataspace, dims, NULL); // dims[0] = nvertex, dims[1] = 3
    //printf("Nombre de points: %llu, dimension de chaque point: %llu\n", dims[0], dims[1]);
    nvertex = dims[0];
    
    // Create GridCoordinates
    PyObject *children5 = PyList_New(0);
    PyObject *GC = Py_BuildValue("[sOOs]", "GridCoordinates", Py_None, children5, "GridCoordinates_t");
    PyList_Append(children4, GC);
    Py_DECREF(GC);
    
    // On alloue un tableau pour stocker les donnes
    double *r = new double[dims[0]*dims[1]];
    
    hid_t tid = H5Tcopy(H5T_NATIVE_DOUBLE);
    H5Tset_precision(tid, 64);
    hid_t mid = H5S_ALL;
    H5Dread(dset, tid, mid, dataspace, H5P_DEFAULT, r);
    H5Sclose(dataspace); H5Dclose(dset);

    // // CoordinateX
    npy_dim_vals[0] = nvertex;
    PyArrayObject *xc = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT64, 1);
    E_Float *pxc = (E_Float *)PyArray_DATA(xc);
    for (E_Int i = 0; i < nvertex; i++)
      pxc[i] = r[3 * i];
    PyObject *children6 = PyList_New(0);
    PyObject *nxc = Py_BuildValue("[sOOs]", "CoordinateX", xc, children6, "DataArray_t");
    PyList_Append(children5, nxc);
    Py_DECREF(nxc);

    // CoordinateY
    PyArrayObject *yc = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT64, 1);
    E_Float *pyc = (E_Float *)PyArray_DATA(yc);
    for (E_Int i = 0; i < nvertex; i++)
      pyc[i] = r[3 * i + 1];
    PyObject *children7 = PyList_New(0);
    PyObject *nyc = Py_BuildValue("[sOOs]", "CoordinateY", yc, children7, "DataArray_t");
    PyList_Append(children5, nyc);
    Py_DECREF(nyc);

    // CoordinateZ
    PyArrayObject *zc = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT64, 1);
    E_Float *pzc = (E_Float *)PyArray_DATA(zc);
    for (E_Int i = 0; i < nvertex; i++)
      pzc[i] = r[3 * i + 2];
    PyObject *children8 = PyList_New(0);
    PyObject *nzc = Py_BuildValue("[sOOs]", "CoordinateZ", zc, children8, "DataArray_t");
    PyList_Append(children5, nzc);
    Py_DECREF(nzc);
    delete[] r;

    // Create GridElements
    const char *prefix_connect = "/Connectivity_";
    len = strlen(prefix_connect) + strlen(blockNames[b]) + 1; // +1 pour '\0'
    char *connectName = (char *)malloc(len);
    if (!connectName)
      continue;
    strcpy(connectName, prefix_connect);
    strcat(connectName, blockNames[b]);
    // printf("%s\n", connectName); // affiche "Coordinates_SliceZ_cropped"

    dset = H5Dopen(fid, connectName, H5P_DEFAULT);
    if (dset < 0)
    {
      printf("Warning: prolbread: no Connectivity node for %s.\n", blockNames[b]);
      return 1;
    }
    dataspace = H5Dget_space(dset);
    hsize_t dimss[2];
    H5Sget_simple_extent_dims(dataspace, dimss, NULL); // dims[0] = nelem, dims[1] = pts_per_elem
    // printf("Nombre d'elements: %llu, dimension de chaque element: %llu\n", dimss[0], dimss[1]);
    E_Int nelem = dimss[0];

    std::vector<npy_intp> npy_dim_vals(1);
    npy_dim_vals[0] = 2;
#ifdef E_DOUBLEINT
    PyArrayObject *r1 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT64, 1);
    int64_t *pp1 = (int64_t *)PyArray_DATA(r1);
#else
    PyArrayObject *r1 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT32, 1);
    int32_t *pp1 = (int32_t *)PyArray_DATA(r1);
#endif
    pp1[0] = 5; pp1[1] = 0; // Pour le moment juste TRI3
    PyObject *children11 = PyList_New(0);
    PyObject *GE = Py_BuildValue("[sOOs]", "GridElements", r1, children11, "Elements_t");

    // Element range
#ifdef E_DOUBLEINT
    PyArrayObject *r2 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT64, 1);
    int64_t *pp2 = (int64_t *)PyArray_DATA(r2);
#else
    PyArrayObject *r2 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT32, 1);
    int32_t *pp2 = (int32_t *)PyArray_DATA(r2);
#endif
    pp2[0] = 1;
    pp2[1] = 1 + nelem - 1;
    PyObject *children12 = PyList_New(0);
    PyObject *er = Py_BuildValue("[sOOs]", "ElementRange", r2, children12, "IndexRange_t");
    PyList_Append(children11, er); Py_DECREF(er);

    // Element connectivity
    hid_t sid = H5Dget_space(dset);
    E_Int ndims = H5Sget_simple_extent_ndims(sid);
    hsize_t dimsss[3];
    H5Sget_simple_extent_dims(sid, dimsss, NULL);
    E_Int size2 = 1;
    for (E_Int i = 0; i < ndims; i++)
      size2 *= dimsss[i];
    npy_dim_vals[0] = size2;
    mid = H5S_ALL;
#ifdef E_DOUBLEINT
    PyArrayObject *r3 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT64, 1);
    int64_t *pp3 = (int64_t *)PyArray_DATA(r3);
    tid = H5Tcopy(H5T_NATIVE_INT64);
    H5Tset_precision(tid, 64); // mem data type
#else
    PyArrayObject *r3 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT32, 1);
    int32_t *pp3 = (int32_t *)PyArray_DATA(r3);
    tid = H5Tcopy(H5T_NATIVE_INT);
    H5Tset_precision(tid, 32);
#endif
    H5Dread(dset, tid, mid, sid, H5P_DEFAULT, pp3);
    H5Dclose(dset);
    for (E_Int i = 0; i < size2; i++) pp3[i] += 1;
    PyObject *children13 = PyList_New(0);
    PyObject *ec = Py_BuildValue("[sOOs]", "ElementConnectivity", r3, children13, "DataArray_t");
    PyList_Append(children11, ec);
    Py_DECREF(ec);

    PyList_Append(children4, GE);
    Py_DECREF(GE);

    // Create FlowSolution
    hsize_t nobj;
    H5Gget_num_objs(fid, &nobj); // nombre d’objets racine
    for (int ii = 0; ii < nobj; ii++)
    {
      char name[256];
      H5Gget_objname_by_idx(fid, ii, name, sizeof(name));

      // Construction du préfixe attendu : blockName + "_t"
      char prefix_fs[256];
      snprintf(prefix_fs, sizeof(prefix_fs), "%s_t", blockNames[b]);

      // Filtrer les datasets de type scalarPressure
      if (strncmp(name, prefix_fs, strlen(prefix_fs)) == 0 && ii == nobj-1 )
      {
        //printf("Frame dataset : %s\n", name);

        PyObject *childrenFS = PyList_New(0);
        PyObject *FS = Py_BuildValue("[sOOs]", "FlowSolution", Py_None, childrenFS, "FlowSolution_t");
        PyList_Append(children4, FS);
        Py_DECREF(FS);
        npy_dim_vals[0] = 10;
        PyObject *children14 = PyList_New(0);
        PyArrayObject *r14 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_STRING, 1);
        char *pp14 = (char *)PyArray_DATA(r14);
        K_STRING::cpy(pp14, "Vertex", npy_dim_vals[0], false);
        PyObject *gl = Py_BuildValue("[sOOs]", "GridLocation", r14, children14, "GridLocation_t");
        PyList_Append(childrenFS, gl);
        Py_DECREF(gl);

        // Lire ce dataset
        hid_t dset = H5Dopen(fid, name, H5P_DEFAULT);
        hid_t space = H5Dget_space(dset);
        hsize_t dims[2];
        H5Sget_simple_extent_dims(space, dims, NULL); // dims[0] = nvertex, dims[1] = nvars
        E_Int nvertex = dims[0];
        E_Int nvars = dims[1]; //printf("nvars = %d\n", nvars);

        // On alloue un tableau pour stocker les donnes
        double *r = new double[dims[0] * dims[1]];

        hid_t tid = H5Tcopy(H5T_NATIVE_DOUBLE);
        H5Tset_precision(tid, 64);
        hid_t mid = H5S_ALL;
        H5Dread(dset, tid, mid, space, H5P_DEFAULT, r);

        // Pressure
        npy_dim_vals[0] = nvertex;
        PyArrayObject *f = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT64, 1);
        E_Float *fp = (E_Float *)PyArray_DATA(f);
        for (E_Int i = 0; i < nvertex; i++) fp[i] = r[nvars * i];
        PyObject *fc = PyList_New(0);
        PyObject *fl = Py_BuildValue("[sOOs]", "Pressure", f, fc, "DataArray_t");
        PyList_Append(childrenFS, fl); Py_DECREF(fl);
        H5Sclose(space); H5Dclose(dset);

      }

    }
  }

    // Close file
  H5Fclose(fid);

  delete [] buf;
  // delete [] prevData;
  // delete [] keyword;
  return 0;
}

//=============================================================================
/*
   prolbxmlread : lecture du fichier .xmf
*/
//=============================================================================
E_Int K_IO::GenIO::prolbxmlread(char *file, std::vector<const char *>& blockNames, 
                                            std::vector<const char*>& varInfos,
                                            std::vector<int>& itInfos)
{
  
  /* File Opening */
  FILE* ptrFile;
  ptrFile = fopen(file, "r");

  if (ptrFile == NULL)
  {
    printf("Warning: prolbread: cannot open file %s.\n", file);
    return 1;
  }

  // Lecture de l'entete
  E_Int res;

  res = readGivenKeyword(ptrFile, "<?");
  if (res == 0)
  {
    printf("Warning: prolbread: cannot find xml header.\n");
    fclose(ptrFile);
    return 1;
  }
  res = readGivenKeyword(ptrFile, ">");
  if (res == 0)
  {
    printf("Warning: prolbread: file seems to be corrupted.\n");
    fclose(ptrFile);
    return 1;
  }
  
  // Lecture du fichier .xmf
  char* buf = new char[BUFSIZE];

  bool inMetaData = false;
  bool inSimInfo = false, inSimParams = false, inUserVars = false;
  bool inRecording = false, inBlock = false;
  E_Int depth = 0;

  bool inGridSpatial = false, inGridTemporal = false, inTimeFrame = false;
  E_Int timeFrameCount = 0; 

  // vector<const char*> blockNames;
  char *recordingName = new char[256];
  E_Int startIteration = 0;
  E_Int endIteration = 0;
  E_Int periodicity = 1;
  E_Int numberOfBlocks = 0;


  while ((res = readline(ptrFile, buf, BUFSIZE)) == 0)
  {

    // On supprime les espaces en debut de ligne
    E_Int i = 0; while (buf[i] == ' ' || buf[i] == '\t') i++;
    if (i > 0) memmove(buf, buf + i, strlen(buf + i) + 1);

    // DEBUG : affiche la ligne et les flags avant traitement
    // if (!inMetaData) printf("Ligne: %s", buf);
    // printf("  Flags avant => MetaData:%d SimuInfo:%d SimuParams:%d UserVars:%d Recording:%d Block:%d (depth=%d)\n",
    //        inMetaData, inSimInfo, inSimParams, inUserVars, inRecording, inBlock, depth);

    // Detecteurs ouverture / fermeture sur la ligne
    bool hasOpen     = (strstr(buf, "<Information") != nullptr);
    bool hasClose    = (strstr(buf, "</Information>") != nullptr);
    bool selfClosing = (strstr(buf, "/>") != nullptr);

    // Detection ouverture ET fermeture sur la même ligne (fermeture après ouverture)
    bool immediateClose = false;
    if (hasOpen && hasClose) {
        const char* pOpen = strstr(buf, "<Information");
        const char* pClose = strstr(buf, "</Information>");
        // On s'assure que la fermeture vient APRES l'ouverture
        if (pOpen && pClose && pClose > pOpen) immediateClose = true;
    }

    // Entree dans MetaData : on considere MetaData comme un niveau (depth = 1)
    if (!inMetaData && hasOpen && strstr(buf, "MetaData"))
    {
        inMetaData = true;
        depth = 1; // MetaData compte pour depth = 1
        continue; // On passe a la ligne suivante
    }

    // Traitement des blocs dans MetaData
    if (inMetaData)
    {
        if (hasOpen)  // On a une ouverture
        {
            // Lecture Name/Value
            char name[256] = "", value[256] = "";
            getAttributeValue(buf, "Name", name);//, sizeof(name));
            getAttributeValue(buf, "Value", value); //, sizeof(value));

            // Si ouverture reelle (non self-closing, et pas open+close sur meme ligne)
            if (!selfClosing && !immediateClose && !hasClose)
            {
                // on entre dans un nouveau niveau
                ++depth;

                // activer le flag correspondant
                if (strcmp(name, "SimulationInformations") == 0) inSimInfo = true;
                else if (strcmp(name, "SimulationParameters") == 0) inSimParams = true;
                else if (strcmp(name, "UserVariables") == 0) inUserVars = true;
                else if (strcmp(name, "RecordingParameters") == 0) inRecording = true;
                else if (strcmp(name, "BlockId") == 0) inBlock = true;

                // DEBUG : on affiche ce qu'on ouvre affichage indicatif
                // printf("[OPEN %s] %s = %s (depth=%d)\n", name, name, value, depth);

            }
            else // self-closing ou open+close sur la même ligne : on recupere les paramètres qui nous interessent
            {

              // On recupere la version de ProLB
              char val[256];
              E_Int check = getValueFromName(buf, inSimInfo, "LBsolverVersion", val);

              check = getValueFromName(buf, inRecording, "RecordingName", recordingName);

              check = getValueFromName(buf, inRecording, "StartIteration", &startIteration);
              check = getValueFromName(buf, inRecording, "EndIteration", &endIteration);
              check = getValueFromName(buf, inRecording, "Periodicity", &periodicity);
              
              char bName[256];
              check = getValueFromName(buf, inBlock, "BlockName", bName);
              if (check==0)
                blockNames.push_back(strdup(bName));
            }
        }
        else if (hasClose && !hasOpen) // ligne contenant uniquement une fermeture </Information>
        {
            // désactiver le flag le plus profond
            if (inBlock)
                inBlock = false;
            else if (inRecording)
                inRecording = false;
            else if (inUserVars)
                inUserVars = false;
            else if (inSimParams)
                inSimParams = false;
            else if (inSimInfo)
                inSimInfo = false;

            // décrémenter le depth (on était dans MetaData donc depth >= 1 normalement)
            --depth;

            if (depth < 0) {
                // erreur d'imbrication
                fprintf(stderr, "Erreur : depth < 0 (XML mal imbriqué ?) -> stop\n");
                break;
            }

            // Si on est revenu à 0 => on a complètement quitté MetaData
            if (depth == 0) {
                inMetaData = false;
                // break; // ici on sort complètement du parsing de MetaData (volontaire)
            }
        }
    }

    //==================================================================================
    // Detecteurs ouverture / fermeture sur la ligne
    hasOpen = (strstr(buf, "<Grid") != nullptr);
    hasClose = (strstr(buf, "</Grid>") != nullptr);
    selfClosing = (strstr(buf, "/>") != nullptr);

    // detecte ouverture+fermeture sur la même ligne : "<Grid ...> ... </Grid>"
    immediateClose = false;
    if (hasOpen && hasClose)
    {
      const char *pOpen = strstr(buf, "<Grid");
      const char *pClose = strstr(buf, "</Grid>");
      if (pOpen && pClose && pClose > pOpen)
        immediateClose = true;
    }

    // Entree dans Grid : on considere Grid comme un niveau (depth = 1)
    if (!inGridSpatial && hasOpen && strstr(buf, "CollectionType=\"Spatial\""))
    {
      inGridSpatial = true;
      depth = 1;
      continue;
    }

    // Traitement des blocs dans Grid
    if (inGridSpatial)
    {
      if (hasOpen)
      {
        // Si ouverture reelle (non self-closing, et pas open+close sur meme ligne)
        if (!selfClosing && !immediateClose && !hasClose)
        {
          // on entre dans un nouveau niveau
          ++depth;

          // activer le flag correspondant
          if (strstr(buf, "CollectionType=\"Temporal\"") != nullptr)
            inGridTemporal = true;
          else if (strstr(buf, "GridType=\"Uniform\"") != nullptr)
            inTimeFrame = true;
        }
      }
      else if (hasClose && !hasOpen)
      {
        // désactiver le flag le plus profond
        if (inTimeFrame)
        {
          inTimeFrame = false;
          timeFrameCount++;
          if (timeFrameCount >= 1)
            break;
        }
        else if (inGridTemporal)
          inGridTemporal = false;

        // décrémenter le depth (on était dans MetaData donc depth >= 1 normalement)
        --depth;

        // Si on est revenu à 0 => on a complètement quitté MetaData
        if (depth == 0)
        {
          inGridSpatial = false;
          break; // ici on sort complètement du parsing de MetaData (volontaire)
        }
      }
      else
      {
        if (inTimeFrame && strstr(buf, "<Attribute") != nullptr && timeFrameCount == 0)
        {
          char varName[256] = "";
          getAttributeValue(buf, "Name", varName);//, sizeof(tempName)))
          char varType[256] = "";
          getAttributeValue(buf, "AttributeType", varType);
          // On stocke les infos sur les variables
          varInfos.push_back(strdup(varName));
          varInfos.push_back(strdup(varType));
        }
      }
    }
    // DEBUG :flags apres traitement
    // printf("  Flags après => MetaData:%d SimuInfo:%d SimuParams:%d UserVars:%d Recording:%d Block:%d (depth=%d)\n\n",
    //       inMetaData, inSimInfo, inSimParams, inUserVars, inRecording, inBlock, depth);
    // printf("  Flags après => inMetaData:%d inGridSpatial:%d inGridTemporal:%d inTimeFrame:%d, timeFrameCount:%d (depth=%d)\n\n",
    //        inMetaData, inGridSpatial, inGridTemporal, inTimeFrame, timeFrameCount, depth);

    } // fin while
  fclose(ptrFile);

  //-----------------------------------
  // Affichage des résultats
  //-----------------------------------
  // printf("Recording name: %s\n", recordingName);
  // printf("Iterations: %d → %d step %d\n", startIteration, endIteration, periodicity);

  int snapshots = ((endIteration - startIteration) / periodicity) + 1;
  // printf("Snapshots: %d\n", snapshots);
  // printf("Itérations des snapshots:\n");

  for (int i = 0; i < snapshots; i++)
  {
    int iteration = startIteration + i * periodicity;
    itInfos.push_back(iteration);
    // printf("Snapshot %d -> Itération %d\n", i + 1, iteration);
  }

  // printf("Blocks declared: %d\n", blockNames.size()); //numberOfBlocks);
  // for (int b = 0; b < blockNames.size(); b++)
  // {
  //   printf("  Block %d: %s\n", //, Nodes=%ld, Elements=%ld\n",
  //                              //  blockIds[b], blockNames[b], blockNodes[b], blockElems[b]);
  //          b, blockNames[b]);
  //       }

  delete [] buf;

  return 0;
}

//=============================================================================
/*
   prolbh5read : lecture du fichier .h5
*/
//=============================================================================
E_Int K_IO::GenIO::prolbh5read(char *file, const std::vector<const char *> &blockNames, 
                                           const std::vector<const char *> &varInfos, 
                                           const std::vector<int> &itInfos, PyObject *&tree)
{

  // Open file
  hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
  if (fapl < 0) return 1;
  hid_t fid = H5Fopen(file, H5F_ACC_RDONLY, fapl);
  H5Pclose(fapl);
  if (fid < 0)
  {
    printf("Warning: prolbh5read: can not open file %s.\n", file);
    return 1;
  }

  // Create tree root
  PyObject *children1 = PyList_New(0);
  tree = Py_BuildValue("[sOOs]", "tree", Py_None, children1, "CGNSTree");

  // Create version
  std::vector<npy_intp> npy_dim_vals(2);
  npy_dim_vals[0] = 1;
  PyObject *children2 = PyList_New(0);
  PyArrayObject *r2 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT32, 1);
  float *pf2 = (float *)PyArray_DATA(r2);
  pf2[0] = 4.0;
  PyObject *version = Py_BuildValue("[sOOs]", "CGNSLibraryVersion", (PyObject *)r2, children2, "CGNSLibraryVersion_t");
  PyList_Append(children1, version); Py_DECREF(version);

  // Create Base
  npy_dim_vals[0] = 2;
#ifdef E_DOUBLEINT
  PyArrayObject *r3 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT64, 1);
  int64_t *pp3 = (int64_t *)PyArray_DATA(r3);
#else
  PyArrayObject *r3 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT32, 1);
  int32_t *pp3 = (int32_t *)PyArray_DATA(r3);
#endif
  pp3[0] = 3; pp3[1] = 3;
  PyObject *children3 = PyList_New(0);
  PyObject *base = Py_BuildValue("[sOOs]", "Base", r3, children3, "CGNSBase_t");
  PyList_Append(children1, base); Py_DECREF(base);

  // Loop over blocks (zones)
  for (int b = 0; b < blockNames.size(); b++)
  {
    
    // printf("Working on block number = %d\n", b);

    // -- -Create Zone node-- -
    npy_dim_vals[0] = 1; npy_dim_vals[1] = 3;
#ifdef E_DOUBLEINT
    PyArrayObject *r4 = (PyArrayObject *)PyArray_EMPTY(2, &npy_dim_vals[0], NPY_INT64, 1);
    int64_t *pp4 = (int64_t *)PyArray_DATA(r4);
#else
    PyArrayObject *r4 = (PyArrayObject *)PyArray_EMPTY(2, &npy_dim_vals[0], NPY_INT32, 1);
    int32_t *pp4 = (int32_t *)PyArray_DATA(r4);
#endif
    pp4[0] = 0; pp4[1] = 0; pp4[2] = 0;
    PyObject *children4 = PyList_New(0);
    PyObject *zone = Py_BuildValue("[sOOs]", blockNames[b], r4, children4, "Zone_t");
    PyList_Append(children3, zone); Py_DECREF(zone);

    // --- ZoneType ---
    PyObject *children9 = PyList_New(0);
    npy_dim_vals[0] = 12;
    PyArrayObject *r9 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_STRING, 1);
    char *pp9 = (char *)PyArray_DATA(r9);
    K_STRING::cpy(pp9, "Unstructured", 12, false);
    PyObject *zoneType = Py_BuildValue("[sOOs]", "ZoneType", r9, children9, "ZoneType_t");
    PyList_Append(children4, zoneType); Py_DECREF(zoneType);

    // --- Open Coordinates dataset ---
    E_Int nvertex = 0; // nbre de vertex
    const char *prefix = "/Coordinates_";
    size_t len = strlen(prefix) + strlen(blockNames[b]) + 1; // +1 pour '\0'
    char *coordName = (char *)malloc(len);
    if (!coordName) continue;
    strcpy(coordName, prefix); strcat(coordName, blockNames[b]);
    // printf("%s\n", coordName); // affiche "Coordinates_SliceZ_cropped"

    hid_t dset = H5Dopen(fid, coordName, H5P_DEFAULT);
    if (dset < 0)
    {
      printf("Warning: prolbread: no Coordinates node for %s.\n", blockNames[b]);
      return 1;
    }
    hid_t dataspace = H5Dget_space(dset);
    hsize_t dims[2];
    H5Sget_simple_extent_dims(dataspace, dims, NULL); // dims[0] = nvertex, dims[1] = 3 ou 8
    // printf("Nombre de points: %llu, dimension de chaque point: %llu\n", dims[0], dims[1]);
    nvertex = dims[0]; pp4[0] = nvertex;

    // --- GridCoordinates: create X/Y/Z arrays and add them to children4 ---
    PyObject *children5 = PyList_New(0);
    PyObject *GC = Py_BuildValue("[sOOs]", "GridCoordinates", Py_None, children5, "GridCoordinates_t");
    PyList_Append(children4, GC); Py_DECREF(GC);

    // On alloue un tableau pour stocker les donnes
    double *r = new double[dims[0] * dims[1]];

    hid_t tid = H5Tcopy(H5T_NATIVE_DOUBLE);
    H5Tset_precision(tid, 64);
    hid_t mid = H5S_ALL;
    H5Dread(dset, tid, mid, dataspace, H5P_DEFAULT, r);
    H5Sclose(dataspace); H5Dclose(dset);

    // CoordinateX
    npy_dim_vals[0] = nvertex;
    PyArrayObject *xc = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT64, 1);
    E_Float *pxc = (E_Float *)PyArray_DATA(xc);
    for (E_Int i = 0; i < nvertex; i++) pxc[i] = r[3 * i];
    PyObject *children6 = PyList_New(0);
    PyObject *nxc = Py_BuildValue("[sOOs]", "CoordinateX", xc, children6, "DataArray_t");
    PyList_Append(children5, nxc); Py_DECREF(nxc);

    // CoordinateY
    PyArrayObject *yc = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT64, 1);
    E_Float *pyc = (E_Float *)PyArray_DATA(yc);
    for (E_Int i = 0; i < nvertex; i++) pyc[i] = r[3 * i + 1];
    PyObject *children7 = PyList_New(0);
    PyObject *nyc = Py_BuildValue("[sOOs]", "CoordinateY", yc, children7, "DataArray_t");
    PyList_Append(children5, nyc); Py_DECREF(nyc);

    // CoordinateZ
    PyArrayObject *zc = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT64, 1);
    E_Float *pzc = (E_Float *)PyArray_DATA(zc);
    for (E_Int i = 0; i < nvertex; i++) pzc[i] = r[3 * i + 2];
    PyObject *children8 = PyList_New(0);
    PyObject *nzc = Py_BuildValue("[sOOs]", "CoordinateZ", zc, children8, "DataArray_t");
    PyList_Append(children5, nzc); Py_DECREF(nzc);
    
    delete[] r;

    // Create GridElements
    const char *prefix_connect = "/Connectivity_";
    len = strlen(prefix_connect) + strlen(blockNames[b]) + 1; // +1 pour '\0'
    char *connectName = (char *)malloc(len);
    if (!connectName) continue;
    strcpy(connectName, prefix_connect); strcat(connectName, blockNames[b]);
    // printf("%s\n", connectName); // affiche "Coordinates_SliceZ_cropped"

    dset = H5Dopen(fid, connectName, H5P_DEFAULT);
    if (dset < 0)
    {
      printf("Warning: prolbread: no Connectivity node for %s.\n", blockNames[b]);
      return 1;
    }
    dataspace = H5Dget_space(dset);
    hsize_t dimss[2];
    H5Sget_simple_extent_dims(dataspace, dimss, NULL); // dims[0] = nelem, dims[1] = eltType
    // printf("Nombre d'elements: %llu, dimension de chaque element: %llu\n", dimss[0], dimss[1]);
    E_Int nelem = dimss[0]; E_Int eltType = dimss[1];
    pp4[1] = nelem; 

    std::vector<npy_intp> npy_dim_vals(1);
    npy_dim_vals[0] = 2;
#ifdef E_DOUBLEINT
    PyArrayObject *r1 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT64, 1);
    int64_t *pp1 = (int64_t *)PyArray_DATA(r1);
#else
    PyArrayObject *r1 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT32, 1);
    int32_t *pp1 = (int32_t *)PyArray_DATA(r1);
#endif
    if (eltType == 3) pp1[0] = 5;
    else if (eltType == 8) pp1[0] = 17;
    pp1[1] = 0;
    PyObject *children11 = PyList_New(0);
    PyObject *GE = Py_BuildValue("[sOOs]", "GridElements", r1, children11, "Elements_t");
    
    // Element range
#ifdef E_DOUBLEINT
    PyArrayObject *r2 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT64, 1);
    int64_t *pp2 = (int64_t *)PyArray_DATA(r2);
#else
    PyArrayObject *r2 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT32, 1);
    int32_t *pp2 = (int32_t *)PyArray_DATA(r2);
#endif
    pp2[0] = 1; pp2[1] = 1 + nelem - 1;
    PyObject *children12 = PyList_New(0);
    PyObject *er = Py_BuildValue("[sOOs]", "ElementRange", r2, children12, "IndexRange_t");
    PyList_Append(children11, er); Py_DECREF(er);

    // Element connectivity
    hid_t sid = H5Dget_space(dset);
    E_Int ndims = H5Sget_simple_extent_ndims(sid);
    hsize_t dimsss[3];
    H5Sget_simple_extent_dims(sid, dimsss, NULL);

    E_Int size2 = 1;
    for (E_Int i = 0; i < ndims; i++)
      size2 *= dimsss[i];
    npy_dim_vals[0] = size2;
    mid = H5S_ALL;
#ifdef E_DOUBLEINT
    PyArrayObject *r3 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT64, 1);
    int64_t *pp3 = (int64_t *)PyArray_DATA(r3);
    tid = H5Tcopy(H5T_NATIVE_INT64);
    H5Tset_precision(tid, 64); // mem data type
#else
    PyArrayObject *r3 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT32, 1);
    int32_t *pp3 = (int32_t *)PyArray_DATA(r3);
    tid = H5Tcopy(H5T_NATIVE_INT);
    H5Tset_precision(tid, 32);
#endif
    H5Dread(dset, tid, mid, sid, H5P_DEFAULT, pp3);
    H5Dclose(dset);
    for (E_Int i = 0; i < size2; i++)
      pp3[i] += 1;
    PyObject *children13 = PyList_New(0);
    PyObject *ec = Py_BuildValue("[sOOs]", "ElementConnectivity", r3, children13, "DataArray_t");
    PyList_Append(children11, ec);
    Py_DECREF(ec);

    PyList_Append(children4, GE);
    Py_DECREF(GE);

    for (int iit = 0; iit < itInfos.size(); iit++)
    {
      //printf("Snapshot %d -> Itération %d\n", iit + 1, itInfos[iit]);

      // Create FlowSolution#iit
      char flowSolName[64];
      if (iit==0 && itInfos.size() == 1)
        strcpy(flowSolName, "FlowSolution");
      else
        snprintf(flowSolName, sizeof(flowSolName), "FlowSolution#%d", iit+1);
      PyObject *childrenFS = PyList_New(0);
      PyObject *FS = Py_BuildValue("[sOOs]", flowSolName, Py_None, childrenFS, "FlowSolution_t");
      PyList_Append(children4, FS);
      Py_DECREF(FS);
      npy_dim_vals[0] = 10;
      PyObject *children14 = PyList_New(0);
      PyArrayObject *r14 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_STRING, 1);
      char *pp14 = (char *)PyArray_DATA(r14);
      K_STRING::cpy(pp14, "Vertex", npy_dim_vals[0], false);
      PyObject *gl = Py_BuildValue("[sOOs]", "GridLocation", r14, children14, "GridLocation_t");
      PyList_Append(childrenFS, gl);
      Py_DECREF(gl);

      npy_dim_vals[0] = 1;
      PyObject *children141 = PyList_New(0);
      PyArrayObject *r141 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_INT32, 1);
      int32_t *pp141 = (int32_t *)PyArray_DATA(r141);
      pp141[0] = itInfos[iit];
      PyObject *gl1 = Py_BuildValue("[sOOs]", "Iteration", r141, children141, "UserDefined_t");
      PyList_Append(childrenFS, gl1);
      Py_DECREF(gl1);

      for (int ivar = 0; ivar < varInfos.size()/2.; ivar++)
      {
        char datasetName[256];
        snprintf(datasetName, sizeof(datasetName), "%s_t%09d_%s", blockNames[b], itInfos[iit], varInfos[2*ivar]);
        // printf("%s\n", datasetName);

        // Lire ce dataset
        hid_t dset = H5Dopen(fid, datasetName, H5P_DEFAULT);
        hid_t space = H5Dget_space(dset);
        hsize_t dims[2];
        H5Sget_simple_extent_dims(space, dims, NULL); // dims[0] = nvertex, dims[1] = nvars
        E_Int nvertex = dims[0]; E_Int nvars = dims[1]; // printf("nvars = %d\n", nvars);

        // On alloue un tableau pour stocker les donnes
        double *r = new double[dims[0] * dims[1]];

        hid_t tid = H5Tcopy(H5T_NATIVE_DOUBLE);
        H5Tset_precision(tid, 64);
        hid_t mid = H5S_ALL;
        H5Dread(dset, tid, mid, space, H5P_DEFAULT, r);
        
        npy_dim_vals[0] = nvertex;
        // Ajout des variables dans l'arbre
        for (E_Int varn = 0; varn < nvars; varn++)
        { 
          // printf("Varn = %d", varn);
          PyArrayObject *f = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT64, 1);
          E_Float *fp = (E_Float *)PyArray_DATA(f);
          for (E_Int i = 0; i < nvertex; i++)
            fp[i] = r[nvars * i + varn];
          if (varn==0 && nvars==1)
          {
            // printf("Ajout de : %s\n", datasetName); // X
            PyObject *fc = PyList_New(0);
            PyObject *fl = Py_BuildValue("[sOOs]", varInfos[2*ivar], f, fc, "DataArray_t");
            PyList_Append(childrenFS, fl);
            Py_DECREF(fl);
          }
          else if (varn==0 && nvars==3) // Composante X d'un vecteur
          {
            char newVar[128];
            snprintf(newVar, sizeof(newVar), "%sX", varInfos[2*ivar]);
            // printf("Nouvelle variable : %s\n", newVar); // X
            PyObject *fc = PyList_New(0);
            PyObject *fl = Py_BuildValue("[sOOs]", newVar, f, fc, "DataArray_t");
            PyList_Append(childrenFS, fl);
            Py_DECREF(fl);
          }
          else if (varn==1 && nvars==3) // Composante Y d'un vecteur
          {
            char newVar[128];
            snprintf(newVar, sizeof(newVar), "%sY", varInfos[2*ivar]);
            // printf("Nouvelle variable : %s\n", newVar); 
            PyObject *fc = PyList_New(0);
            PyObject *fl = Py_BuildValue("[sOOs]", newVar, f, fc, "DataArray_t");
            PyList_Append(childrenFS, fl);
            Py_DECREF(fl);
          }
          else if (varn == 2 && nvars == 3) // Composante Z d'un vecteur
          {
            char newVar[128];
            snprintf(newVar, sizeof(newVar), "%sZ", varInfos[2*ivar]);
            // printf("Nouvelle variable : %s\n", newVar);
            PyObject *fc = PyList_New(0);
            PyObject *fl = Py_BuildValue("[sOOs]", newVar, f, fc, "DataArray_t");
            PyList_Append(childrenFS, fl); Py_DECREF(fl);
          }
        }
        H5Sclose(space);
        H5Dclose(dset);
      }

    }
    // // Create FlowSolution
    // PyObject *childrenFS = PyList_New(0);
    // PyObject *FS = Py_BuildValue("[sOOs]", "FlowSolution", Py_None, childrenFS, "FlowSolution_t");
    // PyList_Append(children4, FS);
    // Py_DECREF(FS);
    // npy_dim_vals[0] = 10;
    // PyObject *children14 = PyList_New(0);
    // PyArrayObject *r14 = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_STRING, 1);
    // char *pp14 = (char *)PyArray_DATA(r14);
    // K_STRING::cpy(pp14, "Vertex", npy_dim_vals[0], false);
    // PyObject *gl = Py_BuildValue("[sOOs]", "GridLocation", r14, children14, "GridLocation_t");
    // PyList_Append(childrenFS, gl);
    // Py_DECREF(gl);

    // hsize_t nobj; H5Gget_num_objs(fid, &nobj); // nombre d’objets racine
    // for (int ii = 0; ii < nobj; ii++)
    // {
    //   char name[256];
    //   H5Gget_objname_by_idx(fid, ii, name, sizeof(name));

    //   // Construction du préfixe attendu : blockName + "_t"
    //   char prefix_fs[256];
    //   snprintf(prefix_fs, sizeof(prefix_fs), "%s_t", blockNames[b]);

    //   // Filtrer les datasets : on identifie ceux qui commencent par "blockName_t"
    //   if (strncmp(name, prefix_fs, strlen(prefix_fs)) == 0) 
    //   {

    //     printf("Frame dataset : %s\n", name);
    //     const char *var = getVariableFromFrame(name);
    //     int timestep = getTimeFromFrame(name);
    //     printf("Timestep = %d\n", timestep);

    //     // Lire ce dataset
    //     hid_t dset = H5Dopen(fid, name, H5P_DEFAULT);
    //     hid_t space = H5Dget_space(dset);
    //     hsize_t dims[2];
    //     H5Sget_simple_extent_dims(space, dims, NULL); // dims[0] = nvertex, dims[1] = nvars
    //     E_Int nvertex = dims[0]; E_Int nvars = dims[1]; // printf("nvars = %d\n", nvars);

    //     // On alloue un tableau pour stocker les donnes
    //     double *r = new double[dims[0] * dims[1]];

    //     hid_t tid = H5Tcopy(H5T_NATIVE_DOUBLE);
    //     H5Tset_precision(tid, 64);
    //     hid_t mid = H5S_ALL;
    //     H5Dread(dset, tid, mid, space, H5P_DEFAULT, r);
        
    //     npy_dim_vals[0] = nvertex;
    //     // Ajout des variables dans l'arbre
    //     for (E_Int varn = 0; varn < nvars; varn++)
    //     { 
    //       // printf("Varn = %d", varn);
    //       PyArrayObject *f = (PyArrayObject *)PyArray_EMPTY(1, &npy_dim_vals[0], NPY_FLOAT64, 1);
    //       E_Float *fp = (E_Float *)PyArray_DATA(f);
    //       for (E_Int i = 0; i < nvertex; i++)
    //         fp[i] = r[nvars * i + varn];
    //       if (varn==0 && nvars==1)
    //       {
    //         PyObject *fc = PyList_New(0);
    //         PyObject *fl = Py_BuildValue("[sOOs]", var, f, fc, "DataArray_t");
    //         PyList_Append(childrenFS, fl);
    //         Py_DECREF(fl);
    //       }
    //       else if (varn==0 && nvars==3) // Composante X d'un vecteur
    //       {
    //         char newVar[128];
    //         snprintf(newVar, sizeof(newVar), "%sX", var);
    //         // printf("Nouvelle variable : %s\n", newVar); // X
    //         PyObject *fc = PyList_New(0);
    //         PyObject *fl = Py_BuildValue("[sOOs]", newVar, f, fc, "DataArray_t");
    //         PyList_Append(childrenFS, fl);
    //         Py_DECREF(fl);
    //       }
    //       else if (varn==1 && nvars==3) // Composante Y d'un vecteur
    //       {
    //         char newVar[128];
    //         snprintf(newVar, sizeof(newVar), "%sY", var);
    //         // printf("Nouvelle variable : %s\n", newVar); 
    //         PyObject *fc = PyList_New(0);
    //         PyObject *fl = Py_BuildValue("[sOOs]", newVar, f, fc, "DataArray_t");
    //         PyList_Append(childrenFS, fl);
    //         Py_DECREF(fl);
    //       }
    //       else if (varn == 2 && nvars == 3) // Composante Z d'un vecteur
    //       {
    //         char newVar[128];
    //         snprintf(newVar, sizeof(newVar), "%sZ", var);
    //         // printf("Nouvelle variable : %s\n", newVar);
    //         PyObject *fc = PyList_New(0);
    //         PyObject *fl = Py_BuildValue("[sOOs]", newVar, f, fc, "DataArray_t");
    //         PyList_Append(childrenFS, fl); Py_DECREF(fl);
    //       }
    //     }
    //     H5Sclose(space);
    //     H5Dclose(dset);
    //   }
    // }
  }

  // Close file
  H5Fclose(fid);

  return 0;
}
