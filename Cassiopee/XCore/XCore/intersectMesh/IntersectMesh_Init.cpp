/*    
    Copyright 2013-2024 Onera.

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
#include "mesh.h"
#include "karray.h"

PyObject *K_XCORE::IntersectMesh_Init(PyObject *self, PyObject *args)
{
    PyObject *ARRAY;

    if (!PYPARSETUPLE_(args, O_, &ARRAY)) {
        RAISE("Bad input.");
        return NULL;
    }

    Karray karray;

    E_Int ret;

    ret = Karray_parse_ngon(ARRAY, karray);

    if (ret != 0) return NULL;

    // Init mesh

    IMesh *M = new IMesh(*karray.cn, karray.X, karray.Y, karray.Z, karray.npts);

    // Clean-up

    Karray_free_ngon(karray);

    // TODO(Imad): Python2
    PyObject *hook = PyCapsule_New((void *)M, "IntersectMesh", NULL);

    return hook;
}