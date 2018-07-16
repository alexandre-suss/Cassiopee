/*
 
 
 
              NUGA 
 
 
 
 */
//Authors : Sâm Landier (sam.landier@onera.fr), Alexis Gay (alexis.gay@onera.fr)

#ifndef NUGA_HIERACHICAL_MESH_HXX
#define NUGA_HIERACHICAL_MESH_HXX

#if defined (DEBUG_HIERARCHICAL_MESH) || (OUTPUT_ITER_MESH)
#include "IO/io.h"
#include "Nuga/Boolean/NGON_debug.h"
using NGDBG = NGON_debug<K_FLD::FloatArray,K_FLD::IntArray>;
#endif

#include "Nuga/include/tree.hxx"
#include "Nuga/include/q9.hxx"
#include "Nuga/include/h27.hxx"
#include "Connect/IdTool.h"
#include "Nuga/Delaunay/Triangulator.h"



namespace NUGA
{

template <typename ELT_t, typename ngo_t = ngon_type, typename crd_t = K_FLD::FloatArray>
class hierarchical_mesh
{
  
  public:
  using elt_type = ELT_t;
  
  crd_t&                    _crd;
  ngo_t&                    _ng;
  tree<ELT_t>               _tree;
  K_FLD::IntArray           _F2E;
  bool                      _initialized;

  hierarchical_mesh(crd_t& crd, ngo_t & ng):_crd(crd), _ng(ng), _tree(ng), _initialized(false){}

  E_Int init();
  
  E_Int adapt(Vector_t<E_Int>& adap_incr);
  
  void refine_PGs(const Vector_t<E_Int> &PHadap);
  
  void refine_PHs(const Vector_t<E_Int> &PHadap);
  
  void get_nodes_PHi(E_Int* nodes, E_Int PHi, E_Int* BOT, E_Int* TOP, E_Int* LEFT, E_Int* RIGHT, E_Int* FRONT, E_Int* BACK);
  
  void retrieve_ordered_data(E_Int PGi, E_Int i0, bool reorient, E_Int* four_childrenPG, E_Int* LNODES);
  
  bool need_a_reorient(E_Int PGi, E_Int PHi, bool oriented_if_R);
  
  E_Int get_i0(E_Int* pFace, E_Int common_node, E_Int* nodes, E_Int nb_edges_face);
  
  void filter_ngon(ngon_type& filtered_ng);
  
  void update_F2E(E_Int PHi, E_Int PHchildr0, E_Int* INT, E_Int* BOT, E_Int* TOP, E_Int* LEFT, E_Int* RIGHT, E_Int* FRONT, E_Int* BACK);
  
  void update_children_F2E(E_Int PGi, E_Int side);
  
  void get_cell_center(E_Int PGi, E_Float* center);
  
  private:
    std::map<K_MESH::NO_Edge,E_Int> _ecenter;
    Vector_t<E_Int>                 _fcenter;
    
