/*    
    Copyright 2013-2018 Onera.

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

// Onera v3d binary file support

# include "GenIO.h"
# include "Array/Array.h"

# include <stdio.h>
# include <string.h>

using namespace std;
using namespace K_FLD;
//=============================================================================
// Read an int on ptrFile
// OUT: value: value to be read
// IN: size: size of value
// INOUT: ptrFile: stream
// IN: convertEndian: endian conversion
// IN: sizeInt: sizeof(int)
// IN: sizeLong: sizeof(E_LONG)
//=============================================================================
E_Int K_IO::GenIO::readInt(E_Int& value,
                           E_Int size, 
                           FILE* ptrFile, 
                           E_Boolean convertEndian,
                           E_Int sizeInt, E_Int sizeLong)
{
  int ib; E_LONG lb; E_Int ret;
  if (size == sizeInt)
  {
    ret = fread(&ib, sizeInt, 1, ptrFile);
    if (convertEndian == true) value = IBE(ib);
    else value = ib;
  }
  else
  {
    ret = fread(&lb, sizeLong, 1, ptrFile);
    if (convertEndian == true) value = LBE(lb);
    else value = lb;
  }
  return ret;
}

//=============================================================================
// Write an int on ptrFile
// IN: value: value to be written
// IN: size: size value should appear in file
// INOUT: ptrFile: stream
// IN: convertEndian: endian conversion
// IN: sizeInt: sizeof(int)
// IN: sizeLong: sizeof(E_LONG)
//=============================================================================
void K_IO::GenIO::writeInt(E_Int value, 
                           E_Int size, 
                           FILE* ptrFile, 
                           E_Boolean convertEndian,
                           E_Int sizeInt, E_Int sizeLong)
{
  int ib; E_LONG lb;
  if (size == sizeInt)
  {
    ib = value;
    if (convertEndian == true) ib = IBE(ib);
    fwrite(&ib, sizeInt, 1, ptrFile);
  }
  else
  {
    lb = value;
    if (convertEndian == true) lb = LBE(lb);
    fwrite(&lb, sizeLong, 1, ptrFile);
  }
}

//=============================================================================
/* v3dread */
/* Read binary v3d file as generated by ifort compiler. 
 IN: file: file name
 OUT: varString: strings of read variables
 OUT: ni, nj, nk: vector of number of points of read zones
 OUT: field: read field.
 Return 0 on success, 1 on failure.
 Automatically detects :
 - endianess
 - i4/i8
   Manque le r4/r8 pour l'instant.
   Fonctionne si tous les blocs dans le fichier ont les memes variables.
   Il faut que les domaines soient stockes par paquets :
   Toutes les variables du domaine 1,
   Toutes les variables du domaine 2, ...
*/
//=============================================================================
E_Int K_IO::GenIO::v3dread(
  char* file, char*& varString,
  vector<E_Int>& ni, vector<E_Int>& nj, vector<E_Int>& nk,
  vector<FldArrayF*>& field, vector<char*>& zoneNames)
{
  FILE* ptrFile;
  E_Int s;
  char dummy[BUFSIZE+1];
  E_Int i, n;
  int ib;
  FldArrayF* buf = NULL;
  varString = new char [K_ARRAY::VARSTRINGLENGTH];

  /* Constants */
  E_Int si = sizeof(int);
  E_Int si2 = sizeof(E_LONG);

  // Check endianess
  E_Int ret = v3dCheckEndian(file);
  if (ret == -1)
  {
    printf("Warning: v3dread: can not open file %s.\n", file);
    return 1;
  }
  if (ret == -2)
  {
    printf("Warning: v3dread: format in file %s is not recognised.\n", file);
    return 1;
  }
  else if (ret == 0)
  {
    _convertEndian = false;
    _intLength = 4;
  }
  else if (ret == 1)
  {
    _convertEndian = false;
    _intLength = 8;
  }
  else if (ret == 2)
  {
    _convertEndian = true;
    _intLength = 4;
  }
  else
  {
    _convertEndian = true;
    _intLength = 8;
  }

  /* Opening */
  ptrFile = fopen(file, "rb");
  
  E_Int prevDom = -1; // domaine precedent lu
  E_Int currentVar = 0; // numero de la variable courante
  E_Int refdom = -1; // domaine de reference pour les variables
  E_Int nrec, nvar=0;

  E_Int loop = readInt(s, si,
                       ptrFile, _convertEndian, si, si2);
  E_Int domain = 0;

  // Il y deux philosophie au multibloc.
  // 1. Stocker au debut nrec = nvar*nb, lire tous les nrecs et distribue suivant ndom
  // 2. Lire plusieurs nrec = nvar pour chaque bloc
  // Cette routine doit pouvoir lire les 2.

  while (loop > 0)
  {
    // read nvars
    //readInt(s, si,
    //        ptrFile, _convertEndian, si, si2);
    if (s != _intLength) // sep: taille int
    {
      printf("Warning: v3dread: binary file corrupted.\n");
      fclose(ptrFile);
      return 1;
    }
    readInt(nrec, _intLength, 
            ptrFile, _convertEndian, si, si2);
    readInt(s, si, 
            ptrFile, _convertEndian, si, si2);
    if (s != _intLength) // sep : taille int
    {
      printf("Warning: v3dread: binary file corrupted.\n");
      fclose(ptrFile);
      return 1;
    }
    nvar = nrec;

    for (n = 0; n < nrec; n++)
    {
      // Read variable string
      readInt(s, si, 
              ptrFile, _convertEndian, si, si2);

      E_Int nchar = s; // nombre de characters
      fread(dummy, sizeof(char), nchar, ptrFile);
 
      // Suppress the end
      i = nchar-1;
      while (dummy[i] == ' ' && i >=0) i--;
      dummy[i+1] = '\0';
      //printf("var: %s\n", dummy);

      // Suppress va if needed
      if (dummy[0] == 'v' && dummy[1] == 'a')
      {
        s = 2; while (dummy[s] == ' ' && s < nchar+1) s++;

        i = s;
        while (dummy[i] != '\0' && i < nchar+1)
        {
          dummy[i-s] = dummy[i]; i++;
        }
        dummy[i-s] = '\0';
      }
    
      // Alignement
      readInt(s, si,
              ptrFile, _convertEndian, si, si2);
      if (s != nchar)
      {
        printf("Warning: v3dread: binary file corrupted.\n");
        fclose(ptrFile);
        return 1;
      }

      if (strcmp(dummy, "x") != 0 && 
          strcmp(dummy, "y") != 0 &&
          strcmp(dummy, "z") != 0)
      {
        readInt(s, si,
                ptrFile, _convertEndian, si, si2);
        if (s != 5*_intLength)
        {
          printf("Warning: v3dread: binary file corrupted.\n");
          fclose(ptrFile);
          return 1;
        }

        E_Int indvar;
        readInt(indvar, _intLength,
                ptrFile, _convertEndian, si, si2);
        //printf("indvar: %d\n", indvar);
      }
      else
      {
        readInt(s, si,
                ptrFile, _convertEndian, si, si2);
        if (s != 4*_intLength)
        {
          printf("Warning: v3dread: binary file corrupted.\n");
          fclose(ptrFile);
          return 1;
        }  
      }
    
      // Read ndom
      E_Int ndom;
      readInt(ndom, _intLength,
              ptrFile, _convertEndian, si, si2);
      if (refdom == -1) refdom = ndom;
      E_Int ip, jp, kp;
      readInt(ip, _intLength, 
              ptrFile, _convertEndian, si, si2);
      readInt(jp, _intLength, 
              ptrFile, _convertEndian, si, si2);
      readInt(kp, _intLength, 
              ptrFile, _convertEndian, si, si2);
      //printf("%d - %d %d %d\n", ndom, ip,jp,kp);

      if (refdom == ndom && domain == 0)
      {
        if (currentVar == 0) strcpy(varString, dummy);
        else 
        {
          strcat(varString, ",");
          strcat(varString, dummy);
        }
      }
      if (ndom != refdom) nvar = K_ARRAY::getNumberOfVariables(varString);
      //printf("dimensionned nvar (%s) %d\n", varString, nvar);

      if (prevDom != ndom || n == 0)
      { 
        // nouveau domaine
        currentVar = 0;
        ni.push_back(ip);
        nj.push_back(jp);
        nk.push_back(kp);
        FldArrayF* f = new FldArrayF(ip*jp*kp, nvar);
        buf = f;
        field.push_back(f);
      }
    
      if (ndom != refdom)
      {
        currentVar = K_ARRAY::isNamePresent(dummy, varString);
        if (currentVar == -1)
        {
          printf("Warning: v3dread: the variables are different in blocks.\n");
          currentVar = 0;
        }
      }
    
      fread(&ib, si, 1, ptrFile); // Sep
      readInt(s, si,
              ptrFile, _convertEndian, si, si2);
      E_Int sizeBytes = ip*jp*kp*sizeof(E_Float);
      if (s != sizeBytes)
      {
        printf("Warning: v3dread: binary file corrupted.\n");
        fclose(ptrFile);
        return 1;
      }
    
      // read data
      fread(buf->begin(currentVar+1), sizeof(E_Float), ip*jp*kp, ptrFile);

      readInt(s, si,
              ptrFile, _convertEndian, si, si2);
      if (s != sizeBytes)
      {
        printf("Warning: v3dread: binary file corrupted.\n");
        fclose(ptrFile);
        return 1;
      }
      prevDom = ndom;
      if (ndom == refdom) currentVar++;
    }

    // Try to read the next domain
    loop = readInt(s, si,
                   ptrFile, _convertEndian, si, si2);
    domain++;
  } // data blocks

  //printf("nb domain %d\n", domain);
  // Seul le premier a ete sur-alloue
  field[0]->reAllocMat(field[0]->getSize(), nvar);

  if (_convertEndian == true)
  {
    E_Int fieldSize = field.size();
    for (E_Int i = 0; i < fieldSize; i++)
      convertEndianField(*field[i]);
  }
  fclose(ptrFile);

  // Cree les noms de zones
  E_Int fieldSize = field.size();
  for (E_Int i = 0; i < fieldSize; i++)
  {
    char* zoneName = new char [128];
    sprintf(zoneName, "zone%d", i);
    zoneNames.push_back(zoneName);
  }
  //printf("nb domain 2 %d\n", field.size());
  return 0;
}

