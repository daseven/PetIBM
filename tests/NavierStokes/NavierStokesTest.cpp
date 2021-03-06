/***************************************************************************//**
 * \file NavierStokesTest.cpp
 * \author Anush Krishnan (anush@bu.edu)
 * \brief Unit-test for the Navier-Stokes solver.
 */


#include "createSolver.h"
#include "gtest/gtest.h"
#include <petscdmcomposite.h>


class NavierStokesTest : public ::testing::Test
{
public:
  std::string directory;
  CartesianMesh cartesianMesh;
  FlowDescription<2> flowDescription;
  SimulationParameters simulationParameters;
  std::unique_ptr< NavierStokesSolver<2> > solver;
  Vec lambdaGold, error;

  NavierStokesTest()
  {
    lambdaGold = PETSC_NULL;
    
    // read input files and create solver
    directory = "NavierStokes/case";
    cartesianMesh = CartesianMesh(directory+"/cartesianMesh.yaml");
    flowDescription = FlowDescription<2>(directory+"/flowDescription.yaml");
    simulationParameters = SimulationParameters(directory, directory+"/simulationParameters.yaml");
    solver = createSolver<2>(&cartesianMesh, &flowDescription, &simulationParameters);
  }

  virtual void SetUp()
  {
    // initialise solver
    solver->initialize();

    // perform the simulation
    while (!solver->finished())
    {
      solver->stepTime();
    }
  }

  virtual void TearDown()
  {
    solver->finalize();
    if (lambdaGold != PETSC_NULL) VecDestroy(&lambdaGold);
    if (error != PETSC_NULL) VecDestroy(&error);
  }
};

TEST_F(NavierStokesTest, ComparePhi)
{
  PetscViewer viewer;
  Vec phi;
  PetscReal errorNorm, goldNorm;

  // create vector to store the gold data and the error
  VecDuplicate(solver->lambda, &lambdaGold);
  VecDuplicate(solver->lambda, &error);

  // read the gold data from file
  DMCompositeGetAccess(solver->lambdaPack, lambdaGold, &phi);
  std::string filePath = directory + "/data/phi.dat";
  PetscViewerBinaryOpen(PETSC_COMM_WORLD, filePath.c_str(), FILE_MODE_READ, &viewer);
  VecLoad(phi, viewer);
  PetscViewerDestroy(&viewer);
  DMCompositeRestoreAccess(solver->lambdaPack, lambdaGold, &phi);

  // check the difference
  VecWAXPY(error, -1, solver->lambda, lambdaGold);
  VecNorm(error, NORM_2, &errorNorm);
  VecNorm(lambdaGold, NORM_2, &goldNorm);

  EXPECT_LT(errorNorm/goldNorm, 5.0E-04);
}


int main(int argc, char **argv)
{
  PetscErrorCode ierr, result;

  ::testing::InitGoogleTest(&argc, argv);
  ierr = PetscInitialize(&argc, &argv, NULL, NULL); CHKERRQ(ierr);
  result = RUN_ALL_TESTS();
  ierr = PetscFinalize(); CHKERRQ(ierr);

  return result;
}