/***************************************************************************//**
 * \file diffusiveTermTest.cpp
 * \author Olivier Mesnard (mesnardo@gwu.edu)
 * \brief Unit-test for the diffusive term.
 */


#include "DiffusiveTerm.h"
#include <navierStokes/NavierStokesSolver.h>
#include <CartesianMesh.h>
#include <FlowDescription.h>
#include <SimulationParameters.h>

#include <memory>

#include <petscksp.h>

#ifndef DIMENSIONS
#define DIMENSIONS 2
#endif


int main(int argc, char **argv)
{
  PetscErrorCode ierr;

  const PetscInt dim = DIMENSIONS;

  ierr = PetscInitialize(&argc, &argv, NULL, NULL); CHKERRQ(ierr);

  char dir[PETSC_MAX_PATH_LEN];
  ierr = PetscOptionsGetString(NULL, "-directory", dir, sizeof(dir), NULL); CHKERRQ(ierr);
  std::string directory(dir);

  CartesianMesh cartesianMesh(directory+"/cartesianMesh.yaml");
  FlowDescription<dim> flowDescription(directory+"/flowDescription.yaml");
  SimulationParameters simulationParameters(directory, directory+"/simulationParameters.yaml");

  std::unique_ptr< DiffusiveTerm<dim> > solver(new DiffusiveTerm<dim>(&cartesianMesh, 
                                                                      &flowDescription, 
                                                                      &simulationParameters));

  ierr = solver->initialize(); CHKERRQ(ierr);

  ierr = solver-> calculateExplicitTerms(); CHKERRQ(ierr);

  ierr = solver->calculateExactSolution(); CHKERRQ(ierr);
  ierr = solver->calculateRelativeError(); CHKERRQ(ierr);
  ierr = solver->writeRelativeError(); CHKERRQ(ierr);

  ierr = solver->finalize(); CHKERRQ(ierr);

  ierr = PetscFinalize(); CHKERRQ(ierr);

  return ierr;
} // main