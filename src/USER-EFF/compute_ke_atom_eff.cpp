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

/* ----------------------------------------------------------------------
   Contributing author: Andres Jaramillo-Botero (Caltech)
------------------------------------------------------------------------- */

#include "string.h"
#include "compute_ke_atom_eff.h"
#include "atom.h"
#include "update.h"
#include "modify.h"
#include "comm.h"
#include "force.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

ComputeKEAtomEff::ComputeKEAtomEff(LAMMPS *lmp, int narg, char **arg) :
  Compute(lmp, narg, arg)
{
  if (narg != 3) error->all("Illegal compute ke/atom/eff command");

  peratom_flag = 1;
  size_peratom_cols = 0;

  nmax = 0;
  ke = NULL;

  // error check

  if (!atom->ervel_flag) 
    error->all("Compute ke/atom/eff requires atom attribute ervel");
}

/* ---------------------------------------------------------------------- */

ComputeKEAtomEff::~ComputeKEAtomEff()
{
  memory->sfree(ke);
}

/* ---------------------------------------------------------------------- */

void ComputeKEAtomEff::init()
{
  int count = 0;
  for (int i = 0; i < modify->ncompute; i++)
    if (strcmp(modify->compute[i]->style,"ke/atom/eff") == 0) count++;
  if (count > 1 && comm->me == 0)
    error->warning("More than one compute ke/atom/eff");
}

/* ---------------------------------------------------------------------- */

void ComputeKEAtomEff::compute_peratom()
{
  invoked_peratom = update->ntimestep;

  // grow ke array if necessary

  if (atom->nlocal > nmax) {
    memory->sfree(ke);
    nmax = atom->nmax;
    ke = (double *)
      memory->smalloc(nmax*sizeof(double),"compute/ke/atom/eff:ke");
    vector_atom = ke;
  }
  
  // compute kinetic energy for each atom in group
  
  double mvv2e = force->mvv2e;
  double **v = atom->v;
  double *ervel = atom->ervel;
  double *mass = atom->mass;
  double *rmass = atom->rmass;
  int *spin = atom->spin;
  int *mask = atom->mask;
  int *type = atom->type;
  int nlocal = atom->nlocal;

  if (rmass)
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit) {
        ke[i] = 0.5 * mvv2e * rmass[i] * 
	  (v[i][0]*v[i][0] + v[i][1]*v[i][1] + v[i][2]*v[i][2]);
	if (spin[i]) 
          ke[i] += 0.5 * mvv2e * rmass[i] * ervel[i]*ervel[i] * 0.75;
      } else ke[i] = 0.0;
    }

  else
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit) {
        ke[i] = 0.5 * mvv2e * mass[type[i]] * 
	  (v[i][0]*v[i][0] + v[i][1]*v[i][1] + v[i][2]*v[i][2]);
	if (spin[i]) 
          ke[i] += 0.5 * mvv2e * mass[type[i]] * ervel[i]*ervel[i] * 0.75;
      } else ke[i] = 0.0;
    }
}

/* ----------------------------------------------------------------------
   memory usage of local atom-based array
------------------------------------------------------------------------- */

double ComputeKEAtomEff::memory_usage()
{
  double bytes = nmax * sizeof(double);
  return bytes;
}