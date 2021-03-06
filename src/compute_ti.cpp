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
   Contributing author: Sai Jayaraman (University of Notre Dame)
------------------------------------------------------------------------- */

#include "mpi.h"
#include "string.h"
#include "compute_ti.h"
#include "update.h"
#include "modify.h"
#include "domain.h"
#include "force.h"
#include "pair.h"
#include "kspace.h"
#include "input.h"
#include "variable.h"
#include "error.h"

using namespace LAMMPS_NS;

enum{PAIR,TAIL,KSPACE};

/* ---------------------------------------------------------------------- */

ComputeTI::ComputeTI(LAMMPS *lmp, int narg, char **arg) : 
  Compute(lmp, narg, arg) 
{ 
  if (narg < 4) error->all("Illegal compute ti command");

  peflag = 1;
  scalar_flag = 1;
  extscalar = 1;
  timeflag = 1;

  // terms come in triplets

  nterms = (narg-3) / 3;
  if (narg != 3*nterms + 3) error->all("Illegal compute ti command");

  which = new int[nterms];
  ivar1 = new int[nterms];
  ivar2 = new int[nterms];
  var1 = new char*[nterms];
  var2 = new char*[nterms];
  pptr = new Pair*[nterms];
  pstyle = new char*[nterms];

  for (int m = 0; m < nterms; m++) pstyle[m] = NULL;
    
  // parse keywords

  nterms = 0;

  int iarg = 3;
  while (iarg < narg) {
    if (iarg+3 > narg) error->all("Illegal compute ti command");
    if (strcmp(arg[iarg],"kspace") == 0) which[nterms] = KSPACE;
    else if (strcmp(arg[iarg],"tail") == 0) which[nterms] = TAIL;
    else {
      which[nterms] = PAIR;
      int n = strlen(arg[iarg]) + 1;
      pstyle[nterms] = new char[n];
      strcpy(pstyle[nterms],arg[iarg]);
    }

    if (strstr(arg[iarg+1],"v_") == arg[iarg+1]) {
      int n = strlen(&arg[iarg+1][2]) + 1;
      var1[nterms] = new char[n];
      strcpy(var1[nterms],&arg[iarg+1][2]);
    } else error->all("Illegal compute ti command");
    if (strstr(arg[iarg+2],"v_") == arg[iarg+2]) {
      int n = strlen(&arg[iarg+2][2]) + 1;
      var2[nterms] = new char[n];
      strcpy(var2[nterms],&arg[iarg+2][2]);
    } else error->all("Illegal compute ti command");

    nterms++;
    iarg += 3;
  }
}

/* --------------------------------------------------------------------- */

ComputeTI::~ComputeTI()
{
  for (int m = 0; m < nterms; m++) {
    delete [] var1[m];
    delete [] var2[m];
    delete [] pstyle[m];
  }
  delete [] which;
  delete [] ivar1;
  delete [] ivar2;
  delete [] var1;
  delete [] var2;
  delete [] pptr;
  delete [] pstyle;
}

/* --------------------------------------------------------------------- */

void ComputeTI::init()
{
  // setup and error checks

  for (int m = 0; m < nterms; m++) {
    ivar1[m] = input->variable->find(var1[m]);
    ivar2[m] = input->variable->find(var2[m]);
    if (ivar1[m] < 0 || ivar2 < 0)
      error->all("Variable name for compute ti does not exist");
    if (!input->variable->equalstyle(ivar1[m]) ||
	!input->variable->equalstyle(ivar2[m]))
      error->all("Variable for compute ti is invalid style");

    if (which[m] == PAIR) {
      pptr[m] = force->pair_match(pstyle[m],1);
      if (pptr[m] == NULL) error->all("Compute ti pair style does not exist");

    } else if (which[m] == TAIL) {
      if (force->pair == NULL || force->pair->tail_flag == 0) 
	error->all("Compute ti tail when pair style does not "
		   "compute tail corrections");

    } else if (which[m] == KSPACE) {
      if (force->kspace == NULL) 
	error->all("Compute ti is incompatible with KSpace style");
    }
  }
}

/* --------------------------------------------------------------------- */

double ComputeTI::compute_scalar()
{
  double eng,engall,value1,value2;

  invoked_scalar = update->ntimestep;
  if (update->eflag_global != invoked_scalar)
    error->all("Energy was not tallied on needed timestep");

  double dUdl = 0.0;

  for (int m = 0; m < nterms; m++) {
    value1 = input->variable->compute_equal(ivar1[m]);
    value2 = input->variable->compute_equal(ivar2[m]);
    if (value1 == 0.0) continue;

    if (which[m] == PAIR) {
      eng = pptr[m]->eng_vdwl + pptr[m]->eng_coul;
      MPI_Allreduce(&eng,&engall,1,MPI_DOUBLE,MPI_SUM,world);
      dUdl += engall/value1 * value2;

    } else if (which[m] == TAIL) {
      double vol = domain->xprd*domain->yprd*domain->zprd;
      eng = force->pair->etail / vol;
      dUdl += eng/value1 * value2;

    } else if (which[m] == KSPACE) {
      eng = force->kspace->energy;
      dUdl += eng/value1 * value2;
    }
  }

  scalar = dUdl;
  return scalar;
}
