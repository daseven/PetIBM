/***************************************************************************//**
 * \file CartesianMesh.cpp
 * \author Anush Krishnan (anush@bu.edu)
 * \brief Implementation of the methods of the class `CartesianMesh`.
 */


#include "CartesianMesh.h"

#include <fstream>
#include <iomanip>

#include "yaml-cpp/yaml.h"
#include <petscvec.h>
#include <petscviewerhdf5.h>


/**
 * \brief Constructor.
 */
CartesianMesh::CartesianMesh()
{
} // CartesianMesh


/**
 * \brief Constructor -- Parses the YAML input file with the mesh information.
 *
 * \param filePath Path of the file to parse with YAML-CPP
 */
CartesianMesh::CartesianMesh(std::string filePath)
{
  // possibility to overwrite the path of the configuration file
  // using the command-line parameter: `-cartesian_mesh <file-path>`
  char path[PETSC_MAX_PATH_LEN];
  PetscBool found;
  PetscOptionsGetString(NULL, NULL, "-cartesian_mesh", path, sizeof(path), &found);
  if (found)
    filePath = std::string(path);
  initialize(filePath);
} // CartesianMesh


/**
 * \brief Destructor
 */
CartesianMesh::~CartesianMesh()
{
} // ~CartesianMesh


/**
 * \brief Parses the input file using YAML format and discretizes the domain.
 *
 * \param filePath Path of the file containing the mesh parameters
 */
void CartesianMesh::initialize(std::string filePath)
{
  PetscPrintf(PETSC_COMM_WORLD, "\nParsing file %s... ", filePath.c_str());
  
  YAML::Node nodes = YAML::LoadFile(filePath);

  std::string path = nodes["filePath"].as<std::string>("no-path");
  if (path != "no-path")
  {
    read(path);
  }
  else
  {
    PetscInt numCells;
    std::string direction;

    nx = 0;
    ny = 0;
    nz = 0;
    for (unsigned int i=0; i<nodes.size(); i++)
    {
      numCells = 0;
      direction = nodes[i]["direction"].as<std::string>();
      const YAML::Node &subDomains = nodes[i]["subDomains"];
      for (unsigned int j=0; j<subDomains.size(); j++)
        numCells += subDomains[j]["cells"].as<PetscInt>();
      if (direction == "x")
        nx += numCells;
      else if (direction == "y")
        ny += numCells;
      else if (direction == "z")
        nz += numCells;
    }
    
    // allocate memory
    // number of nodes will be 1 greater than the number of cells
    x.resize(nx+1);
    dx.resize(nx);
    y.resize(ny+1);
    dy.resize(ny);
    if (nz > 0)
    {
      z.resize(nz+1);
      dz.resize(nz);
    }
    
    // second pass of the input file
    // to calculate the coordinates of the nodes, and the cell widths
    PetscInt first;
    PetscReal start,
              end,
              stretchRatio,
              h;

    // loop over each direction
    for (unsigned int k=0; k<nodes.size(); k++)
    {
      direction = nodes[k]["direction"].as<std::string>();
      start = nodes[k]["start"].as<PetscReal>();

      const YAML::Node &subDomains = nodes[k]["subDomains"];
      first = 0;
      if (direction == "x") x[first] = start;
      else if (direction == "y") y[first] = start;
      else if (direction == "z") z[first] = start;
      for (unsigned int i=0; i<subDomains.size(); i++)
      {
        end = subDomains[i]["end"].as<PetscReal>();
        numCells = subDomains[i]["cells"].as<PetscInt>();
        stretchRatio = subDomains[i]["stretchRatio"].as<PetscReal>();

        if (fabs(stretchRatio-1.0) < 1.0E-06) // uniform discretization
        {
          h = (end - start)/numCells;
          for (int j=first; j<first+numCells; j++)
          {
            if (direction == "x")
            {
              dx[j] = h;
              x[j+1] = x[j] + dx[j];
            }
            else if (direction == "y")
            {
              dy[j] = h;
              y[j+1] = y[j] + dy[j];
            }
            else if (direction == "z")
            {
              dz[j] = h;
              z[j+1] = z[j] + dz[j];
            }
          }
        }
        else // stretched discretization
        {
          // width of the first cell in the subdomain
          h = (end - start)*(stretchRatio-1)/(pow(stretchRatio, numCells)-1);
          for (int j=first; j<first+numCells; j++)
          {
            // obtain the widths of subsequent cells by
            // multiplying by the stretching ratio
            if (direction == "x")
            {
              dx[j]  = h*pow(stretchRatio, j-first);
              x[j+1] = x[j] + dx[j];
            }
            else if (direction == "y")
            {
              dy[j]  = h*pow(stretchRatio, j-first);
              y[j+1] = y[j] + dy[j];
            }
            else if (direction == "z")
            {
              dz[j]  = h*pow(stretchRatio, j-first);
              z[j+1] = z[j] + dz[j];
            }
          }
        }
        // the first node in the next subdomain is the last node in the current subdomain
        first += numCells;
        start = end;
      }
    }

  }

  PetscPrintf(PETSC_COMM_WORLD, "done.\n");

} // initialize