//=============================================================================
// v3dwrite
// write v3d file, with options.
// IN: file: written file name
// IN: varString: string designating variables name.
// IN: ni, nj, nk: dimensions of zones
// IN: field: written field
// IN: isize: size of integers in file
// IN: rsize: size of real in file
// IN: convertEndian: if true, endian is changed between machine and file.
// Return 1 upon failure.
//=============================================================================
E_Int K_IO::GenIO::v3dwrite(
  char* file, char* dataFmt, char* varString, 
  vector<E_Int>& ni, vector<E_Int>& nj, vector<E_Int>& nk,
  vector<FldArrayF*>& field,
  vector<char*>& zoneNames, E_Int isize, E_Int rsize, 
  E_Boolean convertEndian)
{
  FILE* ptrFile;
  E_Int c, i, l, n;

  /* Constants */
  E_Int si = sizeof(int);
  E_Int si2 = sizeof(E_LONG);

  // Open file
  ptrFile = fopen(file, "wb");
  if (ptrFile == NULL)
  {
    printf("Warning: v3dwrite: can not open file %s\n.", file);
    return 1;
  }

  // Get number of variables
  E_Int fieldSize = field.size();
  if (fieldSize == 0) return 0; // nothing to write
  E_Int nvar = field[0]->getNfld();

  // Construction de la liste des variables
  typedef char fixStr[21];
  fixStr* vars = new fixStr[nvar];
  fixStr temp;

  if (nvar == 0) return 0; // no var to write

  // Analyse la varString pour construire une chaine par variable.
  c = 0; n = 0; l = 0;
  while (varString[c] != '\0')
  {
    if (varString[c] == ',')
    {
      vars[n][l] = '\0';
      n++; l = 0; c++;
      if (n >= nvar)
      {
        printf("Warning: v3dwrite: the number of vars in string is greater than the real number of variables in field.\n");
        goto go;
      }
    }
    else
    {
      vars[n][l] = varString[c];
      l++; c++;
    }
  }

  vars[n][l] = '\0';

  if (nvar > n+1)
  {
    for (i = n+1; i < nvar; i++)
      strcpy(vars[i], "unknown");
  }

  go:;
  // Replace CoordinateX, CoordinateY, CoordinateZ with x,y,z
  for (E_Int i = 0; i < nvar; i++)
  {
    if (strcmp(vars[i], "CoordinateX") == 0) strcpy(vars[i], "x");
    if (strcmp(vars[i], "CoordinateY") == 0) strcpy(vars[i], "y");
    if (strcmp(vars[i], "CoordinateZ") == 0) strcpy(vars[i], "z");
  }

  // Traitement des chaines: ajoute les blancs et va si necessaire
  for (i = 0; i < nvar; i++)
  {
    strcpy(temp, vars[i]);
    if (strcmp(temp, "x") != 0 && strcmp(temp, "y") != 0 
        && strcmp(temp, "z") != 0)
    {
      vars[i][0]='v'; vars[i][1]='a'; vars[i][2]=' '; vars[i][3]=' ';
      vars[i][4] = '\0'; strcat(vars[i], temp);
    }
    l = strlen(vars[i]);
    for (c = l; c < 20; c++) vars[i][c] = ' ';
    vars[i][20] = '\0';
  }

  // Sep 
  writeInt(isize, si,
           ptrFile, convertEndian, si, si2);

  // Nombre d'enregistrements
  writeInt(nvar*fieldSize, isize,
           ptrFile, convertEndian, si, si2);
  
  // Sep : int size
  writeInt(isize, si,
           ptrFile, convertEndian, si, si2);
  
  // Bloc write
  for (E_Int nb = 0; nb < fieldSize; nb++)
  {
    FldArrayF buf(*field[nb]);
    if (convertEndian == true) convertEndianField(buf);

    for (n = 0; n < nvar; n++)
    {
      // Sep : 20
      writeInt(20, si,
               ptrFile, convertEndian, si, si2);
    
      // VarName
      fwrite(vars[n], sizeof(char), 20, ptrFile);

      writeInt(20, si,
               ptrFile, convertEndian, si, si2);

      if (strcmp(vars[n], "x                   ") == 0 || 
          strcmp(vars[n], "y                   ") == 0 || 
          strcmp(vars[n], "z                   ") == 0)
      { 
        writeInt(4*isize, si,
                 ptrFile, convertEndian, si, si2);
      }
      else
      {
        writeInt(5*isize, si,
                 ptrFile, convertEndian, si, si2);
        writeInt(n+1, isize,
                 ptrFile, convertEndian, si, si2);
      }

      // ndom
      writeInt(nb+1, isize,
               ptrFile, convertEndian, si, si2);
  
      // ip, jp, kp
      writeInt(ni[nb], isize,
               ptrFile, convertEndian, si, si2);
      writeInt(nj[nb], isize,
               ptrFile, convertEndian, si, si2);
      writeInt(nk[nb], isize,
               ptrFile, convertEndian, si, si2);

      // Sep
      writeInt(40, si,
               ptrFile, convertEndian, si, si2);
      writeInt(ni[nb]*nj[nb]*nk[nb]*sizeof(E_Float), si,
               ptrFile, convertEndian, si, si2);
      fwrite(buf.begin(n+1), sizeof(E_Float), ni[nb]*nj[nb]*nk[nb], ptrFile);
      writeInt(ni[nb]*nj[nb]*nk[nb]*sizeof(E_Float), si,
               ptrFile, convertEndian, si, si2);
    }
  }
  
  delete [] vars;
  fclose(ptrFile);
  return 0;

}