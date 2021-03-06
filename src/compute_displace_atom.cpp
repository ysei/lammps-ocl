/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under 
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "math.h"
#include "string.h"
#include "compute_displace_atom.h"
#include "atom.h"
#include "update.h"
#include "group.h"
#include "domain.h"
#include "modify.h"
#include "fix.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

ComputeDisplaceAtom::ComputeDisplaceAtom(LAMMPS *lmp, int narg, char **arg) :
  Compute(lmp, narg, arg)
{
  if (narg != 3) error->all("Illegal compute displace/atom command");

  peratom_flag = 1;
  size_peratom_cols = 4;

  // create a new fix store/state style
  // id = compute-ID + store_state, fix group = compute group

  int n = strlen(id) + strlen("_store_state") + 1;
  id_fix = new char[n];
  strcpy(id_fix,id);
  strcat(id_fix,"_store_state");

  char **newarg = new char*[7];
  newarg[0] = id_fix;
  newarg[1] = group->names[igroup];
  newarg[2] = (char *) "store/state";
  newarg[3] = (char *) "0";
  newarg[4] = (char *) "xu";
  newarg[5] = (char *) "yu";
  newarg[6] = (char *) "zu";
  modify->add_fix(7,newarg);
  delete [] newarg;

  nmax = 0;
  displace = NULL;
}

/* ---------------------------------------------------------------------- */

ComputeDisplaceAtom::~ComputeDisplaceAtom()
{
  // check nfix in case all fixes have already been deleted

  if (modify->nfix) modify->delete_fix(id_fix);

  delete [] id_fix;
  memory->destroy(displace);
}

/* ---------------------------------------------------------------------- */

void ComputeDisplaceAtom::init()
{
  // set fix which stores original atom coords

  int ifix = modify->find_fix(id_fix);
  if (ifix < 0) error->all("Could not find compute displace/atom fix ID");
  fix = modify->fix[ifix];
}

/* ---------------------------------------------------------------------- */

void ComputeDisplaceAtom::compute_peratom()
{
  invoked_peratom = update->ntimestep;

  // grow local displacement array if necessary

  if (atom->nlocal > nmax) {
    memory->destroy(displace);
    nmax = atom->nmax;
    memory->create(displace,nmax,4,"displace/atom:displace");
    array_atom = displace;
  }

  // dx,dy,dz = displacement of atom from original position
  // original unwrapped position is stored by fix
  // for triclinic, need to unwrap current atom coord via h matrix

  double **xoriginal = fix->array_atom;

  double **x = atom->x;
  int *mask = atom->mask;
  int *image = atom->image;
  int nlocal = atom->nlocal;

  double *h = domain->h;
  double xprd = domain->xprd;
  double yprd = domain->yprd;
  double zprd = domain->zprd;

  int xbox,ybox,zbox;
  double dx,dy,dz;

  if (domain->triclinic == 0) {
    for (int i = 0; i < nlocal; i++)
      if (mask[i] & groupbit) {
	xbox = (image[i] & 1023) - 512;
	ybox = (image[i] >> 10 & 1023) - 512;
	zbox = (image[i] >> 20) - 512;
	dx = x[i][0] + xbox*xprd - xoriginal[i][0];
	dy = x[i][1] + ybox*yprd - xoriginal[i][1];
	dz = x[i][2] + zbox*zprd - xoriginal[i][2];
	displace[i][0] = dx;
	displace[i][1] = dy;
	displace[i][2] = dz;
	displace[i][3] = sqrt(dx*dx + dy*dy + dz*dz);
      } else displace[i][0] = displace[i][1] =
	       displace[i][2] = displace[i][3] = 0.0;

  } else {
    for (int i = 0; i < nlocal; i++)
      if (mask[i] & groupbit) {
	xbox = (image[i] & 1023) - 512;
	ybox = (image[i] >> 10 & 1023) - 512;
	zbox = (image[i] >> 20) - 512;
	dx = x[i][0] + h[0]*xbox + h[5]*ybox + h[4]*zbox - xoriginal[i][0];
	dy = x[i][1] + h[1]*ybox + h[3]*zbox - xoriginal[i][1];
	dz = x[i][2] + h[2]*zbox - xoriginal[i][2];
	displace[i][0] = dx;
	displace[i][1] = dy;
	displace[i][2] = dz;
	displace[i][3] = sqrt(dx*dx + dy*dy + dz*dz);
      } else displace[i][0] = displace[i][1] =
	       displace[i][2] = displace[i][3] = 0.0;
  }
}

/* ----------------------------------------------------------------------
   memory usage of local atom-based array
------------------------------------------------------------------------- */

double ComputeDisplaceAtom::memory_usage()
{
  double bytes = nmax*4 * sizeof(double);
  return bytes;
}
