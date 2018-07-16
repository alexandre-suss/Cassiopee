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


//#define FLAG_STEP

# include <string>
# include <sstream> 
# include "intersector.h"
# include "Fld/ngon_t.hxx"
# include "Nuga/Delaunay/Triangulator.h"
# include "Nuga/Boolean/Splitter.h"

#include "Nuga/include/tree.hxx"
#include "Nuga/include/geom_sensor.hxx"
#include "Nuga/include/adaptor.hxx"
#include "Nuga/include/hierarchical_mesh.hxx"

#include "Search/BbTree.h"

//#include <iostream>

using namespace std;
using namespace NUGA;

#ifdef FLAG_STEP
E_Int chrono::verbose = 0;
#endif


//=============================================================================
/* Agglomerate superfuous faces (overdefined polyhedra) */
//=============================================================================
PyObject* K_INTERSECTOR::splitNonStarCells(PyObject* self, PyObject* args)
{
  PyObject *arr(0);
  E_Float PH_conc_threshold(1./3.);
  E_Float PH_cvx_threshold(0.05);
  E_Float PG_cvx_threshold(1.e-8);

  if (!PYPARSETUPLEF(args, "Oddd", "Offf", &arr, &PH_conc_threshold, &PH_cvx_threshold, &PG_cvx_threshold)) return NULL;

  K_FLD::FloatArray* f(0);
  K_FLD::IntArray* cn(0);
  char* varString, *eltType;
  // Check array # 1
  E_Int err = check_is_NGON(arr, f, cn, varString, eltType);
  if (err) return NULL;
    
  K_FLD::FloatArray & crd = *f;
  K_FLD::IntArray & cnt = *cn;
  
  //~ std::cout << "crd : " << crd.cols() << "/" << crd.rows() << std::endl;
  //~ std::cout << "cnt : " << cnt.cols() << "/" << cnt.rows() << std::endl;
  
  typedef ngon_t<K_FLD::IntArray> ngon_type;
  ngon_type ngi(cnt), ngo;
  
  NUGA::Splitter::split_non_star_phs<DELAUNAY::Triangulator>(crd, ngi, PH_conc_threshold, PH_cvx_threshold, PG_cvx_threshold, ngo);

  K_FLD::IntArray cnto;
  ngo.export_to_array(cnto);
  
  PyObject* tpl = K_ARRAY::buildArray(crd, varString, cnto, -1, eltType, false);;

  delete f; delete cn;
  return tpl;
}

//=============================================================================
/* Agglomerate superfuous faces (overdefined polyhedra) */
//=============================================================================
PyObject* K_INTERSECTOR::adaptCells(PyObject* self, PyObject* args)
{
  PyObject *arr(nullptr), *arrS(nullptr);
  

  if (!PyArg_ParseTuple(args, "OO", &arr, &arrS)) return NULL;

  K_FLD::FloatArray* f(nullptr), *fS(nullptr);
  K_FLD::IntArray* cn(nullptr), *cnS(nullptr);
  char* varString, *eltType, *varString2, *eltType2;
  // Check the mesh (NGON)
  E_Int err = check_is_NGON(arr, f, cn, varString, eltType);
  if (err) return NULL;
    
  K_FLD::FloatArray & crd = *f;
  K_FLD::IntArray & cnt = *cn;

  E_Int ni, nj, nk;
  E_Int res2 = K_ARRAY::getFromArray(arrS, varString2, fS, ni, nj, nk,
                                     cnS, eltType2);

  K_FLD::FloatArray & crdS = *fS;
  
  //~ std::cout << "crd : " << crd.cols() << "/" << crd.rows() << std::endl;
  //~ std::cout << "cnt : " << cnt.cols() << "/" << cnt.rows() << std::endl;
  
  typedef ngon_t<K_FLD::IntArray> ngon_type;
  ngon_type ngi(cnt);

  using mesh_type = NUGA::hierarchical_mesh<K_MESH::Hexahedron>;
  using sensor_type = NUGA::geom_sensor/*geom_static_sensor*/<mesh_type>;
  
  mesh_type hmesh(crd, ngi);
  sensor_type sensor(hmesh);
  
  NUGA::adaptor<mesh_type, sensor_type>::run(hmesh, sensor, crdS);
  
  ngon_type ngo;
  hmesh.filter_ngon(ngo);

  K_FLD::IntArray cnto;
  ngo.export_to_array(cnto);
  
  PyObject* tpl = K_ARRAY::buildArray(hmesh._crd, varString, cnto, -1, "NGON", false);;

  delete f; delete cn;
  delete fS; delete cnS;
  return tpl;
}

//=============================================================================
/* todo */
//=============================================================================
PyObject* K_INTERSECTOR::adaptBox(PyObject* self, PyObject* args)
{
  PyObject *arrS(nullptr);
  E_Float bratio(10.);
  

  if (!PYPARSETUPLEF(args, "Od", "Of", &arrS, &bratio)) return NULL;

  if (bratio < 1.)bratio = 1.;

  //std::cout << "in K_INTERSECTOR::adaptBox" << std::endl;


  K_FLD::FloatArray* f(nullptr);
  K_FLD::IntArray* cn(nullptr);
  char* varString, *eltType;

  E_Int ni, nj, nk;
  E_Int res2 = K_ARRAY::getFromArray(arrS, varString, f, ni, nj, nk,
                                     cn, eltType);

  K_FLD::FloatArray & crdS = *f;
  //std::cout << "crd : " << crdS.cols() << "/" << crdS.rows() << std::endl;

  //std::cout << "compute box..." << std::endl;

  
  // Create the box
  K_SEARCH::BBox3D box;
  K_FLD::ArrayAccessor<K_FLD::FloatArray> acrd(crdS);
  box.compute(acrd);

  // Box center and deltas
  E_Float C[3], Delta[3], DeltaMax=0.;
  for (int i=0; i < 3; ++i){
  	C[i] = 0.5*(box.minB[i] + box.maxB[i]);
  	Delta[i] = box.maxB[i] - box.minB[i];
  	DeltaMax = std::max(DeltaMax, Delta[i]);
  }
  // Enlarge it 
  for (int i=0; i < 3; ++i)
  {
  	box.minB[i] = C[i] - (bratio*DeltaMax/*Delta[i]*/);
  	box.maxB[i] = C[i] + (bratio*DeltaMax/*Delta[i]*/);
  }

  //std::cout << "convert box..." << std::endl;

  typedef ngon_t<K_FLD::IntArray> ngon_type;
  ngon_type ngi;
  K_FLD::FloatArray crd;
  K_FLD::IntArray cnt;
  box.convert2NG<ngon_type>(crd, ngi);

  //std::cout << "adapt box..." << std::endl;

  using mesh_type = NUGA::hierarchical_mesh<K_MESH::Hexahedron>;
  using sensor_type = NUGA::geom_sensor/*geom_static_sensor*/<mesh_type>;
  
  mesh_type hmesh(crd, ngi);
  sensor_type sensor(hmesh);
  
  NUGA::adaptor<mesh_type, sensor_type>::run(hmesh, sensor, crdS);

  std::cout << "output leaves..." << std::endl;
  
  ngon_type ngo;
  hmesh.filter_ngon(ngo);

  K_FLD::IntArray cnto;
  ngo.export_to_array(cnto);

  //std::cout << "output ..." << std::endl;
  
  PyObject* tpl = K_ARRAY::buildArray(hmesh._crd, varString, cnto, -1, "NGON", false);

  delete f; delete cn;
  return tpl;
}


//=======================  Intersector/PolyMeshTools/splitCells.cpp ====================