    void __compute_edge_center(const Vector_t<E_Int> &PGlist, K_FLD::FloatArray& crd, std::map<K_MESH::NO_Edge,E_Int> & ecenter);
    void __compute_face_centers(K_FLD::FloatArray& crd, const typename ngo_t::unit_type& pgs, const Vector_t<E_Int> &PGlist, Vector_t<E_Int>& fcenter);
    inline void __compute_face_center(const crd_t& crd, const E_Int* nodes, E_Int nb_nodes, E_Float* C);
    void __compute_cell_center(const E_Int* nodes27, E_Float* C);
  
};

template <typename ELT_t, typename ngo_t, typename crd_t>
E_Int hierarchical_mesh<ELT_t, ngo_t, crd_t>::init()
{
  if (_initialized) return 0;
  
  E_Int err(0);
  
  // We reorient the PG of our NGON
  _ng.flag_externals(1);
  DELAUNAY::Triangulator dt;
  bool has_been_reversed;
  err = ngon_type::reorient_skins(dt, _crd, _ng, has_been_reversed);
  if (err)
    return 1;
  
  //F2E
  ngon_unit neighbors;
  _ng.build_ph_neighborhood(neighbors);
  _ng.build_F2E(neighbors, _F2E);
  
  // sort the PGs of the initial NGON
  for (int i = 0; i < (int)_ng.PHs.size(); ++i)
  {
      ELT_t::reorder_pgs(_ng,_F2E,i);
  }
  
  _initialized = true;
  
  return err;
}

template <typename ELT_t, typename ngo_t, typename crd_t>
E_Int hierarchical_mesh<ELT_t, ngo_t, crd_t>::adapt(Vector_t<E_Int>& adap_incr)
{
  E_Int err(0);

  Vector_t<E_Int> PHadap, PHref, PGref;

#ifdef OUTPUT_ITER_MESH
  E_Int iter = 1;
#endif
  
  // identify the sensor
  E_Int max = *std::max_element(ALL(adap_incr));
  E_Int min = *std::min_element(ALL(adap_incr));
  E_Bool loop = true;
  
  if ( (abs(min) <= 1) && (abs(max) <= 1) )
      loop = false;
  
  while (!err)
  {
    PHadap.clear();
    
    // PHadap : Get the list of enabled PHs for which adap_incr[PHi] != 0
    for (size_t i = 0; i < _ng.PHs.size(); ++i)
      if (adap_incr[i] != 0 && _tree._PHtree.is_enabled(i)) PHadap.push_back(i);

    if (PHadap.empty()) break; // adapted

    // sort the PHadap list in order to solve starting by lower level of refinement
    std::sort(PHadap.begin(),PHadap.end(),[this](int a, int b){
        return _tree._PHtree.get_level(a) < _tree._PHtree.get_level(b);
    });

    // refine PGs : create missing children (those PGi with _tree._PGtree.nb_children(PGi) == 0)
    refine_PGs(PHadap);

    // refine PHs with missing children (those PHi with _tree._PHtree.nb_children(PHi) == 0)
    refine_PHs(PHadap);


    // fixme : enabling the appropriate PHs
    adap_incr.resize(_ng.PHs.size(),0);
    for (size_t i = 0; i < PHadap.size(); ++i)
    {
      E_Int PHi = PHadap[i];

      if (adap_incr[PHi] > 0) // activate the children
      {
        E_Int lvl_p1 = _tree._PHtree.get_level(PHi)+ 1;
        E_Int nb_child = _tree._PHtree.nb_children(PHi);
        const E_Int* children = _tree._PHtree.children(PHi);
        for (E_Int j = 0; j < nb_child; ++j)
        {
            _tree._PHtree.enable(*(children+j));
            adap_incr[*(children+j)] = adap_incr[PHi]-1;
            _tree._PHtree.set_level(*(children+j), lvl_p1);
        }
      }
      adap_incr[PHi] = 0;
    }
    _ng.PGs.updateFacets();
    _ng.PHs.updateFacets();

#ifdef DEBUG_HIERARCHICAL_MESH  
    //if (! _ng.attributes_are_consistent()) return false;
#endif
    
#ifdef OUTPUT_ITER_MESH
    ngon_type filtered_ng;
    filter_ngon(filtered_ng); 
    
    std::ostringstream o;
    o << "NGON_it_" << iter << ".plt"; // we create a file at each iteration
    K_FLD::IntArray cnto;
    filtered_ng.export_to_array(cnto);
    MIO::write(o.str().c_str(), _crd, cnto, "NGON");     
    
    ++iter;
#endif
    
    if (!loop) break;
  }

  return err;
}

///
template <>
void hierarchical_mesh<K_MESH::Hexahedron, ngon_type, K_FLD::FloatArray>::__compute_cell_center
(const E_Int* nodes27, E_Float* centroid)
{
  E_Int new_bary[8];
  new_bary[0] = nodes27[0];
  new_bary[1] = nodes27[1];
  new_bary[2] = nodes27[2];
  new_bary[3] = nodes27[3];
  new_bary[4] = nodes27[4];
  new_bary[5] = nodes27[5];
  new_bary[6] = nodes27[6];
  new_bary[7] = nodes27[7];

  K_MESH::Polyhedron<STAR_SHAPED>::iso_barycenter(_crd, new_bary, 8, 1,centroid);

  //  E_Int new_pg[4];
  //  new_pg[0] = nodes27[0];
  //  new_pg[1] = nodes27[1];
  //  new_pg[2] = nodes27[6];
  //  new_pg[3] = nodes27[7];
  //  K_MESH::Polygon::iso_barycenter<acrd_t,3>(acrd,new_pg,4,1,centroid);
}

///
template <>
void hierarchical_mesh<K_MESH::Hexahedron, ngon_type, K_FLD::FloatArray>::refine_PGs(const Vector_t<E_Int> &PHadap)
{
  
  E_Int nb_phs = PHadap.size();
  
  // Gets PGs to refine
  Vector_t<E_Int> PGref;
  {
    E_Int nb_pgs(_ng.PGs.size()), nb_pgs_ref(0);
    Vector_t<bool> is_PG_to_refine(nb_pgs, false);
    //
    for (E_Int i = 0; i < nb_phs; ++i)
    {
      E_Int PHi = PHadap[i];

      E_Int nb_faces = _ng.PHs.stride(PHi); 
      E_Int* faces = _ng.PHs.get_facets_ptr(PHi);
      
#ifdef DEBUG_HIERARCHICAL_MESH
      assert (nb_faces == 6);
#endif
      for (E_Int j = 0; j < nb_faces; ++j)
      {
        E_Int PGi = * (faces + j) - 1;
        
        if (_tree._PGtree.nb_children(PGi) == 0) // leaf PG => to refine
        {
          is_PG_to_refine[PGi] = true;
          ++nb_pgs_ref;
        }
      }
    }

    PGref.reserve(nb_pgs_ref);
    for (E_Int i = 0; i < nb_pgs; ++i)
    {
      if (is_PG_to_refine[i])
        PGref.push_back(i);
    }
  }

  E_Int nb_pgs_ref = PGref.size();
  // Compute Edges refine points
  __compute_edge_center(PGref, _crd, _ecenter);
  
  // Compute Faces centroids
  _fcenter.clear();
  __compute_face_centers(_crd, _ng.PGs, PGref, _fcenter);
  
  // Reserve space for children in the tree
  _tree.resize(_tree._PGtree, PGref);
  // And in the mesh : each Q4 is split into 4 Q4
  E_Int nb_pgs0 = _ng.PGs.size();
  _ng.PGs.expand_n_fixed_stride(4*nb_pgs_ref, 4/*Q4 stride*/);
  _F2E.resize(2,nb_pgs0+4*nb_pgs_ref+12*nb_phs,E_IDX_NONE);

#ifdef DEBUG_HIERARCHICAL_MESH
  Vector_t<E_Int> ids;
#endif
  
  K_MESH::NO_Edge noE;  
  
  // Refine them
  for (E_Int i = 0; i < nb_pgs_ref; ++i)
  {
    E_Int PGi = PGref[i];
    E_Int PGichildr[4];
    E_Int q9[9];
   
    E_Int* nodes = _ng.PGs.get_facets_ptr(PGi);
    
#ifdef DEBUG_HIERARCHICAL_MESH
    assert (_ng.PGs.stride(PGi) == 4);
#endif
    
    for (E_Int n=0; n < 4; ++n)
    {
        
        PGichildr[n] = nb_pgs0 + 4*i + n;
        // children have the L & R elements of the father
        _F2E(0,PGichildr[n]) = _F2E(0,PGi);
        _F2E(1,PGichildr[n]) = _F2E(1,PGi);
        
        q9[n] = *(nodes + n);
        noE.setNodes(*(nodes + n), *(nodes + (n+1)%4));
        q9[n+4] = _ecenter[noE];
    }
    
    q9[8] = _fcenter[i] + 1;
    
    // set them in _ng.PGs
    E_Int* q41 = _ng.PGs.get_facets_ptr(PGichildr[0]);
    E_Int* q42 = _ng.PGs.get_facets_ptr(PGichildr[1]);
    E_Int* q43 = _ng.PGs.get_facets_ptr(PGichildr[2]);
    E_Int* q44 = _ng.PGs.get_facets_ptr(PGichildr[3]);
    
    NUGA::Q9::splitQ4(_crd, q9, q41, q42, q43, q44);
    
#ifdef DEBUG_HIERARCHICAL_MESH
//    ngon_unit pgs;
//    pgs.add(4,q41);
//    pgs.add(4,q42);
//    pgs.add(4,q43);
//    pgs.add(4,q44);
//    NGDBG::draw_PGT3s(_crd,pgs);
#endif   

#ifdef DEBUG_HIERARCHICAL_MESH
//    std::cout << "q41 nb nodes : " << _ng.PGs.stride(PGichildr[0]) << std::endl;
//    for (size_t i=0; i < 9; ++i)
//      std::cout << q9[i] << "/";
//    std::cout << std::endl;
//    
//    std::cout << q41[0] << "/" << q41[1] << "/" << q41[2] << "/" << q41[3] << std::endl;
#endif

    // set them in the tree
    _tree._PGtree.set_children(PGi, PGichildr, 4);
   
#ifdef DEBUG_HIERARCHICAL_MESH
    ids.insert(ids.end(), PGichildr, PGichildr+4);
#endif
    
  }
  
#ifdef DEBUG_HIERARCHICAL_MESH
  //NGDBG::draw_PGs(_crd, _ng.PGs, ids, false);
#endif

}

///
template <>
void hierarchical_mesh<K_MESH::Hexahedron, ngon_type, K_FLD::FloatArray>::retrieve_ordered_data(E_Int PGi, E_Int i0, bool reorient, E_Int* four_childrenPG, E_Int* LNODES)
{
    E_Int* pN = _ng.PGs.get_facets_ptr(PGi);
    E_Int nb_edges = _ng.PGs.stride(PGi);
    
    for (int i = 0; i < nb_edges; i++)
    {
        LNODES[i] = pN[i];
        four_childrenPG[i] = *(_tree._PGtree.children(PGi)+i); // numéros des PG fils
    }

    E_Int* pNFils0 = _ng.PGs.get_facets_ptr(four_childrenPG[0]);
    E_Int* pNFils2 = _ng.PGs.get_facets_ptr(four_childrenPG[2]);
    

#ifdef DEBUG_HIERARCHICAL_MESH    
    E_Int* pNFils1 = _ng.PGs.get_facets_ptr(four_childrenPG[1]);
    E_Int* pNFils3 = _ng.PGs.get_facets_ptr(four_childrenPG[3]);

    assert(pNFils0[2] == pNFils2[0]);
    assert(pNFils1[0] == pNFils0[1]);
    assert(pNFils1[2] == pNFils2[1]);
    assert(pNFils2[3] == pNFils3[2]);
    assert(pNFils0[3] == pNFils3[0]);
#endif

    // we got the 4 to 8 thanks to child 0 and child 2 (never swapped)
    LNODES[4] = pNFils0[1];
    LNODES[5] = pNFils2[1];
    LNODES[6] = pNFils2[3];
    LNODES[7] = pNFils0[3];
    LNODES[8] = pNFils0[2]; // centroid
    
    
    K_CONNECT::IdTool::right_shift<4>(&LNODES[0],i0);
    K_CONNECT::IdTool::right_shift<4>(&LNODES[4],i0);     
    K_CONNECT::IdTool::right_shift<4>(&four_childrenPG[0],i0);
    
    if (reorient == true)
    {
        std::reverse(&LNODES[1], &LNODES[1] + 3 );
        std::reverse(&LNODES[4], &LNODES[4] + 4 );
        std::swap(four_childrenPG[1],four_childrenPG[3]);
    }
    
}

///
template <typename ELT_t, typename ngo_t, typename crd_t>
bool hierarchical_mesh<ELT_t, ngo_t, crd_t>::need_a_reorient(E_Int PGi, E_Int PHi, bool oriented_if_R)
{
    if (_F2E(1,PGi) == PHi && oriented_if_R == true) return false;
    else if (_F2E(0,PGi) == PHi && oriented_if_R == false) return false;
    else return true;
}

///
template <typename ELT_t, typename ngo_t, typename crd_t>
E_Int hierarchical_mesh<ELT_t, ngo_t, crd_t>::get_i0(E_Int* pFace, E_Int common_node, E_Int* nodes, E_Int nb_edges_face)
{
    for (int i = 0; i < nb_edges_face; i++)
       if (pFace[i] == nodes[common_node]) return i; 
    return -1;
}

///
template <typename ELT_t, typename ngo_t, typename crd_t>
void hierarchical_mesh<ELT_t, ngo_t, crd_t>::filter_ngon(ngon_type& filtered_ng)
{
    filtered_ng.PGs = _ng.PGs;
        
    //E_Int sz = _tree._PGtree.get_parent_size();
    E_Int sz = _ng.PHs.size();
        
    for (int i = 0; i < sz; i++)
    {
      if (_tree._PHtree.is_enabled(i) == true)
      {
        E_Int* p = _ng.PHs.get_facets_ptr(i);
        filtered_ng.PHs.add(6,p);
      }
    }
    
    filtered_ng.PGs.updateFacets();
    filtered_ng.PHs.updateFacets();
    
    
    //Vector_t<E_Int> pgnids, phnids;
    // fixme filtered_ng.remove_unreferenced_pgs(pgnids, phnids);
}

///
template <>
void hierarchical_mesh<K_MESH::Hexahedron, ngon_type, K_FLD::FloatArray>::get_nodes_PHi(E_Int* nodes, E_Int PHi, E_Int* BOT, E_Int* TOP, E_Int* LEFT, E_Int* RIGHT, E_Int* FRONT, E_Int* BACK)
{
  E_Int* pPGi = _ng.PHs.get_facets_ptr(PHi);
  E_Int PGi = pPGi[0] - 1;
  E_Int* pN = _ng.PGs.get_facets_ptr(PGi);

  nodes[0] = *pN; // 0 -> PHi(0,0)

  if (_F2E(1,PGi) == PHi) // for BOT, PH is the right element : well oriented
    for (int k = 1; k < 4; k++) nodes[k] =*(pN+k);

  else // otherwise : wrong orientation (swap 1 & 3)
  { 
    nodes[1] = *(pN+3);
    nodes[2] = *(pN+2);
    nodes[3] = *(pN+1);
  }
  // let's fill in nodes
  E_Int tmp[9];
  bool reorient;

  // BOT
  E_Int i0 = 0;

  reorient = need_a_reorient(pPGi[0]-1,PHi,true);
  retrieve_ordered_data(pPGi[0]-1,i0,reorient,BOT,tmp);

  nodes[8] = tmp[4];
  nodes[9] = tmp[5];
  nodes[10] = tmp[6];
  nodes[11] = tmp[7];
  nodes[12] = tmp[8];

#ifdef DEBUG_HIERARCHICAL_MESH
  assert(tmp[0] == nodes[0]);
  assert(tmp[1] == nodes[1]);
  assert(tmp[2] == nodes[2]);
  assert(tmp[3] == nodes[3]);
#endif

  // LEFT 
  E_Int* p = _ng.PGs.get_facets_ptr(pPGi[2]-1);
  i0 = get_i0(p,0,nodes,4);// common point : nodes[0]

  reorient = need_a_reorient(pPGi[2]-1,PHi,true);
  retrieve_ordered_data(pPGi[2]-1,i0,reorient,LEFT,tmp);

  nodes[21] = tmp[5];
  nodes[7] = tmp[2];
  nodes[16] = tmp[6];
  nodes[4] = tmp[3];
  nodes[18] = tmp[7];
  nodes[25] = tmp[8];

#ifdef DEBUG_HIERARCHICAL_MESH
  assert(tmp[0] == nodes[0]);
  assert(tmp[1] == nodes[3]);
  assert(tmp[4] == nodes[11]);
#endif

  // RIGHT
  p = _ng.PGs.get_facets_ptr(pPGi[3]-1);
  i0 = get_i0(p,1,nodes,4);

  reorient = need_a_reorient(pPGi[3]-1,PHi,false);
  retrieve_ordered_data(pPGi[3]-1,i0,reorient,RIGHT,tmp);

  nodes[20] = tmp[5];
  nodes[6] = tmp[2];
  nodes[14] = tmp[6];
  nodes[5] = tmp[3];
  nodes[19] = tmp[7];
  nodes[23] = tmp[8];

#ifdef DEBUG_HIERARCHICAL_MESH
  assert(tmp[0] == nodes[1]);
  assert(tmp[1] == nodes[2]);
  assert(tmp[4] == nodes[9]);
#endif

  // TOP
  p = _ng.PGs.get_facets_ptr(pPGi[1]-1);
  i0 = get_i0(p,4,nodes,4);

  reorient = need_a_reorient(pPGi[1]-1,PHi,false);
  retrieve_ordered_data(pPGi[1]-1,i0,reorient,TOP,tmp);

  nodes[13] = tmp[4];
  nodes[15] = tmp[6];
  nodes[17] = tmp[8];

#ifdef DEBUG_HIERARCHICAL_MESH
  assert(tmp[0] == nodes[4]);
  assert(tmp[1] == nodes[5]);
  assert(tmp[2] == nodes[6]);
  assert(tmp[3] == nodes[7]);
  assert(tmp[5] == nodes[14]);
  assert(tmp[7] == nodes[16]);
#endif

  // FRONT
  p = _ng.PGs.get_facets_ptr(pPGi[4]-1);
  i0 = get_i0(p,1,nodes,4);

  reorient = need_a_reorient(pPGi[4]-1,PHi,true);
  retrieve_ordered_data(pPGi[4]-1,i0,reorient,FRONT,tmp);

  nodes[22] = tmp[8];

#ifdef DEBUG_HIERARCHICAL_MESH
  assert(tmp[0] == nodes[1]);
  assert(tmp[1] == nodes[0]);
  assert(tmp[2] == nodes[4]);
  assert(tmp[3] == nodes[5]);
  assert(tmp[4] == nodes[8]);
  assert(tmp[5] == nodes[18]);
  assert(tmp[6] == nodes[13]);
  assert(tmp[7] == nodes[19]);
#endif

  // BACK
  p = _ng.PGs.get_facets_ptr(pPGi[5]-1);
  i0 = get_i0(p,2,nodes,4);

  reorient = need_a_reorient(pPGi[5]-1,PHi,false);
  retrieve_ordered_data(pPGi[5]-1,i0,reorient,BACK,tmp);

  nodes[24] = tmp[8];

#ifdef DEBUG_HIERARCHICAL_MESH
  assert(tmp[0] == nodes[2]);
  assert(tmp[1] == nodes[3]);
  assert(tmp[2] == nodes[7]);
  assert(tmp[3] == nodes[6]);
  assert(tmp[4] == nodes[10]);
  assert(tmp[5] == nodes[21]);
  assert(tmp[6] == nodes[15]);
  assert(tmp[7] == nodes[20]);
#endif

  // Calcul du centroid 

 E_Float centroid[3];
 __compute_cell_center(nodes, centroid);

  E_Int nb_cols = _crd.cols();

  _crd.pushBack(centroid, centroid+3);

  nodes[26] = nb_cols+1;

#ifdef DEBUG_HIERARCHICAL_MESH
//        for (int i = 0 ; i < 27; i++){
//            for (int j = 0; j < 27; j++){
//                if (nodes[i] == nodes[j] && i < j )
//                    std::cout << std::endl << " couple " << "      (" << i << " , " << j << ")"; 
//            }
//        }
//        std::cout << std::endl;
#endif
}

///
template <typename ELT_t, typename ngo_t, typename crd_t>
void hierarchical_mesh<ELT_t, ngo_t, crd_t>::update_children_F2E(E_Int PGi, E_Int side)
{
  if (_tree._PGtree.children(PGi) != nullptr)
  {
    E_Int* p = _tree._PGtree.children(PGi);
    E_Int nb_children = _tree._PGtree.nb_children(PGi);
    for (int j = 0; j < nb_children; ++j)
      _F2E(side,p[j]) = _F2E(side,PGi);
  }
}

///
template <>
void hierarchical_mesh<K_MESH::Hexahedron, ngon_type, K_FLD::FloatArray>::update_F2E(E_Int PHi, E_Int PHchildr0, E_Int* INT, E_Int* BOT, E_Int* TOP, E_Int* LEFT, E_Int* RIGHT, E_Int* FRONT, E_Int* BACK)
{
  E_Int elt0 = PHchildr0;
  E_Int* pPGi = _ng.PHs.get_facets_ptr(PHi);

  for (int i = 0; i < 4; i++)
  {
    E_Int sid = (_F2E(1,pPGi[0]-1) == PHi) ? 1 : 0;
    _F2E(sid,BOT[i]) = elt0+i;
    update_children_F2E(BOT[i],sid);

    sid = (_F2E(0,pPGi[1]-1) == PHi) ? 0 : 1;
    _F2E(sid,TOP[i]) = elt0+4+i;
    update_children_F2E(TOP[i],sid);
  }

  E_Int sid = (_F2E(1,pPGi[2]-1) == PHi) ? 1: 0;
  _F2E(sid,LEFT[0]) = elt0;
  _F2E(sid,LEFT[1]) = elt0+3;
  _F2E(sid,LEFT[2]) = elt0+7;
  _F2E(sid,LEFT[3]) = elt0+4;
  for (int i = 0; i < 4; ++i)
    update_children_F2E(LEFT[i], sid);

  sid = (_F2E(0,pPGi[3]-1) == PHi) ? 0 : 1;
  _F2E(sid,RIGHT[0]) = elt0+1;
  _F2E(sid,RIGHT[1]) = elt0+2;
  _F2E(sid,RIGHT[2]) = elt0+6;
  _F2E(sid,RIGHT[3]) = elt0+5;
  for (int i = 0; i < 4; ++i)
    update_children_F2E(RIGHT[i],sid);

  sid = (_F2E(1,pPGi[4]-1) == PHi) ? 1: 0;
  _F2E(sid,FRONT[0]) = elt0+1;
  _F2E(sid,FRONT[1]) = elt0;
  _F2E(sid,FRONT[2]) = elt0+4;
  _F2E(sid,FRONT[3]) = elt0+5;
  for (int i = 0; i < 4; ++i)
    update_children_F2E(FRONT[i],sid);

  sid = (_F2E(0,pPGi[5]-1) == PHi) ? 0 : 1;
  _F2E(sid,BACK[0]) = elt0+2;
  _F2E(sid,BACK[1]) = elt0+3;
  _F2E(sid,BACK[2]) = elt0+7;
  _F2E(sid,BACK[3]) = elt0+6;
  for (int i = 0; i < 4; ++i)
    update_children_F2E(BACK[i],sid);            

  _F2E(0,INT[0]) = elt0+1;
  _F2E(1,INT[0]) = elt0+2;

  _F2E(0,INT[1]) = elt0;
  _F2E(1,INT[1]) = elt0+3;

  _F2E(0,INT[2]) = elt0+4;
  _F2E(1,INT[2]) = elt0+7;

  _F2E(0,INT[3]) = elt0+5;
  _F2E(1,INT[3]) = elt0+6;

  _F2E(0,INT[4]) = elt0;
  _F2E(1,INT[4]) = elt0+4;

  _F2E(0,INT[5]) = elt0+1;
  _F2E(1,INT[5]) = elt0+5;

  _F2E(0,INT[6]) = elt0+2;
  _F2E(1,INT[6]) = elt0+6;

  _F2E(0,INT[7]) = elt0+3;
  _F2E(1,INT[7]) = elt0+7;

  _F2E(0,INT[8]) = elt0;
  _F2E(1,INT[8]) = elt0+1;

  _F2E(0,INT[9]) = elt0+3;
  _F2E(1,INT[9]) = elt0+2;

  _F2E(0,INT[10]) = elt0+7;
  _F2E(1,INT[10]) = elt0+6;

  _F2E(0,INT[11]) = elt0+4;
  _F2E(1,INT[11]) = elt0+5;
}

///
template <>
void hierarchical_mesh<K_MESH::Hexahedron, ngon_type, K_FLD::FloatArray>::refine_PHs(const Vector_t<E_Int> &PHadap)
{
  E_Int nb_phs = PHadap.size();
  // internal PGs created (12 per PH)
  // Reserve space for internal faces in the tree
  E_Int current_sz = _tree._PGtree.get_parent_size();

  _tree._PGtree.resize_hierarchy(current_sz+12*nb_phs);

  E_Int nb_pgs0 = _ng.PGs.size();

  // And in the mesh : each H27 is split in 36 Pgs including 12 new internal Q4

  _ng.PGs.expand_n_fixed_stride(12*nb_phs, 4/*Q4 stride*/);

  // for the children PH
  // Reserve space for children in the tree
  _tree.resize(_tree._PHtree, PHadap);
  // And in the mesh : each H27 is split into 8 H27
  E_Int nb_phs0 = _ng.PHs.size();
  _ng.PHs.expand_n_fixed_stride(8*nb_phs, 6/*H27 stride*/);
    
  for (E_Int i = 0; i < nb_phs; ++i)
  {
    E_Int PHi = PHadap[i];
    E_Int nodes[27]; // 0-26 to crd1 indexes
    E_Int BOT[4],TOP[4],LEFT[4],RIGHT[4],FRONT[4],BACK[4];// children lists

    get_nodes_PHi(nodes,PHi,BOT,TOP,LEFT,RIGHT,FRONT,BACK);

    E_Int points[27];
    K_MESH::Hexahedron::get_internal(nodes,points);

    E_Int PGichildr[4];
    E_Int INT[12];

    for (int j = 0; j < 3; ++j)
    {
      PGichildr[0] = nb_pgs0 + 12*i + 4*j;
      PGichildr[1] = nb_pgs0 + 12*i + 4*j + 1;
      PGichildr[2] = nb_pgs0 + 12*i + 4*j + 2;
      PGichildr[3] = nb_pgs0 + 12*i + 4*j + 3;

      E_Int* q41 = _ng.PGs.get_facets_ptr(PGichildr[0]);
      E_Int* q42 = _ng.PGs.get_facets_ptr(PGichildr[1]);
      E_Int* q43 = _ng.PGs.get_facets_ptr(PGichildr[2]);
      E_Int* q44 = _ng.PGs.get_facets_ptr(PGichildr[3]);

      NUGA::Q9::splitQ4(_crd, &points[9*j], q41, q42, q43, q44);

      INT[4*j] = PGichildr[0];
      INT[4*j+1] = PGichildr[1];
      INT[4*j+2] = PGichildr[2];
      INT[4*j+3] = PGichildr[3];
    }

    // créer les 8 nouveaux éléments
    E_Int PHichildr[8];

    for (int j = 0; j < 8; ++j)
        PHichildr[j] = nb_phs0 + 8*i + j;

    E_Int* h271 = _ng.PHs.get_facets_ptr(PHichildr[0]);
    E_Int* h272 = _ng.PHs.get_facets_ptr(PHichildr[1]);
    E_Int* h273 = _ng.PHs.get_facets_ptr(PHichildr[2]);
    E_Int* h274 = _ng.PHs.get_facets_ptr(PHichildr[3]);        
    E_Int* h275 = _ng.PHs.get_facets_ptr(PHichildr[4]);
    E_Int* h276 = _ng.PHs.get_facets_ptr(PHichildr[5]);
    E_Int* h277 = _ng.PHs.get_facets_ptr(PHichildr[6]);
    E_Int* h278 = _ng.PHs.get_facets_ptr(PHichildr[7]);

    NUGA::H27::splitH27(_crd, INT, BOT, TOP, LEFT, RIGHT, FRONT, BACK, h271, h272, h273, h274, h275, h276, h277, h278);

    // set them in the tree
    _tree._PHtree.set_children(PHi, PHichildr, 8);

    // update F2E
    update_F2E(PHi,PHichildr[0],INT,BOT,TOP,LEFT,RIGHT,FRONT,BACK);
  }  
}

///
template <typename ELT_t, typename ngo_t, typename crd_t>
void hierarchical_mesh<ELT_t, ngo_t, crd_t>::__compute_edge_center
(const Vector_t<E_Int> &PGlist, K_FLD::FloatArray& crd, std::map<K_MESH::NO_Edge,E_Int> & ecenter)
{
  for (int i = 0; i < PGlist.size(); i++)
  {
    E_Int PGi = PGlist[i];

    E_Int nb_nodes = _ng.PGs.stride(PGi);
    E_Int* nodes = _ng.PGs.get_facets_ptr(PGlist[i]);
                
    for (E_Int j = 0; j < nb_nodes; j++)
    {

      E_Int ind_point1 = *(nodes+j);
      E_Int ind_point2 = *(nodes+(j+1)%nb_nodes);

      K_MESH::NO_Edge no_edge(ind_point1,ind_point2); // not oriented (a,b) = (b,a)

      auto it = ecenter.find(no_edge);
      if (it == ecenter.end())
      {
        E_Float mid[3];
        K_FUNC::sum<3>(0.5, crd.col(ind_point1-1), 0.5, crd.col(ind_point2-1), mid);
        crd.pushBack(mid, mid+3);
        ecenter[no_edge] = crd.cols();
      }   
    }
  }
}

///
template <typename ELT_t, typename ngo_t, typename crd_t>
void hierarchical_mesh<ELT_t, ngo_t, crd_t>::__compute_face_centers
(K_FLD::FloatArray& crd, const typename ngo_t::unit_type& PGs, const Vector_t<E_Int> &PGlist, Vector_t<E_Int>& fcenter)
{
  E_Int nb_pgs = PGlist.size();
  
  fcenter.resize(nb_pgs, E_IDX_NONE);
  
  E_Int pos = crd.cols();
  
  crd.resize(3, pos + nb_pgs);
  
  for (size_t i = 0; i < nb_pgs; ++i)
  {
    E_Int PGi = PGlist[i];
    // Calcul du centroid 
    __compute_face_center(crd, PGs.get_facets_ptr(PGi), PGs.stride(PGi), crd.col(pos));
    
    fcenter[i] = pos++;
  }
}

///
template <typename ELT_t, typename ngo_t, typename crd_t>
void hierarchical_mesh<ELT_t, ngo_t, crd_t>::__compute_face_center
(const crd_t& crd, const E_Int* nodes, E_Int nb_nodes, E_Float* C)
{   
  K_MESH::Polygon::iso_barycenter<crd_t,3>(crd, nodes, nb_nodes, 1, C);
}

///
template <>
void hierarchical_mesh<K_MESH::Hexahedron, ngon_type, K_FLD::FloatArray>::get_cell_center(E_Int PGi, E_Float* center)
{
    E_Int new_bary[8];
    
    const E_Int* p = _ng.PHs.get_facets_ptr(PGi);
    
    for (int i = 0; i < 2; ++i) // 8 points : bot and top
    {
        const E_Int* nodes = _ng.PGs.get_facets_ptr(p[i]-1);
        E_Int nb_nodes = _ng.PGs.stride(p[i]-1);
        
        for (int k = 0; k  < nb_nodes; ++k)
            new_bary[nb_nodes*i+k] = nodes[k];
        
    }
    
    K_MESH::Polyhedron<STAR_SHAPED>::iso_barycenter(_crd, new_bary, 8, 1, center);
}





}

#endif