/**
 * \brief Reads grid points from file.
 *
 * \param filePath Path of the file to read from
 */
PetscErrorCode CartesianMesh::read(std::string filePath)
{
  PetscErrorCode ierr;
  PetscReal val;

  PetscFunctionBeginUser;

  std::ifstream infile(filePath);
  std::string line;
  std::getline(infile, line);
  std::stringstream ss(line);
  PetscInt i;
  std::vector<PetscInt> nCells;
  while (ss >> i)
    nCells.push_back(i);
  nx = nCells[0];
  ny = nCells[1];
  if (nCells.size() == 3)
    nz = nCells[2];
  else
    nz = 0;
  x.reserve(nx+1);
  dx.reserve(nx);
  for (unsigned int i=0; i<nx+1; i++)
  {
    infile >> val;
    x.push_back(val);
  }
  for (unsigned int i=0; i<nx; i++)
    dx[i] = x[i+1] - x[i];
  y.reserve(ny+1);
  dy.reserve(ny);
  for (unsigned int i=0; i<ny+1; i++)
  {
    infile >> val;
    y.push_back(val);
  }
  for (unsigned int i=0; i<ny; i++)
    dy[i] = y[i+1] - y[i];
  if (nz > 0)
  {
    z.reserve(nz+1);
    dz.reserve(nz);
    for (unsigned int i=0; i<nz+1; i++)
    {
      infile >> val;
      z.push_back(val);
    }
    for (unsigned int i=0; i<nz; i++)
      dz[i] = z[i+1] - z[i];
  }
  infile.close();

  PetscFunctionReturn(0);
}


/**
 * \brief Writes grid points into file.
 *
 * \param filePath Path of the file to write
 */
PetscErrorCode CartesianMesh::write(std::string filePath)
{
  PetscErrorCode ierr;

  ierr = PetscPrintf(PETSC_COMM_WORLD, "\nWriting grid into file... "); CHKERRQ(ierr);

  PetscMPIInt rank;
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD, &rank); CHKERRQ(ierr);

  if (rank == 0)
  {
    std::ofstream streamFile(filePath);
    if (nz == 0)
    {
      streamFile << nx << '\t' << ny << '\n';
    }
    else
    {
      streamFile << nx << '\t' << ny << '\t' << nz << '\n';
    }
    for (std::vector<PetscReal>::const_iterator i=x.begin(); i!=x.end(); ++i)
      streamFile << std::setprecision(16) << *i << '\n';
    for (std::vector<PetscReal>::const_iterator i=y.begin(); i!=y.end(); ++i)
      streamFile << std::setprecision(16) << *i << '\n';
    if (nz > 0)
    {  
      for (std::vector<PetscReal>::const_iterator i=z.begin(); i!=z.end(); ++i)
        streamFile << std::setprecision(16) << *i << '\n';
    }
    streamFile.close();
  }
  
  ierr = PetscPrintf(PETSC_COMM_WORLD, "done.\n"); CHKERRQ(ierr);

  return 0;
} // write


#ifdef PETSC_HAVE_HDF5
/**
 * \brief Writes the grid stations into a HDF5 file.
 *
 * \param filePath Path of the file to write
 * \param mode Staggered mode to define locations of a variable points
 */
