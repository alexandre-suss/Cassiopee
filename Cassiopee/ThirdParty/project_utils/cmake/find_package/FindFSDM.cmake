# FSCOMMON_INCLUDE_DIR - where to find cgns.h, etc.
# FSDM_LIBRARIES   - List of fully qualified libraries to link against when using FSDM.
# FSDM_FOUND       - Do not attempt to use FSDM if "no" or undefined.
cmake_minimum_required(VERSION 3.12)

find_path(FSCOMMON_INCLUDE_DIR            NAMES FSCommon.h            PATH_SUFFIXES FSCommon           )
find_path(FSBOUNDARYCONDITION_INCLUDE_DIR NAMES FSBoundaryCondition.h PATH_SUFFIXES FSBoundaryCondition)
find_path(FSDATALOG_INCLUDE_DIR           NAMES FSDataLog.h           PATH_SUFFIXES FSDataLog          )
find_path(FSMESH_INCLUDE_DIR              NAMES FSMesh.h              PATH_SUFFIXES FSMesh             )
find_path(FSGEOMETRY_INCLUDE_DIR          NAMES FSGeometry.h          PATH_SUFFIXES FSGeometry         )
find_path(FSRELATIONSMODEL_INCLUDE_DIR    NAMES FSRelationsModel.h    PATH_SUFFIXES FSRelationsModel   )
find_path(FSDATAMANAGER_INCLUDE_DIR       NAMES FSDataManager.h       PATH_SUFFIXES FSDataManager      )

find_library(FSCOMMON_LIBRARY            NAMES libFSCommon.so           )
find_library(FSBOUNDARYCONDITION_LIBRARY NAMES libFSBoundaryCondition.so)
find_library(FSDATALOG_LIBRARY           NAMES libFSDataLog.so          )
find_library(FSMESH_LIBRARY              NAMES libFSMesh.so             )
find_library(FSGEOMETRY_LIBRARY          NAMES libFSGeometry.so         )
find_library(FSRELATIONSMODEL_LIBRARY    NAMES libFSRelationsModel.so   )
find_library(FSDATAMANAGER_LIBRARY       NAMES libFSDataManager.so      )
mark_as_advanced(
  FSCOMMON_INCLUDE_DIR            FSCOMMON_LIBRARY
  FSBOUNDARYCONDITION_INCLUDE_DIR FSBOUNDARYCONDITION_LIBRARY
  FSDATALOG_INCLUDE_DIR           FSDATALOG_LIBRARY
  FSMESH_INCLUDE_DIR              FSMESH_LIBRARY
  FSGEOMETRY_INCLUDE_DIR          FSGEOMETRY_LIBRARY
  FSRELATIONSMODEL_INCLUDE_DIR    FSRELATIONSMODEL_LIBRARY
  FSDATAMANAGER_INCLUDE_DIR       FSDATAMANAGER_LIBRARY
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FSDM
  REQUIRED_VARS
    FSCOMMON_INCLUDE_DIR            FSCOMMON_LIBRARY
    FSBOUNDARYCONDITION_INCLUDE_DIR FSBOUNDARYCONDITION_LIBRARY
    FSDATALOG_INCLUDE_DIR           FSDATALOG_LIBRARY
    FSMESH_INCLUDE_DIR              FSMESH_LIBRARY
    FSGEOMETRY_INCLUDE_DIR          FSGEOMETRY_LIBRARY
    FSRELATIONSMODEL_INCLUDE_DIR    FSRELATIONSMODEL_LIBRARY
    FSDATAMANAGER_INCLUDE_DIR       FSDATAMANAGER_LIBRARY
)


if (FSDM_FOUND)
  set(FSDM_INCLUDE_DIRS "${FSCOMMON_INCLUDE_DIR} ${FSBOUNDARYCONDITION_INCLUDE_DIR} ${FSDATALOG_INCLUDE_DIR} ${FSMESH_INCLUDE_DIR} ${FSGEOMETRY_INCLUDE_DIR} ${FSRELATIONSMODEL_INCLUDE_DIR} ${FSDATAMANAGER_INCLUDE_DIR}")
  set(FSDM_LIBRARIES "${FSCOMMON_LIBRARY} ${FSBOUNDARYCONDITION_LIBRARY} ${FSDATALOG_LIBRARY} ${FSMESH_LIBRARY} ${FSGEOMETRY_LIBRARY} ${FSRELATIONSMODEL_LIBRARY} ${FSDATAMANAGER_LIBRARY}")

  add_library(FSDM::FSCommon SHARED IMPORTED)
  set_target_properties(FSDM::FSCommon PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FSCOMMON_INCLUDE_DIR};${FSCOMMON_INCLUDE_DIR}/../thirdparty"
    IMPORTED_LOCATION "${FSCOMMON_LIBRARY}"
  )
  add_library(FSDM::FSBoundaryCondition SHARED IMPORTED)
  set_target_properties(FSDM::FSBoundaryCondition PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FSBOUNDARYCONDITION_INCLUDE_DIR}"
    IMPORTED_LOCATION "${FSBOUNDARYCONDITION_LIBRARY}"
  )
  add_library(FSDM::FSDataLog SHARED IMPORTED)
  set_target_properties(FSDM::FSDataLog PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FSDATALOG_INCLUDE_DIR}"
    IMPORTED_LOCATION "${FSDATALOG_LIBRARY}"
  )
  add_library(FSDM::FSMesh SHARED IMPORTED)
  set_target_properties(FSDM::FSMesh PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FSMESH_INCLUDE_DIR}"
    IMPORTED_LOCATION "${FSMESH_LIBRARY}"
  )
  add_library(FSDM::FSGeometry SHARED IMPORTED)
  set_target_properties(FSDM::FSGeometry PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FSGEOMETRY_INCLUDE_DIR}"
    IMPORTED_LOCATION "${FSGEOMETRY_LIBRARY}"
  )
  add_library(FSDM::FSRelationsModel SHARED IMPORTED)
  set_target_properties(FSDM::FSRelationsModel PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FSRELATIONSMODEL_INCLUDE_DIR}"
    IMPORTED_LOCATION "${FSRELATIONSMODEL_LIBRARY}"
  )
  add_library(FSDM::FSDataManager SHARED IMPORTED)
  set_target_properties(FSDM::FSDataManager PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FSDATAMANAGER_INCLUDE_DIR}"
    IMPORTED_LOCATION "${FSDATAMANAGER_LIBRARY}"
  )
endif ()