PetscErrorCode CartesianMesh::write(std::string filePath, StaggeredMode mode, BoundaryType bType)
{
  PetscErrorCode ierr;
  PetscReal value;
  PetscInt n;
  PetscViewer viewer;
  Vec xs, ys, zs;
  PetscMPIInt rank;

  ierr = MPI_Comm_rank(PETSC_COMM_WORLD, &rank); CHKERRQ(ierr);

  if (rank == 0)
  {
    ierr = PetscViewerHDF5Open(PETSC_COMM_SELF, filePath.c_str(), FILE_MODE_WRITE, &viewer); CHKERRQ(ierr);
    // stations along a gridline in the x-direction
    if (mode == STAGGERED_MODE_X)
    {
      n = (bType == PERIODIC) ? nx : nx-1;
      ierr = VecCreateSeq(PETSC_COMM_SELF, n, &xs); CHKERRQ(ierr);
      for (int i=0; i<n; i++)
      {
        value = x[i+1];
        ierr = VecSetValue(xs, i, value, INSERT_VALUES); CHKERRQ(ierr);
      }
    }
    else
    {
      ierr = VecCreateSeq(PETSC_COMM_SELF, nx, &xs); CHKERRQ(ierr);
      for (int i=0; i<nx; i++)
      {
        value = 0.5 * (x[i] + x[i+1]);
        ierr = VecSetValue(xs, i, value, INSERT_VALUES); CHKERRQ(ierr);
      }  
    }
    ierr = PetscObjectSetName((PetscObject) xs, "x"); CHKERRQ(ierr);
    ierr = VecView(xs, viewer); CHKERRQ(ierr);
    ierr = VecDestroy(&xs); CHKERRQ(ierr);
    // stations along a gridline in the y-direction
    if (mode == STAGGERED_MODE_Y)
    {
      n = (bType == PERIODIC) ? ny : ny-1;
      ierr = VecCreateSeq(PETSC_COMM_SELF, n, &ys); CHKERRQ(ierr);
      for (int i=0; i<n; i++)
      {
        value = y[i+1];
        ierr = VecSetValue(ys, i, value, INSERT_VALUES); CHKERRQ(ierr);
      }  
    }
    else
    {
      ierr = VecCreateSeq(PETSC_COMM_SELF, ny, &ys); CHKERRQ(ierr);
      for (int i=0; i<ny; i++)
      {
        value = 0.5 * (y[i] + y[i+1]);
        ierr = VecSetValue(ys, i, value, INSERT_VALUES); CHKERRQ(ierr);
      }  
    }
    ierr = PetscObjectSetName((PetscObject) ys, "y"); CHKERRQ(ierr);
    ierr = VecView(ys, viewer); CHKERRQ(ierr);
    ierr = VecDestroy(&ys); CHKERRQ(ierr);
    if (nz > 0)
    {
      // stations along a gridline in the z-direction
      if (mode == STAGGERED_MODE_Z)
      {
        n = (bType == PERIODIC) ? nz : nz-1;
        ierr = VecCreateSeq(PETSC_COMM_SELF, n, &zs); CHKERRQ(ierr);
        for (int i=0; i<n; i++)
        {
          value = z[i+1];
          ierr = VecSetValue(zs, i, value, INSERT_VALUES); CHKERRQ(ierr);
        }  
      }
      else
      {
        ierr = VecCreateSeq(PETSC_COMM_SELF, nz, &zs); CHKERRQ(ierr);
        for (int i=0; i<nz; i++)
        {
          value = 0.5 * (z[i] + z[i+1]);
          ierr = VecSetValue(zs, i, value, INSERT_VALUES); CHKERRQ(ierr);
        }  
      }
      ierr = PetscObjectSetName((PetscObject) zs, "z"); CHKERRQ(ierr);
      ierr = VecView(zs, viewer); CHKERRQ(ierr);
      ierr = VecDestroy(&zs); CHKERRQ(ierr);
    }
    ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);
  }

  return 0;
} // write
#endif


/**
 * \brief Prints information about the Cartesian mesh.
 */
PetscErrorCode CartesianMesh::printInfo()
{
  PetscErrorCode ierr;
  ierr = PetscPrintf(PETSC_COMM_WORLD, "\n---------------------------------------\n"); CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD, "Cartesian grid\n"); CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD, "---------------------------------------\n"); CHKERRQ(ierr);
  if (nz > 0)
  {
    ierr = PetscPrintf(PETSC_COMM_WORLD, "number of cells: %d x %d x %d\n", nx, ny, nz); CHKERRQ(ierr);
  }
  else
  {
    ierr = PetscPrintf(PETSC_COMM_WORLD, "number of cells: %d x %d\n", nx, ny); CHKERRQ(ierr);
  }
  ierr = PetscPrintf(PETSC_COMM_WORLD, "---------------------------------------\n"); CHKERRQ(ierr);

  return 0;
} // printInfo