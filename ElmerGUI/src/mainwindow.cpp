/*****************************************************************************
 *                                                                           *
 *  Elmer, A Finite Element Software for Multiphysical Problems              *
 *                                                                           *
 *  Copyright 1st April 1995 - , CSC - Scientific Computing Ltd., Finland    *
 *                                                                           *
 *  This program is free software; you can redistribute it and/or            *
 *  modify it under the terms of the GNU General Public License              *
 *  as published by the Free Software Foundation; either version 2           *
 *  of the License, or (at your option) any later version.                   *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program (in file fem/GPL-2); if not, write to the        *
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,         *
 *  Boston, MA 02110-1301, USA.                                              *
 *                                                                           *
 *****************************************************************************/

/*****************************************************************************
 *                                                                           *
 *  ElmerGUI mainwindow                                                      *
 *                                                                           *
 *****************************************************************************
 *                                                                           *
 *  Authors: Mikko Lyly, Juha Ruokolainen and Peter R�back                   *
 *  Email:   Juha.Ruokolainen@csc.fi                                         *
 *  Web:     http://www.csc.fi/elmer                                         *
 *  Address: CSC - Scientific Computing Ltd.                                 *
 *           Keilaranta 14                                                   *
 *           02101 Espoo, Finland                                            *
 *                                                                           *
 *  Original Date: 15 Mar 2008                                               *
 *                                                                           *
 *****************************************************************************/

#include <QtGui>
#include <QFile>
#include <QFont>
#include <iostream>
#include <fstream>
#include "mainwindow.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
 
using namespace std;

#undef MPICH2

// Construct main window...
//-----------------------------------------------------------------------------
MainWindow::MainWindow()
{
#ifdef __APPLE__
  // find "Home directory":
  char executablePath[MAXPATHLENGTH] = {0};
  uint32_t len = MAXPATHLENGTH;
  this->homePath = "";
  if(! _NSGetExecutablePath( (char*) executablePath, &len)){
    // remove executable name from path:
    *(strrchr(executablePath,'/'))='\0';
    // remove last path component name from path:
    *(strrchr(executablePath,'/'))='\0';
    this->homePath = executablePath;
  }
#else
  homePath="";
#endif

  // load ini file:
  egIni = new EgIni(this);

  // splash screen:
  setupSplash();

  // load images and icons for the main window:
  updateSplash("Loading images...");
  iconChecked = QIcon(":/icons/dialog-ok.png");
  iconEmpty = QIcon("");
  
  // load tetlib:
  updateSplash("Loading tetlib...");
  tetlibAPI = new TetlibAPI;
  tetlibPresent = tetlibAPI->loadTetlib();
  this->in = tetlibAPI->in;
  this->out = tetlibAPI->out;
  
  // load nglib:
  updateSplash("Loading nglib...");
  nglibAPI = new NglibAPI;
  nglibPresent = nglibAPI->loadNglib();
  this->mp = nglibAPI->mp;
  this->ngmesh = nglibAPI->ngmesh;
  this->nggeom = nglibAPI->nggeom;

  // construct elmergrid:
  updateSplash("Constructing elmergrid...");
  elmergridAPI = new ElmergridAPI;

  // set dynamic limits:
  limit = new Limit;
  setDynamicLimits();
  
  // widgets and utilities:
  updateSplash("ElmerGUI loading...");
  glWidget = new GLWidget(this);
  setCentralWidget(glWidget);
  sifWindow = new SifWindow(this);
  meshControl = new MeshControl(this);
  boundaryDivide = new BoundaryDivide(this);
  meshingThread = new MeshingThread(this);
  meshutils = new Meshutils;
  solverLogWindow = new SifWindow(this);
  solver = new QProcess(this);
  post = new QProcess(this);
  compiler = new QProcess(this);
  meshSplitter = new QProcess(this);
  meshUnifier = new QProcess(this);
  generalSetup = new GeneralSetup(this);
  equationEditor = new DynamicEditor[limit->maxEquations()];
  materialEditor = new DynamicEditor[limit->maxMaterials()];
  bodyForceEditor = new DynamicEditor[limit->maxBodyforces()];
  boundaryConditionEditor = new DynamicEditor[limit->maxBcs()];
  boundaryPropertyEditor = new BoundaryPropertyEditor[limit->maxBoundaries()];
  initialConditionEditor = new DynamicEditor[limit->maxInitialconditions()];
  solverParameterEditor = new SolverParameterEditor[limit->maxSolvers()];
  bodyPropertyEditor = new BodyPropertyEditor[limit->maxBodies()];
  summaryEditor = new SummaryEditor(this);
  sifGenerator = new SifGenerator;
  sifGenerator->limit = this->limit;
  elmerDefs = new QDomDocument;
  edfEditor = new EdfEditor;
  convergenceView = new ConvergenceView(limit, this);
  glControl = new GLcontrol(this);
  parallel = new Parallel(this);
  checkMpi = new CheckMpi;
#ifdef OCC62
  cadView = new CadView(this);
#endif

  createActions();
  createMenus();
  createToolBars();
  createStatusBar();
      
  // glWidget emits (list_t*) when a boundary is selected by double clicking:
  connect(glWidget, SIGNAL(signalBoundarySelected(list_t*)), 
	  this, SLOT(boundarySelectedSlot(list_t*)));

  // meshingThread emits (void) when the mesh generation has finished or terminated:
  connect(meshingThread, SIGNAL(started()), this, SLOT(meshingStartedSlot()));
  connect(meshingThread, SIGNAL(finished()), this, SLOT(meshingFinishedSlot()));
  connect(meshingThread, SIGNAL(terminated()), this, SLOT(meshingTerminatedSlot()));

  // boundaryDivide emits (double) when "divide button" has been clicked:
  connect(boundaryDivide, SIGNAL(signalDoDivideSurface(double)), 
	  this, SLOT(doDivideSurfaceSlot(double)));

  // boundaryDivide emits (double) when "divide button" has been clicked:
  connect(boundaryDivide, SIGNAL(signalDoDivideEdge(double)), 
	  this, SLOT(doDivideEdgeSlot(double)));

  // solver emits (int) when finished:
  connect(solver, SIGNAL(finished(int)), 
	  this, SLOT(solverFinishedSlot(int))) ;

  // solver emits (void) when there is something to read from stdout:
  connect(solver, SIGNAL(readyReadStandardOutput()), 
	  this, SLOT(solverStdoutSlot()));

  // solver emits (void) when there is something to read from stderr:
  connect(solver, SIGNAL(readyReadStandardError()), 
	  this, SLOT(solverStderrSlot()));

  // solver emits (QProcess::ProcessError) when error occurs:
  connect(solver, SIGNAL(error(QProcess::ProcessError)), 
	  this, SLOT(solverErrorSlot(QProcess::ProcessError)));

  // solver emits (QProcess::ProcessState) when state changed:
  connect(solver, SIGNAL(stateChanged(QProcess::ProcessState)), 
	  this, SLOT(solverStateChangedSlot(QProcess::ProcessState)));

  // compiler emits (int) when finished:
  connect(compiler, SIGNAL(finished(int)), 
	  this, SLOT(compilerFinishedSlot(int))) ;

  // compiler emits (void) when there is something to read from stdout:
  connect(compiler, SIGNAL(readyReadStandardOutput()), 
	  this, SLOT(compilerStdoutSlot()));

  // compiler emits (void) when there is something to read from stderr:
  connect(compiler, SIGNAL(readyReadStandardError()), 
	  this, SLOT(compilerStderrSlot()));

  // post emits (int) when finished:
  connect(post, SIGNAL(finished(int)), 
	  this, SLOT(postProcessFinishedSlot(int))) ;

  // meshSplitter emits (int) when finished:
  connect(meshSplitter, SIGNAL(finished(int)),
	  this, SLOT(meshSplitterFinishedSlot(int)));

  // meshSplitter emits(void) when there is something to read from stdout:
  connect(meshSplitter, SIGNAL(readyReadStandardOutput()),
	  this, SLOT(meshSplitterStdoutSlot()));
  
  // meshSplitter emits(void) when there is something to read from stderr:
  connect(meshSplitter, SIGNAL(readyReadStandardError()),
	  this, SLOT(meshSplitterStderrSlot()));
  
  // meshUnifier emits (int) when finished:
  connect(meshUnifier, SIGNAL(finished(int)),
	  this, SLOT(meshUnifierFinishedSlot(int)));

  // meshUnifier emits(void) when there is something to read from stdout:
  connect(meshUnifier, SIGNAL(readyReadStandardOutput()),
	  this, SLOT(meshUnifierStdoutSlot()));
  
  // meshUnifier emits(void) when there is something to read from stderr:
  connect(meshUnifier, SIGNAL(readyReadStandardError()),
	  this, SLOT(meshUnifierStderrSlot()));
  
  // set initial state:
  operations = 0;
  meshControl->nglibPresent = nglibPresent;
  meshControl->tetlibPresent = tetlibPresent;
  meshControl->defaultControls();
  nglibInputOk = false;
  tetlibInputOk = false;
  activeGenerator = GEN_UNKNOWN;
  bcEditActive = false;
  bodyEditActive = false;
  showConvergence = egIni->isSet("showconvergence");

  // background image:
  glWidget->stateUseBgImage = egIni->isSet("bgimage");
  glWidget->stateStretchBgImage = egIni->isSet("bgimagestretch");
  glWidget->stateAlignRightBgImage = egIni->isSet("bgimagealignright");
  glWidget->bgImageFileName = egIni->value("bgimagefile");

  // set font for text editors:
  // QFont sansFont("Courier", 10);
  // sifWindow->textEdit->setCurrentFont(sansFont);
  // solverLogWindow->textEdit->setCurrentFont(sansFont);

  // load definition files:
  updateSplash("Loading definitions...");
  loadDefinitions();

  // initialization ready:
  synchronizeMenuToState();
  setWindowTitle(tr("ElmerGUI"));
  setWindowIcon(QIcon(":/icons/Mesh3D.png"));
  finalizeSplash();
  setupSysTrayIcon();
}


// dtor...
//-----------------------------------------------------------------------------
MainWindow::~MainWindow()
{
}


// Set limits for dynamic editors, materials, bcs, etc...
//-----------------------------------------------------------------------------
void MainWindow::setDynamicLimits()
{
  // Values defined in "edf/egini.xml" that override default limits:
  if(egIni->isPresent("max_solvers")) {
    limit->setMaxSolvers(egIni->value("max_solvers").toInt());
    cout << "Max solvers: " << limit->maxSolvers() << endl;
  }

  if(egIni->isPresent("max_equations")) {
    limit->setMaxEquations(egIni->value("max_equations").toInt());
    cout << "Max equations: " << limit->maxEquations() << endl;
  }

  if(egIni->isPresent("max_materials")) {
    limit->setMaxMaterials(egIni->value("max_materials").toInt());
    cout << "Max materials: " << limit->maxMaterials() << endl;
  }

  if(egIni->isPresent("max_bodyforces")) {
    limit->setMaxBodyforces(egIni->value("max_bodyforces").toInt());
    cout << "Max bodyforces: " << limit->maxBodyforces() << endl;
  }

  if(egIni->isPresent("max_initialconditions")) {
    limit->setMaxInitialconditions(egIni->value("max_initialconditions").toInt());
    cout << "Max initialconditions: " << limit->maxInitialconditions() << endl;
  }

  if(egIni->isPresent("max_bcs")) {
    limit->setMaxBcs(egIni->value("max_bcs").toInt());
    cout << "Max bcs: " << limit->maxBcs() << endl;
  }

  if(egIni->isPresent("max_bodies")) {
    limit->setMaxBodies(egIni->value("max_bodies").toInt());
    cout << "Max bodies: " << limit->maxBodies() << endl;
  }

  if(egIni->isPresent("max_boundaries")) {
    limit->setMaxBoundaries(egIni->value("max_boundaries").toInt());
    cout << "Max boundaries: " << limit->maxBoundaries() << endl;
  }
}


// Create actions...
//-----------------------------------------------------------------------------
void MainWindow::createActions()
{
  // File -> Open file
  openAct = new QAction(QIcon(":/icons/document-open.png"), tr("&Open..."), this);
  openAct->setShortcut(tr("Ctrl+O"));
  openAct->setStatusTip(tr("Open geometry input file"));
  connect(openAct, SIGNAL(triggered()), 
	  this, SLOT(openSlot()));
  
  // File -> Load mesh...
  loadAct = new QAction(QIcon(":/icons/document-open-folder.png"), tr("&Load mesh..."), this);
  loadAct->setShortcut(tr("Ctrl+L"));
  loadAct->setStatusTip(tr("Load Elmer mesh files"));
  connect(loadAct, SIGNAL(triggered()), 
	  this, SLOT(loadSlot()));
  
  // File -> Load project...
  loadProjectAct = new QAction(QIcon(":/icons/document-import.png"), tr("Load &project..."), this);
  loadProjectAct->setStatusTip(tr("Load previously saved project"));
  connect(loadProjectAct, SIGNAL(triggered()), 
	  this, SLOT(loadProjectSlot()));

  // File -> Definitions...
  editDefinitionsAct = new QAction(QIcon(":/icons/games-config-custom.png"), tr("&Definitions..."), this);
  editDefinitionsAct->setStatusTip(tr("Load and edit Elmer sif definitions file"));
  connect(editDefinitionsAct, SIGNAL(triggered()), 
	  this, SLOT(editDefinitionsSlot()));

  // File -> Save...
  saveAct = new QAction(QIcon(":/icons/document-save.png"), tr("&Save..."), this);
  saveAct->setShortcut(tr("Ctrl+S"));
  saveAct->setStatusTip(tr("Save Elmer mesh and sif-files"));
  connect(saveAct, SIGNAL(triggered()), 
	  this, SLOT(saveSlot()));

  // File -> Save as...
  saveAsAct = new QAction(QIcon(":/icons/document-save-as.png"), tr("&Save as..."), this);
  saveAsAct->setStatusTip(tr("Save Elmer mesh and sif-files"));
  connect(saveAsAct, SIGNAL(triggered()), 
	  this, SLOT(saveAsSlot()));

  // File -> Save project...
  saveProjectAct = new QAction(QIcon(":/icons/document-export.png"), tr("&Save project..."), this);
  saveProjectAct->setStatusTip(tr("Save current project"));
  connect(saveProjectAct, SIGNAL(triggered()), 
	  this, SLOT(saveProjectSlot()));

  // File -> Save picture as...
  savePictureAct = new QAction(QIcon(":/icons/view-preview.png"), tr("&Save picture as..."), this);
  savePictureAct->setStatusTip(tr("Save picture in file"));
  connect(savePictureAct, SIGNAL(triggered()), 
	  this, SLOT(savePictureSlot()));

  // File -> Exit
  exitAct = new QAction(QIcon(":/icons/application-exit.png"), tr("E&xit"), this);
  exitAct->setShortcut(tr("Ctrl+Q"));
  exitAct->setStatusTip(tr("Exit"));
  connect(exitAct, SIGNAL(triggered()), 
	  this, SLOT(closeMainWindowSlot()));

  // Model -> Setup...
  modelSetupAct = new QAction(QIcon(), tr("Setup..."), this);
  modelSetupAct->setStatusTip(tr("Setup simulation environment"));
  connect(modelSetupAct, SIGNAL(triggered()), 
	  this, SLOT(modelSetupSlot()));

  // Model -> Equation...
  addEquationAct = new QAction(QIcon(), tr("Add..."), this);
  addEquationAct->setStatusTip(tr("Add a PDE-system to the equation list"));
  connect(addEquationAct, SIGNAL(triggered()), 
	  this, SLOT(addEquationSlot()));

  // Model -> Material...
  addMaterialAct = new QAction(QIcon(), tr("Add..."), this);
  addMaterialAct->setStatusTip(tr("Add a material set to the material list"));
  connect(addMaterialAct, SIGNAL(triggered()), 
	  this, SLOT(addMaterialSlot()));

  // Model -> Body force...
  addBodyForceAct = new QAction(QIcon(), tr("Add..."), this);
  addBodyForceAct->setStatusTip(tr("Add body forces..."));
  connect(addBodyForceAct, SIGNAL(triggered()), 
	  this, SLOT(addBodyForceSlot()));

  // Model -> Initial condition...
  addInitialConditionAct = new QAction(QIcon(), tr("Add..."), this);
  addInitialConditionAct->setStatusTip(tr("Add initial conditions..."));
  connect(addInitialConditionAct, SIGNAL(triggered()), 
	  this, SLOT(addInitialConditionSlot()));

  // Model -> Boundary condition...
  addBoundaryConditionAct = new QAction(QIcon(), tr("Add..."), this);
  addBoundaryConditionAct->setStatusTip(tr("Add boundary conditions..."));
  connect(addBoundaryConditionAct, SIGNAL(triggered()), 
	  this, SLOT(addBoundaryConditionSlot()));

  // Model -> Set body properties
  bodyEditAct = new QAction(QIcon(), tr("Set body properties"), this);
  bodyEditAct->setStatusTip(tr("Set body properties (equivalent to holding down the SHIFT key)"));
  connect(bodyEditAct, SIGNAL(triggered()), 
	  this, SLOT(bodyEditSlot()));

  // Model -> Set boundary conditions
  bcEditAct = new QAction(QIcon(), tr("Set boundary properties"), this);
  bcEditAct->setStatusTip(tr("Set boundary properties (equivalent to holding down the ALT key)"));
  connect(bcEditAct, SIGNAL(triggered()), 
	  this, SLOT(bcEditSlot()));

  // Model -> Summary...
  modelSummaryAct = new QAction(QIcon(), tr("Summary..."), this);
  modelSummaryAct->setStatusTip(tr("Model summary"));
  connect(modelSummaryAct, SIGNAL(triggered()), 
	  this, SLOT(modelSummarySlot()));

  // Model -> Clear
  modelClearAct = new QAction(QIcon(), tr("Clear all"), this);
  modelClearAct->setStatusTip(tr("Clear all model definitions"));
  connect(modelClearAct, SIGNAL(triggered()), 
	  this, SLOT(modelClearSlot()));

  // Edit -> Generate sif
  generateSifAct = new QAction(QIcon(""), tr("&Generate"), this);
  generateSifAct->setShortcut(tr("Ctrl+G"));
  generateSifAct->setStatusTip(tr("Genarete solver input file"));
  connect(generateSifAct, SIGNAL(triggered()), 
	  this, SLOT(generateSifSlot()));

  // Edit -> Solver input file...
  showsifAct = new QAction(QIcon(":/icons/document-properties.png"), tr("&Edit..."), this);
  showsifAct->setShortcut(tr("Ctrl+S"));
  showsifAct->setStatusTip(tr("Edit solver input file"));
  connect(showsifAct, SIGNAL(triggered()), 
	  this, SLOT(showsifSlot()));

  // Mesh -> Control
  meshcontrolAct = new QAction(QIcon(":/icons/configure.png"), tr("&Configure..."), this);
  meshcontrolAct->setShortcut(tr("Ctrl+C"));
  meshcontrolAct->setStatusTip(tr("Configure mesh generators"));
  connect(meshcontrolAct, SIGNAL(triggered()), 
	  this, SLOT(meshcontrolSlot()));

  // Mesh -> Remesh
  remeshAct = new QAction(QIcon(":/icons/edit-redo.png"), tr("&Remesh"), this);
  remeshAct->setShortcut(tr("Ctrl+R"));
  remeshAct->setStatusTip(tr("Remesh"));
  connect(remeshAct, SIGNAL(triggered()), this, SLOT(remeshSlot()));

  // Mesh -> Kill generator
  stopMeshingAct = new QAction(QIcon(":/icons/window-close.png"), 
			       tr("&Terminate meshing"), this);
  stopMeshingAct->setStatusTip(tr("Terminate mesh generator"));
  connect(stopMeshingAct, SIGNAL(triggered()), 
	  this, SLOT(stopMeshingSlot()));
  stopMeshingAct->setEnabled(false);

  // Mesh -> Divide surface
  surfaceDivideAct = new QAction(QIcon(":/icons/divide.png"), 
				 tr("&Divide surface..."), this);
  surfaceDivideAct->setStatusTip(tr("Divide surface by sharp edges"));
  connect(surfaceDivideAct, SIGNAL(triggered()), 
	  this, SLOT(surfaceDivideSlot()));

  // Mesh -> Unify surface
  surfaceUnifyAct = new QAction(QIcon(":/icons/unify.png"), tr("&Unify surface"), this);
  surfaceUnifyAct->setStatusTip(tr("Unify surface (merge selected)"));
  connect(surfaceUnifyAct, SIGNAL(triggered()), 
	  this, SLOT(surfaceUnifySlot()));

  // Mesh -> Divide edge
  edgeDivideAct = new QAction(QIcon(":/icons/divide-edge.png"), tr("&Divide edge..."), this);
  edgeDivideAct->setStatusTip(tr("Divide edge by sharp points"));
  connect(edgeDivideAct, SIGNAL(triggered()), 
	  this, SLOT(edgeDivideSlot()));

  // Mesh -> Unify edges
  edgeUnifyAct = new QAction(QIcon(":/icons/unify-edge.png"), tr("&Unify edge"), this);
  edgeUnifyAct->setStatusTip(tr("Unify edge (merge selected)"));
  connect(edgeUnifyAct, SIGNAL(triggered()), 
	  this, SLOT(edgeUnifySlot()));

  // Mesh -> Clean up
  cleanHangingSharpEdgesAct = new QAction(QIcon(""), tr("Clean up"), this);
  cleanHangingSharpEdgesAct->setStatusTip(tr("Removes hanging/orphan sharp edges (for visualization)"));
  connect(cleanHangingSharpEdgesAct, SIGNAL(triggered()), 
	  this, SLOT(cleanHangingSharpEdgesSlot()));

  // View -> Show surface mesh
  hidesurfacemeshAct = new QAction(QIcon(), tr("Surface mesh"), this);
  hidesurfacemeshAct->setStatusTip(tr("Show/hide surface mesh "
				      "(do/do not outline surface elements)"));
  connect(hidesurfacemeshAct, SIGNAL(triggered()), 
	  this, SLOT(hidesurfacemeshSlot()));

  // View -> Show volume mesh
  hidevolumemeshAct = new QAction(QIcon(), tr("Volume mesh"), this);
  hidevolumemeshAct->setStatusTip(tr("Show/hide volume mesh "
				      "(do/do not outline volume mesh edges)"));
  connect(hidevolumemeshAct, SIGNAL(triggered()), 
	  this, SLOT(hidevolumemeshSlot()));

  // View -> Show sharp edges
  hidesharpedgesAct = new QAction(QIcon(), tr("Sharp edges"), this);
  hidesharpedgesAct->setStatusTip(tr("Show/hide sharp edges"));
  connect(hidesharpedgesAct, SIGNAL(triggered()), 
	  this, SLOT(hidesharpedgesSlot()));

  // View -> Compass
  viewCoordinatesAct = new QAction(QIcon(), tr("Compass"), this);
  viewCoordinatesAct->setStatusTip(tr("View coordinates "
				      "(RGB=XYZ modulo translation)"));
  connect(viewCoordinatesAct, SIGNAL(triggered()), 
	  this, SLOT(viewCoordinatesSlot()));

  // View -> Select all surfaces
  selectAllSurfacesAct = new QAction(QIcon(), tr("Select all surfaces"), this);
  selectAllSurfacesAct->setStatusTip(tr("Select all surfaces"));
  connect(selectAllSurfacesAct, SIGNAL(triggered()), 
	  this, SLOT(selectAllSurfacesSlot()));

  // View -> Select all edges
  selectAllEdgesAct = new QAction(QIcon(), tr("Select all edges"), this);
  selectAllEdgesAct->setStatusTip(tr("Select all edges"));
  connect(selectAllEdgesAct, SIGNAL(triggered()), 
	  this, SLOT(selectAllEdgesSlot()));

  // View -> Select defined edges
  selectDefinedEdgesAct = new QAction(QIcon(), tr("Select defined edges"), this);
  selectDefinedEdgesAct->setStatusTip(tr("Select defined edges"));
  connect(selectDefinedEdgesAct, SIGNAL(triggered()), 
	  this, SLOT(selectDefinedEdgesSlot()));

  // View -> Select defined surfaces
  selectDefinedSurfacesAct = new QAction(QIcon(), tr("Select defined surfaces"), this);
  selectDefinedSurfacesAct->setStatusTip(tr("Select defined surfaces"));
  connect(selectDefinedSurfacesAct, SIGNAL(triggered()), 
	  this, SLOT(selectDefinedSurfacesSlot()));

  // View -> Hide/show selected
  hideselectedAct = new QAction(QIcon(), tr("&Hide/show selected"), this);
  hideselectedAct->setShortcut(tr("Ctrl+H"));
  hideselectedAct->setStatusTip(tr("Show/hide selected objects"));
  connect(hideselectedAct, SIGNAL(triggered()), 
	  this, SLOT(hideselectedSlot()));

  // View -> Shade model -> Flat
  flatShadeAct = new QAction(QIcon(), tr("Flat"), this);
  flatShadeAct->setStatusTip(tr("Set shade model to flat"));
  connect(flatShadeAct, SIGNAL(triggered()), 
	  this, SLOT(flatShadeSlot()));

  // View -> Show surface numbers
  showSurfaceNumbersAct = new QAction(QIcon(), tr("Surface element numbers"), this);
  showSurfaceNumbersAct->setStatusTip(tr("Show surface element numbers "
				      "(Show the surface element numbering)"));
  connect(showSurfaceNumbersAct, SIGNAL(triggered()), 
	  this, SLOT(showSurfaceNumbersSlot()));

  // View -> Show edge numbers
  showEdgeNumbersAct = new QAction(QIcon(), tr("Edge element numbers"), this);
  showEdgeNumbersAct->setStatusTip(tr("Show edge element numbers "
				      "(Show the node element numbering)"));
  connect(showEdgeNumbersAct, SIGNAL(triggered()), 
	  this, SLOT(showEdgeNumbersSlot()));

  // View -> Show node numbers
  showNodeNumbersAct = new QAction(QIcon(), tr("Node numbers"), this);
  showNodeNumbersAct->setStatusTip(tr("Show node numbers "
				      "(Show the node numbers)"));
  connect(showNodeNumbersAct, SIGNAL(triggered()), 
	  this, SLOT(showNodeNumbersSlot()));
  
  // View -> Show boundray index
  showBoundaryIndexAct = new QAction(QIcon(), tr("Boundary index"), this);
  showBoundaryIndexAct->setStatusTip(tr("Show boundary index"));
  connect(showBoundaryIndexAct, SIGNAL(triggered()), 
	  this, SLOT(showBoundaryIndexSlot()));
  
  // View -> Show body index
  showBodyIndexAct = new QAction(QIcon(), tr("Body index"), this);
  showBodyIndexAct->setStatusTip(tr("Show body index"));
  connect(showBodyIndexAct, SIGNAL(triggered()), 
	  this, SLOT(showBodyIndexSlot()));

  // View -> Colors -> GL controls
  glControlAct = new QAction(QIcon(), tr("GL controls..."), this);
  glControlAct->setStatusTip(tr("Control GL parameters for lights and materials"));
  connect(glControlAct, SIGNAL(triggered()), 
	  this, SLOT(glControlSlot()));
  
  // View -> Colors -> Background
  chooseBGColorAct = new QAction(QIcon(), tr("Background..."), this);
  chooseBGColorAct->setStatusTip(tr("Set background color"));
  connect(chooseBGColorAct, SIGNAL(triggered()), 
	  this, SLOT(backgroundColorSlot()));
  
  // View -> Colors -> Surface elements
  chooseSurfaceColorAct = new QAction(QIcon(), tr("Surface elements..."), this);
  chooseSurfaceColorAct->setStatusTip(tr("Set surface color"));
  connect(chooseSurfaceColorAct, SIGNAL(triggered()), 
	  this, SLOT(surfaceColorSlot()));
  
  // View -> Colors -> Edge elements
  chooseEdgeColorAct = new QAction(QIcon(), tr("Edge elements..."), this);
  chooseEdgeColorAct->setStatusTip(tr("Set edge color"));
  connect(chooseEdgeColorAct, SIGNAL(triggered()), 
	  this, SLOT(edgeColorSlot()));
  
  // View -> Colors -> Surface mesh
  chooseSurfaceMeshColorAct = new QAction(QIcon(), tr("Surface mesh..."), this);
  chooseSurfaceMeshColorAct->setStatusTip(tr("Set surface mesh color"));
  connect(chooseSurfaceMeshColorAct, SIGNAL(triggered()), 
	  this, SLOT(surfaceMeshColorSlot()));
  
  // View -> Colors -> Sharp edges
  chooseSharpEdgeColorAct = new QAction(QIcon(), tr("Sharp edges..."), this);
  chooseSharpEdgeColorAct->setStatusTip(tr("Set sharp edge color"));
  connect(chooseSharpEdgeColorAct, SIGNAL(triggered()), 
	  this, SLOT(sharpEdgeColorSlot()));
  
  // View -> Colors -> Boundaries
  showBoundaryColorAct = new QAction(QIcon(), tr("Boundaries"), this);
  showBoundaryColorAct->setStatusTip(tr("Visualize different boundary parts with color patches"));
  connect(showBoundaryColorAct, SIGNAL(triggered()), 
	  this, SLOT(colorizeBoundarySlot()));
  
  // View -> Colors -> Bodies
  showBodyColorAct = new QAction(QIcon(), tr("Bodies"), this);
  showBodyColorAct->setStatusTip(tr("Visualize different body with color patches"));
  connect(showBodyColorAct, SIGNAL(triggered()), 
	  this, SLOT(colorizeBodySlot()));
  
  // View -> Shade model -> Smooth
  smoothShadeAct = new QAction(QIcon(), tr("Smooth"), this);
  smoothShadeAct->setStatusTip(tr("Set shade model to smooth"));
  connect(smoothShadeAct, SIGNAL(triggered()), 
	  this, SLOT(smoothShadeSlot()));

  // View -> Show all
  showallAct = new QAction(QIcon(), tr("Show all"), this);
  showallAct->setStatusTip(tr("Show all objects"));
  connect(showallAct, SIGNAL(triggered()), 
	  this, SLOT(showallSlot()));

  // View -> Reset model view
  resetAct = new QAction(QIcon(), tr("Reset model view"), this);
  resetAct->setStatusTip(tr("Reset model view"));
  connect(resetAct, SIGNAL(triggered()), 
	  this, SLOT(resetSlot()));

  // View -> Reset model view
  showCadModelAct = new QAction(QIcon(), tr("Cad model..."), this);
  showCadModelAct->setStatusTip(tr("Displays the cad model in a separate window"));
  connect(showCadModelAct, SIGNAL(triggered()), 
	  this, SLOT(showCadModelSlot()));

  // Solver -> Parallel settings
  parallelSettingsAct = new QAction(QIcon(), tr("Parallel settings..."), this);
  parallelSettingsAct->setStatusTip(tr("Choose parametes and methods for parallel solution"));
  connect(parallelSettingsAct, SIGNAL(triggered()), 
	  this, SLOT(parallelSettingsSlot()));

  // Solver -> Run solver
  runsolverAct = new QAction(QIcon(":/icons/Solver.png"), tr("Start solver"), this);
  runsolverAct->setStatusTip(tr("Run ElmerSolver"));
  connect(runsolverAct, SIGNAL(triggered()), 
	  this, SLOT(runsolverSlot()));

  // Solver -> Kill solver
  killsolverAct = new QAction(QIcon(":/icons/window-close.png"), tr("Kill solver"), this);
  killsolverAct->setStatusTip(tr("Kill ElmerSolver"));
  connect(killsolverAct, SIGNAL(triggered()), 
	  this, SLOT(killsolverSlot()));
  killsolverAct->setEnabled(false);

  // Solver -> Show convergence
  showConvergenceAct = new QAction(QIcon(iconChecked), tr("Show convergence"), this);
  showConvergenceAct->setStatusTip(tr("Show/hide convergence plot"));
  connect(showConvergenceAct, SIGNAL(triggered()), this, SLOT(showConvergenceSlot()));

  // Solver -> Post process
  resultsAct = new QAction(QIcon(":/icons/Post.png"), tr("Start postprocessor"), this);
  resultsAct->setStatusTip(tr("Run ElmerPost for visualization"));
  connect(resultsAct, SIGNAL(triggered()), 
	  this, SLOT(resultsSlot()));

  // Solver -> Kill post process
  killresultsAct = new QAction(QIcon(":/icons/window-close.png"), tr("Kill postprocessor"), this);
  killresultsAct->setStatusTip(tr("Kill ElmerPost"));
  connect(killresultsAct, SIGNAL(triggered()), this, SLOT(killresultsSlot()));
  killresultsAct->setEnabled(false);

  // Solver -> Compiler...
  compileSolverAct = new QAction(QIcon(""), tr("Compiler..."), this);
  compileSolverAct->setStatusTip(tr("Compile Elmer specific source code (f90) into a shared library (dll)"));
  connect(compileSolverAct, SIGNAL(triggered()), 
	  this, SLOT(compileSolverSlot()));

  // Help -> About
  aboutAct = new QAction(QIcon(":/icons/help-about.png"), tr("About..."), this);
  aboutAct->setStatusTip(tr("Information about the program"));
  connect(aboutAct, SIGNAL(triggered()), 
	  this, SLOT(showaboutSlot()));

#if WIN32
#else
  compileSolverAct->setEnabled(false);
#endif

  if(egIni->isSet("bgimage"))
    chooseBGColorAct->setEnabled(false);
    
}


// Create menus...
//-----------------------------------------------------------------------------
void MainWindow::createMenus()
{
  // File menu
  fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(openAct);
  fileMenu->addAction(loadAct);
  fileMenu->addAction(loadProjectAct);
  fileMenu->addSeparator();
  fileMenu->addAction(editDefinitionsAct);
  fileMenu->addSeparator();
  fileMenu->addAction(saveAct);
  fileMenu->addAction(saveAsAct);
  fileMenu->addAction(saveProjectAct);
  fileMenu->addSeparator();
  fileMenu->addAction(savePictureAct);
  fileMenu->addSeparator();
  fileMenu->addAction(exitAct);

  // Mesh menu
  meshMenu = menuBar()->addMenu(tr("&Mesh"));
  meshMenu->addAction(meshcontrolAct);
  meshMenu->addAction(remeshAct);
  meshMenu->addAction(stopMeshingAct);
  meshMenu->addSeparator();
  meshMenu->addAction(surfaceDivideAct);
  meshMenu->addAction(surfaceUnifyAct);
  meshMenu->addSeparator();
  meshMenu->addAction(edgeDivideAct);
  meshMenu->addAction(edgeUnifyAct);
  meshMenu->addSeparator();
  meshMenu->addAction(cleanHangingSharpEdgesAct);

  // Model menu
  modelMenu = menuBar()->addMenu(tr("&Model"));

  modelMenu->addAction(modelSetupAct);
  modelMenu->addSeparator();

  equationMenu = modelMenu->addMenu(tr("Equation"));
  equationMenu->addAction(addEquationAct);
  equationMenu->addSeparator();
  connect(equationMenu, SIGNAL(triggered(QAction*)), 
	  this, SLOT(equationSelectedSlot(QAction*)));

  modelMenu->addSeparator();
  materialMenu = modelMenu->addMenu(tr("Material"));
  materialMenu->addAction(addMaterialAct);
  materialMenu->addSeparator();
  connect(materialMenu, SIGNAL(triggered(QAction*)), 
	  this, SLOT(materialSelectedSlot(QAction*)));

  modelMenu->addSeparator();
  bodyForceMenu = modelMenu->addMenu(tr("Body force"));
  bodyForceMenu->addAction(addBodyForceAct);
  bodyForceMenu->addSeparator();
  connect(bodyForceMenu, SIGNAL(triggered(QAction*)), 
	  this, SLOT(bodyForceSelectedSlot(QAction*)));
  
  modelMenu->addSeparator();
  initialConditionMenu = modelMenu->addMenu(tr("Initial condition"));
  initialConditionMenu->addAction(addInitialConditionAct);
  initialConditionMenu->addSeparator();
  connect(initialConditionMenu, SIGNAL(triggered(QAction*)), 
	  this, SLOT(initialConditionSelectedSlot(QAction*)));
  
  modelMenu->addSeparator();
  boundaryConditionMenu = modelMenu->addMenu(tr("Boundary condition"));
  boundaryConditionMenu->addAction(addBoundaryConditionAct);
  boundaryConditionMenu->addSeparator();
  connect(boundaryConditionMenu, SIGNAL(triggered(QAction*)), 
	  this, SLOT(boundaryConditionSelectedSlot(QAction*)));
  
  modelMenu->addSeparator();
  modelMenu->addAction(bodyEditAct);
  modelMenu->addAction(bcEditAct);
  modelMenu->addSeparator();
  modelMenu->addAction(modelSummaryAct);
  modelMenu->addSeparator();
  modelMenu->addAction(modelClearAct);
  modelMenu->addSeparator();

  // View menu
  viewMenu = menuBar()->addMenu(tr("&View"));  
  viewMenu->addAction(hidesurfacemeshAct);
  viewMenu->addAction(hidevolumemeshAct);
  viewMenu->addAction(hidesharpedgesAct);
  viewMenu->addAction(viewCoordinatesAct);
  viewMenu->addSeparator();
  viewMenu->addAction(selectAllSurfacesAct);
  viewMenu->addAction(selectAllEdgesAct);
  // Momentarily disabled (see comment *** TODO *** below):
  // viewMenu->addSeparator();
  // viewMenu->addAction(selectDefinedEdgesAct);
  // viewMenu->addAction(selectDefinedSurfacesAct);
  viewMenu->addSeparator();
  viewMenu->addAction(hideselectedAct);
  viewMenu->addSeparator();
  shadeMenu = viewMenu->addMenu(tr("Shade model"));
  shadeMenu->addAction(flatShadeAct);
  shadeMenu->addAction(smoothShadeAct);
  viewMenu->addSeparator();
  numberingMenu = viewMenu->addMenu(tr("Numbering"));
  numberingMenu->addAction(showSurfaceNumbersAct);
  numberingMenu->addAction(showEdgeNumbersAct);
  numberingMenu->addAction(showNodeNumbersAct);
  numberingMenu->addSeparator();
  numberingMenu->addAction(showBoundaryIndexAct);
  numberingMenu->addAction(showBodyIndexAct);
  viewMenu->addSeparator();
  colorizeMenu = viewMenu->addMenu(tr("Lights and colors"));
  colorizeMenu->addAction(glControlAct);
  colorizeMenu->addSeparator();
  colorizeMenu->addAction(chooseBGColorAct);
  colorizeMenu->addSeparator();
  colorizeMenu->addAction(chooseSurfaceColorAct);
  colorizeMenu->addAction(chooseEdgeColorAct);
  colorizeMenu->addSeparator();
  colorizeMenu->addAction(chooseSurfaceMeshColorAct);
  colorizeMenu->addAction(chooseSharpEdgeColorAct);
  colorizeMenu->addSeparator();
  colorizeMenu->addAction(showBoundaryColorAct);
  colorizeMenu->addAction(showBodyColorAct);
  viewMenu->addSeparator();
  viewMenu->addAction(showallAct);
  viewMenu->addAction(resetAct);
#ifdef OCC62
  viewMenu->addSeparator();
  viewMenu->addAction(showCadModelAct);
#endif

  // Edit menu
  editMenu = menuBar()->addMenu(tr("&Sif"));
  editMenu->addAction(generateSifAct);
  editMenu->addSeparator();
  editMenu->addAction(showsifAct);

  //  SolverMenu
  solverMenu = menuBar()->addMenu(tr("&Run"));
  solverMenu->addAction(parallelSettingsAct);
  solverMenu->addSeparator();
  solverMenu->addAction(runsolverAct);
  solverMenu->addAction(killsolverAct);
  solverMenu->addAction(showConvergenceAct);
  solverMenu->addSeparator();
  solverMenu->addAction(resultsAct);
  solverMenu->addAction(killresultsAct);
  // Momentarily disabled:
  solverMenu->addSeparator();
  solverMenu->addAction(compileSolverAct);

  // Help menu
  helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(aboutAct);

  // Sys tray context menu
  sysTrayMenu = new QMenu;
  sysTrayMenu->addAction(modelSummaryAct);
  sysTrayMenu->addSeparator();
  sysTrayMenu->addAction(stopMeshingAct);
  sysTrayMenu->addSeparator();
  sysTrayMenu->addAction(killsolverAct);
  sysTrayMenu->addAction(killresultsAct);
  sysTrayMenu->addSeparator();
  sysTrayMenu->addAction(aboutAct);
  sysTrayMenu->addSeparator();
  sysTrayMenu->addAction(exitAct);

  // Disable unavailable external components:
  //------------------------------------------
  if(!egIni->isSet("checkexternalcomponents"))
     return;

  QProcess testProcess;
  QStringList args;

  cout << "Checking for ElmerSolver... ";
  updateSplash("Checking for ElmerSolver...");
  args << "-v";
  testProcess.start("ElmerSolver", args);
  if(!testProcess.waitForStarted()) {
    logMessage("no - disabling solver features");
    runsolverAct->setEnabled(false);
    showConvergenceAct->setEnabled(false);
    killsolverAct->setEnabled(false);
  } else {
    cout << "yes" << endl;
  }
  testProcess.waitForFinished(2000);

  cout << "Checking for ElmerPost... ";
  updateSplash("Checking for ElmerPost...");
  args << "-v";
  testProcess.start("ElmerPost", args);
  if(!testProcess.waitForStarted()) {
    logMessage("no - disabling postprocessing features");
    resultsAct->setEnabled(false);
    killresultsAct->setEnabled(false);
  } else {
    cout << "yes" << endl;
  }
  testProcess.waitForFinished(2000);

  cout << "Checking for ElmerGrid... ";
  updateSplash("Checking for ElmerGrid...");
  testProcess.start("ElmerGrid");
  if(!testProcess.waitForStarted()) {
    logMessage("no - disabling parallel features");
    parallelSettingsAct->setEnabled(false);
  } else {
    cout << "yes" << endl;
  }
  testProcess.waitForFinished(2000);

  cout << "Checking for ElmerSolver_mpi... ";
  updateSplash("Checking for ElmerSolver_mpi...");
  args << "-v";
  testProcess.start("ElmerSolver_mpi", args);
  if(!testProcess.waitForStarted()) {
    logMessage("no - disabling parallel features");
    parallelSettingsAct->setEnabled(false);
  } else {
    cout << "yes" << endl;
  }
  testProcess.waitForFinished(2000);

}



// Create tool bars...
//-----------------------------------------------------------------------------
void MainWindow::createToolBars()
{
  // File toolbar
  fileToolBar = addToolBar(tr("&File"));
  fileToolBar->addAction(openAct);
  fileToolBar->addAction(loadAct);
  fileToolBar->addAction(loadProjectAct);
  fileToolBar->addSeparator();
  fileToolBar->addAction(saveAct);
  fileToolBar->addAction(saveAsAct);
  fileToolBar->addAction(saveProjectAct);
  fileToolBar->addSeparator();
  fileToolBar->addAction(savePictureAct);

  // Edit toolbar
  editToolBar = addToolBar(tr("&Edit"));
  editToolBar->addAction(showsifAct);

  // Mesh toolbar
  meshToolBar = addToolBar(tr("&Mesh"));
  meshToolBar->addAction(meshcontrolAct);
  meshToolBar->addAction(remeshAct);
  // meshToolBar->addAction(stopMeshingAct);
  meshToolBar->addSeparator();
  meshToolBar->addAction(surfaceDivideAct);
  meshToolBar->addAction(surfaceUnifyAct);
  meshToolBar->addSeparator();
  meshToolBar->addAction(edgeDivideAct);
  meshToolBar->addAction(edgeUnifyAct);

  // Solver toolbar
  solverToolBar = addToolBar(tr("&Solver"));
  solverToolBar->addAction(runsolverAct);
  solverToolBar->addAction(resultsAct);

  if(egIni->isSet("hidetoolbars")) {
    fileToolBar->hide();
    editToolBar->hide();
    meshToolBar->hide();
    solverToolBar->hide();
  }
}


// Create status bar...
//-----------------------------------------------------------------------------
void MainWindow::createStatusBar()
{
  statusBar()->showMessage(tr("Ready"));
}


//*****************************************************************************
//
//                                File MENU
//
//*****************************************************************************


// File -> Open...
//-----------------------------------------------------------------------------
void MainWindow::openSlot()
{
  QString fileName = QFileDialog::getOpenFileName(this);

  if (!fileName.isEmpty()) {
    
    QFileInfo fi(fileName);
    QString absolutePath = fi.absolutePath();
    QDir::setCurrent(absolutePath);
    
  } else {
    
    logMessage("Unable to open file: file name is empty");
    return;

  }

  operation_t *p = operation.next, *q;

  while(p != NULL) {
    if(p->select_set != NULL)
      delete [] p->select_set;

    q = p->next;

    if(p != NULL)
      delete p;

    p = q;
  }

  operations = 0;
  operation.next = NULL;

  saveDirName = "";
  readInputFile(fileName);
  remeshSlot();
}



// Read input file and populate mesh generator's input structures:
//-----------------------------------------------------------------------------
void MainWindow::readInputFile(QString fileName)
{
  char cs[1024];

  QFileInfo fi(fileName);
  QString absolutePath = fi.absolutePath();
  QString baseName = fi.baseName();
  QString fileSuffix = fi.suffix();
  QString baseFileName = absolutePath + "/" + baseName;
  sprintf(cs, "%s", (const char*)(baseFileName.toAscii()));

  activeGenerator = GEN_UNKNOWN;
  tetlibInputOk = false;
  nglibInputOk = false;

  // Choose generator according to fileSuffix:
  //------------------------------------------
  if((fileSuffix == "smesh") || 
     (fileSuffix == "poly")) {
    
    if(!tetlibPresent) {
      logMessage("unable to mesh - tetlib unavailable");
      return;
    }

    activeGenerator = GEN_TETLIB;
    cout << "Selected tetlib for smesh/poly-format" << endl;

    in->deinitialize();
    in->initialize();
    in->load_poly(cs);

    tetlibInputOk = true;

  } else if(fileSuffix == "off") {

    if(!tetlibPresent) {
      logMessage("unable to mesh - tetlib unavailable");
      return;
    }

    activeGenerator = GEN_TETLIB;
    cout << "Selected tetlib for off-format" << endl;

    in->deinitialize();
    in->initialize();
    in->load_off(cs);

    tetlibInputOk = true;

  } else if(fileSuffix == "ply") {

    if(!tetlibPresent) {
      logMessage("unable to mesh - tetlib unavailable");
      return;
    }

    activeGenerator = GEN_TETLIB;
    cout << "Selected tetlib for ply-format" << endl;

    in->deinitialize();
    in->initialize();
    in->load_ply(cs);

    tetlibInputOk = true;

  } else if(fileSuffix == "mesh") {

    if(!tetlibPresent) {
      logMessage("unable to mesh - tetlib unavailable");
      return;
    }

    activeGenerator = GEN_TETLIB;
    cout << "Selected tetlib for mesh-format" << endl;

    in->deinitialize();
    in->initialize();
    in->load_medit(cs);
    
    tetlibInputOk = true;
    
#ifdef OCC62
  } else if( (fileSuffix == "stl") || 
	     (fileSuffix == "brep") ||
	     (fileSuffix == "step") ||
	     (fileSuffix == "stp") ) {
#else
  } else if(  fileSuffix == "stl") {
#endif
    

#ifdef OCC62
    // Convert to STL:
    //----------------
    bool converted = cadView->convertToSTL(fileName, fileSuffix);
    if(converted) {
      fileName += ".stl";
      baseFileName += "." + fileSuffix;
      sprintf(cs, "%s", (const char*)(baseFileName.toAscii()));
    }
#endif
    
    // for stl there are two alternative generators:
    if(meshControl->generatorType == GEN_NGLIB) {
      
      cout << "nglib" << endl;
      
      if(!nglibPresent) {
	logMessage("unable to mesh - nglib unavailable");
	return;
      }
      
      activeGenerator = GEN_NGLIB;
      cout << "Selected nglib for stl-format" << endl;

      nglibAPI->Ng_Init();
      
      nggeom 
	= nglibAPI->Ng_STL_LoadGeometry((const char*)(fileName.toAscii()), 0);
      
      if(!nggeom) {
	logMessage("Ng_STL_LoadGeometry failed");
	return;
      }
      
      int rv = nglibAPI->Ng_STL_InitSTLGeometry(nggeom);
      cout << "InitSTLGeometry: NG_result=" << rv << endl;
      cout.flush();
      
      nglibInputOk = true;
      
    } else {

      if(!tetlibPresent) {
	logMessage("unable to mesh - tetlib unavailable");
	return;
      }
      
      activeGenerator = GEN_TETLIB;
      cout << "Selected tetlib for stl-format" << endl;
      
      in->deinitialize();
      in->initialize();
      in->load_stl(cs);
      
      tetlibInputOk = true;
      
    }

  } else if((fileSuffix == "grd") ||
	    (fileSuffix == "FDNEUT") ||
	    (fileSuffix == "msh") ||
	    (fileSuffix == "mphtxt") ||
	    (fileSuffix == "inp") ||    
	    (fileSuffix == "unv") ||
            (fileSuffix == "plt")) {

    activeGenerator = GEN_ELMERGRID;
    cout << "Selected elmergrid" << endl;

    int errstat = elmergridAPI->loadElmerMeshStructure((const char*)(fileName.toAscii()));
    
    if (errstat)
      logMessage("loadElmerMeshStructure failed!");

    return;

  } else {

    logMessage("Unable to open file: file type unknown");
    activeGenerator = GEN_UNKNOWN;
    return;

  }
}
  


// Populate elmer's mesh structure and make GL-lists (tetlib):
//-----------------------------------------------------------------------------
void MainWindow::makeElmerMeshFromTetlib()
{
  meshutils->clearMesh(glWidget->mesh);

  glWidget->mesh = tetlibAPI->createElmerMeshStructure();

  glWidget->rebuildLists();

  logMessage("Input file processed");
}



// Populate elmer's mesh structure and make GL-lists (nglib):
//-----------------------------------------------------------------------------
void MainWindow::makeElmerMeshFromNglib()
{
  meshutils->clearMesh(glWidget->mesh);
  nglibAPI->ngmesh = this->ngmesh;
  glWidget->mesh = nglibAPI->createElmerMeshStructure();

  glWidget->rebuildLists();

  logMessage("Input file processed");
}


// File -> Load mesh...
//-----------------------------------------------------------------------------
void MainWindow::loadSlot()
{
  QString dirName = QFileDialog::getExistingDirectory(this);

  if (!dirName.isEmpty()) {

    logMessage("Loading from directory " + dirName);

  } else {

    logMessage("Unable to load mesh: directory undefined");
    return;

  }
  
  loadElmerMesh(dirName);
}



// Import mesh files in elmer-format:
//-----------------------------------------------------------------------------
void MainWindow::loadElmerMesh(QString dirName)
{
  logMessage("Loading elmer mesh files");

  QFile file;
  QDir::setCurrent(dirName);

  // Header:
  file.setFileName("mesh.header");
  if(!file.exists()) {
    logMessage("mesh.header does not exist");
    return;
  }

  file.open(QIODevice::ReadOnly);
  QTextStream mesh_header(&file);

  int nodes, elements, surfaces, types, type, ntype;

  mesh_header >> nodes >> elements >> surfaces;
  mesh_header >> types;

  int elements_zero_d = 0;
  int elements_one_d = 0;
  int elements_two_d = 0;
  int elements_three_d = 0;
  
  for(int i=0; i<types; i++) {
    mesh_header >> type >> ntype;
    
    switch(type/100) {
    case 1:
      elements_zero_d += ntype;
      break;
    case 2:
      elements_one_d += ntype;
      break;
    case 3:
    case 4:
      elements_two_d += ntype;
      break;
    case 5:
    case 6:
    case 7:
    case 8:
      elements_three_d += ntype;
      break;
    default:
      cout << "Unknown element family (possibly not implamented)" << endl;
      cout.flush();
      exit(0);
    }
  }
  
  file.close();

  cout << "Summary:" << endl;
  cout << "Nodes: " << nodes << endl;
  cout << "point elements: " << elements_zero_d << endl;
  cout << "edge elements: " << elements_one_d << endl;
  cout << "surface elements: " << elements_two_d << endl;
  cout << "volume elements: " << elements_three_d << endl;
  cout.flush();

  // Allocate the new mesh:
  meshutils->clearMesh(glWidget->mesh);
  glWidget->mesh = new mesh_t;
  mesh_t *mesh = glWidget->mesh;

  // Set mesh dimension (??? is this correct ???):
  mesh->dim = 0;

  if(elements_one_d > 0)
    mesh->dim = 1;

  if(elements_two_d > 0)
    mesh->dim = 2;

  if(elements_three_d > 0)
    mesh->dim = 3;

  mesh->nodes = nodes;
  mesh->node = new node_t[nodes];

  mesh->points = elements_zero_d;
  mesh->point = new point_t[mesh->points];

  mesh->edges = elements_one_d;
  mesh->edge = new edge_t[mesh->edges];

  mesh->surfaces = elements_two_d;
  mesh->surface = new surface_t[mesh->surfaces];

  mesh->elements = elements_three_d;
  mesh->element = new element_t[mesh->elements];

  // Nodes:
  file.setFileName("mesh.nodes");
  if(!file.exists()) {
    logMessage("mesh.nodes does not exist");
    return;
  }

  file.open(QIODevice::ReadOnly);
  QTextStream mesh_node(&file);
  
  int number, index;
  double x, y, z;

  for(int i=0; i<nodes; i++) {
    node_t *node = &mesh->node[i];
    mesh_node >> number >> index >> x >> y >> z;
    node->x[0] = x;
    node->x[1] = y;
    node->x[2] = z;
    node->index = index;
  }

  file.close();  

  // Elements:
  file.setFileName("mesh.elements");
  if(!file.exists()) {
    logMessage("mesh.elements does not exist");
    meshutils->clearMesh(mesh);
    return;
  }

  file.open(QIODevice::ReadOnly);
  QTextStream mesh_elements(&file);

  int current_point = 0;
  int current_edge = 0;
  int current_surface = 0;
  int current_element = 0;

  point_t *point = NULL;
  edge_t *edge = NULL;
  surface_t *surface = NULL;
  element_t *element = NULL;

  for(int i=0; i<elements; i++) {
    mesh_elements >> number >> index >> type;

    switch(type/100) {
    case 1:
      point = &mesh->point[current_point++];
      point->nature = PDE_BULK;
      point->index = index;
      point->code = type;
      point->nodes = point->code % 100;
      point->node = new int[point->nodes];
      for(int j=0; j < point->nodes; j++) {
	mesh_elements >> point->node[j];
	point->node[j] -= 1;
      }
      point->edges = 2;
      point->edge = new int[point->edges];
      point->edge[0] = -1;
      point->edge[1] = -1;
      break;

    case 2:
      edge = &mesh->edge[current_edge++];
      edge->nature = PDE_BULK;
      edge->index = index;
      edge->code = type;
      edge->nodes = edge->code % 100;
      edge->node = new int[edge->nodes];
      for(int j=0; j < edge->nodes; j++) {
	mesh_elements >> edge->node[j];
	edge->node[j] -= 1;
      }
      edge->surfaces = 0;
      edge->surface = new int[edge->surfaces];
      edge->surface[0] = -1;
      edge->surface[1] = -1;

      break;

    case 3:
    case 4:
      surface = &mesh->surface[current_surface++];
      surface->nature = PDE_BULK;
      surface->index = index;
      surface->code = type;
      surface->nodes = surface->code % 100;
      surface->node = new int[surface->nodes];
      for(int j=0; j < surface->nodes; j++) {
	mesh_elements >> surface->node[j];
	surface->node[j] -= 1;
      }      
      surface->edges = (int)(surface->code/100);
      surface->edge = new int[surface->edges];
      for(int j=0; j<surface->edges; j++)
	surface->edge[j] = -1;
      surface->elements = 2;
      surface->element = new int[surface->elements];
      surface->element[0] = -1;
      surface->element[1] = -1;

      break;

    case 5:
    case 6:
    case 7:
    case 8:
      element = &mesh->element[current_element++];
      element->nature = PDE_BULK;
      element->index = index;
      element->code = type;
      element->nodes = element->code % 100;
      element->node = new int[element->nodes];
      for(int j=0; j < element->nodes; j++) {
	mesh_elements >> element->node[j];
	element->node[j] -= 1;
      }
      break;

    default:
      cout << "Unknown element type (possibly not implemented" << endl;
      cout.flush();
      exit(0);
      break;
    }

  }

  file.close();

  // Boundary elements:
  file.setFileName("mesh.boundary");
  if(!file.exists()) {
    logMessage("mesh.boundary does not exist");
    meshutils->clearMesh(mesh);
    return;
  }

  file.open(QIODevice::ReadOnly);
  QTextStream mesh_boundary(&file);

  int parent0, parent1;
  for(int i=0; i<surfaces; i++) {
    mesh_boundary >> number >> index >> parent0 >> parent1 >> type;

    switch(type/100) {
    case 1:
      point = &mesh->point[current_point++];
      point->nature = PDE_BOUNDARY;
      point->index = index;
      point->edges = 2;
      point->edge = new int[point->edges];
      point->edge[0] = parent0-1;
      point->edge[1] = parent0-1;
      point->code = type;
      point->nodes = point->code % 100;
      point->node = new int[point->nodes];
      for(int j=0; j < point->nodes; j++) {
	mesh_elements >> point->node[j];
	point->node[j] -= 1;
      }
      break;

    case 2:
      edge = &mesh->edge[current_edge++];
      edge->nature = PDE_BOUNDARY;
      edge->index = index;
      edge->surfaces = 2;
      edge->surface = new int[edge->surfaces];
      edge->surface[0] = parent0-1;
      edge->surface[1] = parent1-1;
      edge->code = type;
      edge->nodes = edge->code % 100;
      edge->node = new int[edge->nodes];      
      for(int j=0; j < edge->nodes; j++) {
	mesh_boundary >> edge->node[j];
	edge->node[j] -= 1;
      }

      break;

    case 3:
    case 4:
      surface = &mesh->surface[current_surface++];
      surface->nature = PDE_BOUNDARY;
      surface->index = index;
      surface->elements = 2;
      surface->element = new int[surface->elements];
      surface->element[0] = parent0-1;
      surface->element[1] = parent1-1;
      surface->code = type;
      surface->nodes = surface->code % 100;
      surface->node = new int[surface->nodes];
      for(int j=0; j < surface->nodes; j++) {
	mesh_boundary >> surface->node[j];
	surface->node[j] -= 1;
      }
      surface->edges = (int)(surface->code/100);
      surface->edge = new int[surface->edges];
      for(int j=0; j<surface->edges; j++)
	surface->edge[j] = -1;      
      
      break;

    case 5:
    case 6:
    case 7:
    case 8:
      // can't be boundary elements
      break;

    default:
      break;
    }
  }

  file.close();

  // Compute bounding box:
  meshutils->boundingBox(mesh);

  // Todo: should we always do this?
  meshutils->findSurfaceElementEdges(mesh);
  meshutils->findSurfaceElementNormals(mesh);

  // Finalize:
  saveDirName = dirName;
  logMessage("Ready");
  
  glWidget->rebuildLists();
}



// File -> Save...
//-----------------------------------------------------------------------------
void MainWindow::saveSlot()
{
  if(glWidget->mesh==NULL) {
    logMessage("Unable to save mesh: no data");
    return;
  }
  if (!saveDirName.isEmpty()) {
    logMessage("Output directory " + saveDirName);
  } else {
    saveAsSlot();
    return;
  }
  saveElmerMesh(saveDirName);

}

// File -> Save as...
//-----------------------------------------------------------------------------
void MainWindow::saveAsSlot()
{
  if(glWidget->mesh==NULL) {
    logMessage("Unable to save mesh: no data");
    return;
  }

  saveDirName = QFileDialog::getExistingDirectory(this);

  if (!saveDirName.isEmpty()) {
    logMessage("Output directory " + saveDirName);
  } else {
    logMessage("Unable to save: directory undefined");
    return;
  }

  saveElmerMesh(saveDirName);
}


// File -> Save project...
//-----------------------------------------------------------------------------
void MainWindow::saveProjectSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Unable to save project: no mesh");
    return;
  }
  
  QString projectDirName = QFileDialog::getExistingDirectory(this, tr("Choose project directory"));

  if (!projectDirName.isEmpty()) {
    logMessage("Project directory " + projectDirName);
  } else {
    logMessage("Unable to save project: directory undefined");
    return;
  }
  
  //===========================================================================
  //                               SAVE MESH
  //===========================================================================
  saveElmerMesh(projectDirName);

  //===========================================================================
  //                               SAVE EQUATIONS
  //===========================================================================
  QString equationFileName = projectDirName + "/equation.dat";
  logMessage("Saving equation data in " + equationFileName);
  saveContents(equationFileName, equationEditor, limit->maxEquations());

  //===========================================================================
  //                               SAVE MATERIALS
  //===========================================================================
  QString materialFileName = projectDirName + "/material.dat";
  logMessage("Saving material data in " + materialFileName);
  saveContents(materialFileName, materialEditor, limit->maxMaterials());

  //===========================================================================
  //                             SAVE BODY FORCES
  //===========================================================================
  QString bodyForceFileName = projectDirName + "/bodyforce.dat";
  logMessage("Saving body force data in " + bodyForceFileName);
  saveContents(bodyForceFileName, bodyForceEditor, limit->maxBodyforces());

  //===========================================================================
  //                          SAVE INITIAL CONDITIONS
  //===========================================================================
  QString initialConditionFileName = projectDirName + "/initialcondition.dat";
  logMessage("Saving initial condition data in " + initialConditionFileName);
  saveContents(initialConditionFileName, initialConditionEditor, limit->maxInitialconditions());

  //===========================================================================
  //                          SAVE BOUNDARY CONDITIONS
  //===========================================================================
  QString boundaryConditionFileName = projectDirName + "/boundarycondition.dat";
  logMessage("Saving boundary condition data in " + boundaryConditionFileName);
  saveContents(boundaryConditionFileName, boundaryConditionEditor, limit->maxBcs());


  //===========================================================================
  //                            SAVE BODY PROPERTIES
  //===========================================================================
  QString bodyPropertyFileName = projectDirName + "/bodyproperty.dat";
  logMessage("Saving body properties in " + bodyPropertyFileName);

  QFile bodyPropertyFile(bodyPropertyFileName);
  if(!bodyPropertyFile.open(QIODevice::WriteOnly)) {
    logMessage("Unable to open " + bodyPropertyFileName + " for writing");
    return;
  }
  
  QTextStream bodyPropertyStream(&bodyPropertyFile);  

  for(int i = 0; i < limit->maxBodies(); i++ ) {
    BodyPropertyEditor *body = &bodyPropertyEditor[i];

    DynamicEditor *p = NULL;
    
    QString equationName = "";
    if((p = body->equation) != NULL)
      equationName = p->nameEdit->text().trimmed();
    
    QString materialName = "";      
    if((p = body->material) != NULL)
      materialName = p->nameEdit->text().trimmed();
    
    QString initialName = "";
    if((p = body->initial) != NULL) 
      initialName = p->nameEdit->text().trimmed();
    
    QString forceName = "";
    if((p = body->force) != NULL) 
      forceName = p->nameEdit->text().trimmed();
    
    bodyPropertyStream << "/" << i << "/" 
		       << equationName.toAscii() << "/" 
		       << materialName.toAscii() << "/"
		       << initialName.toAscii()  << "/"
		       << forceName.toAscii()    << endl;
  }

  bodyPropertyFile.close();

  //===========================================================================
  //                          SAVE BOUNDARY PROPERTIES
  //===========================================================================
  QString boundaryPropertyFileName = projectDirName + "/boundaryproperty.dat";
  logMessage("Saving boundary properties in " + boundaryPropertyFileName);

  QFile boundaryPropertyFile(boundaryPropertyFileName);
  if(!boundaryPropertyFile.open(QIODevice::WriteOnly)) {
    logMessage("Unable to open " + boundaryPropertyFileName + " for writing");
    return;
  }
  
  QTextStream boundaryPropertyStream(&boundaryPropertyFile);  

  for(int i = 0; i < limit->maxBoundaries(); i++ ) {
    BoundaryPropertyEditor *boundary = &boundaryPropertyEditor[i];

    DynamicEditor *p = NULL;
    
    QString conditionName = "";
    if((p = boundary->condition) != NULL)
      conditionName = p->nameEdit->text().trimmed();
    
    boundaryPropertyStream << "/" << i << "/" 
			   << conditionName.toAscii() << "/"
			   << boundary->ui.boundaryAsABody->isChecked() << endl;
  }

  boundaryPropertyFile.close();


  //===========================================================================
  //                              SAVE GENERAL SETUP
  //===========================================================================

  // All done:
  saveDirName = projectDirName;
  logMessage("Ready");
}

// Helper function for saveProject
//-----------------------------------------------------------------------------
void MainWindow::saveContents(QString fileName, DynamicEditor *editor, int Nmax)
{
  QFile file(fileName);
  if(!file.open(QIODevice::WriteOnly)) {
    logMessage("Unable to open " + fileName + " for writing");
    return;
  }
  
  QTextStream stream(&file);
  
  for(int i = 0; i < Nmax; i++) {
    DynamicEditor *de = &editor[i];
    
    // Menu item number and activation flag:
    stream << i << "/" << (de->menuAction != NULL) << endl;
    
    // Name:
    if(de->menuAction != NULL)
      stream << de->nameEdit->text().trimmed() << "\n";
    
    // Write all stuff from hash:
    for(int j = 0; j < de->hash.count(); j++) {
      QString key = de->hash.keys().at(j);
      hash_entry_t value = de->hash.values().at(j);
      QDomElement elem = value.elem;
      QWidget *widget = value.widget;
      
      stream << key.toAscii() << "\n";
      if(elem.attribute("Widget", "") == "CheckBox") {
	QCheckBox *checkBox = (QCheckBox*)widget;
	stream << "CheckBox: " << checkBox->isChecked() << "\n";
      } else if(elem.attribute("Widget", "") == "Edit") {
	QLineEdit *lineEdit = (QLineEdit*)widget;
	stream << "Edit: " << lineEdit->text().toAscii() << "\n";
      } else if(elem.attribute("Widget", "") == "Combo") {
	QComboBox *comboBox = (QComboBox*)widget;
	stream << "Combo: " << comboBox->currentText().toAscii() << "\n";
      } else if(elem.attribute("Widget", "") == "Label") {
	QLabel *label = (QLabel*)widget;
	stream << "Label: " << label->text().toAscii() << "\n";
      }
    }
    stream << "End\n";
  }
  file.close();
}


// File -> Load project...
//-----------------------------------------------------------------------------
void MainWindow::loadProjectSlot()
{
  QString projectDirName = QFileDialog::getExistingDirectory(this, tr("Choose project directory"));

  if (!projectDirName.isEmpty()) {
    logMessage("Project directory: " + projectDirName);
  } else {
    logMessage("Unable to load project: directory undefined");
    return;
  }

  QDir::setCurrent(projectDirName);
  saveDirName = projectDirName;

  logMessage("Clearing model data");
  modelClearSlot();

  //===========================================================================
  //                                 LOAD MESH
  //===========================================================================
  loadElmerMesh(projectDirName);

  //===========================================================================
  //                               LOAD EQUATIONS
  //===========================================================================
  QString equationFileName = projectDirName + "/equation.dat";
  logMessage("Loading equation data from " + equationFileName);
  loadContents(equationFileName, equationEditor, limit->maxEquations(), "Equation");

  //===========================================================================
  //                               LOAD MATERIALS
  //===========================================================================
  QString materialFileName = projectDirName + "/material.dat";
  logMessage("Loading material data from " + materialFileName);
  loadContents(materialFileName, materialEditor, limit->maxMaterials(), "Material");

  //===========================================================================
  //                             LOAD BODY FORCES
  //===========================================================================
  QString bodyForceFileName = projectDirName + "/bodyforce.dat";
  logMessage("Loading body force data from " + bodyForceFileName);
  loadContents(bodyForceFileName, bodyForceEditor, limit->maxBodyforces(), "BodyForce");

  //===========================================================================
  //                          LOAD INITIAL CONDITIONS
  //===========================================================================
  QString initialConditionFileName = projectDirName + "/initialcondition.dat";
  logMessage("Loading initial condition data from " + initialConditionFileName);
  loadContents(initialConditionFileName, initialConditionEditor, limit->maxInitialconditions(), "InitialCondition");

  //===========================================================================
  //                          LOAD BOUNDARY CONDITIONS
  //===========================================================================
  QString boundaryConditionFileName = projectDirName + "/boundarycondition.dat";
  logMessage("Loading boundary condition data from " + boundaryConditionFileName);
  loadContents(boundaryConditionFileName, boundaryConditionEditor, limit->maxBcs(), "BoundaryCondition");

  //===========================================================================
  //                            LOAD BODY PROPERTIES
  //===========================================================================
  QString bodyPropertyFileName = projectDirName + "/bodyproperty.dat";
  logMessage("Loading body properties from " + bodyPropertyFileName);

  QFile bodyPropertyFile(bodyPropertyFileName);
  if(!bodyPropertyFile.open(QIODevice::ReadOnly)) {
    logMessage("Unable to open " + bodyPropertyFileName + " for reading");
    return;
  }
  
  QTextStream bodyPropertyStream(&bodyPropertyFile);  

  for(int i = 0; i < limit->maxBodies(); i++ ) {
    QString line = bodyPropertyStream.readLine();

    if(line == "") {
      cout << "Error: bad or corrupted input file: bodyproperty.dat" << endl;
      cout.flush();
      return;
    }

    QStringList splittedLine = line.split("/");
    
    if(splittedLine.count() != 6) {
      cout << "Error: bad or corrupted input file: bodyproperty.dat" << endl;
      cout.flush();
      return;
    }
    
    QString equationName = splittedLine.at(2);
    QString materialName = splittedLine.at(3);
    QString initialName = splittedLine.at(4);
    QString forceName = splittedLine.at(5);
    
    BodyPropertyEditor *p = &bodyPropertyEditor[i];
    populateBodyComboBoxes(p);
    
    for(int j = 0; j < p->ui.equationCombo->count(); j++) {
      QString current = p->ui.equationCombo->itemText(j);
      if(current == equationName)
	p->ui.equationCombo->setCurrentIndex(j);
    }
    
    for(int j = 0; j < p->ui.materialCombo->count(); j++) {
      QString current = p->ui.materialCombo->itemText(j);
      if(current == materialName)
	p->ui.materialCombo->setCurrentIndex(j);
    }
    
    for(int j = 0; j < p->ui.initialConditionCombo->count(); j++) {
      QString current = p->ui.initialConditionCombo->itemText(j);
      if(current == initialName)
	p->ui.initialConditionCombo->setCurrentIndex(j);
    }
    
    for(int j = 0; j < p->ui.bodyForceCombo->count(); j++) {
      QString current = p->ui.bodyForceCombo->itemText(j);
      if(current == forceName)
	p->ui.bodyForceCombo->setCurrentIndex(j);
    }
  }

  bodyPropertyFile.close();

  //===========================================================================
  //                          LOAD BOUNDARY PROPERTIES
  //===========================================================================
  QString boundaryPropertyFileName = projectDirName + "/boundaryproperty.dat";
  logMessage("Loading boundary properties from " + boundaryPropertyFileName);

  QFile boundaryPropertyFile(boundaryPropertyFileName);
  if(!boundaryPropertyFile.open(QIODevice::ReadOnly)) {
    logMessage("Unable to open " + boundaryPropertyFileName + " for reading");
    return;
  }
  
  QTextStream boundaryPropertyStream(&boundaryPropertyFile);  
 
  for(int i = 0; i < limit->maxBoundaries(); i++ ) {
    QString line = boundaryPropertyStream.readLine();

    if(line == "") {
      cout << "Error: bad or corrupted input file: boundaryproperty.dat" << endl;
      cout.flush();
      return;
    }

    QStringList splittedLine = line.split("/");
    
    if(splittedLine.count() != 4) {
      cout << "Error: bad or corrupted input file: boundaryproperty.dat" << endl;
      cout.flush();
      return;
    }
    
    QString boundaryConditionName = splittedLine.at(2);
    bool useAsABody = (splittedLine.at(3).toInt() > 0);
    
    BoundaryPropertyEditor *p = &boundaryPropertyEditor[i];
    populateBoundaryComboBoxes(p);
    
    for(int j = 0; j < p->ui.boundaryConditionCombo->count(); j++) {
      QString current = p->ui.boundaryConditionCombo->itemText(j);
      if(current == boundaryConditionName)
	p->ui.boundaryConditionCombo->setCurrentIndex(j);
    }

    p->ui.boundaryAsABody->setChecked(useAsABody);
  }

  boundaryPropertyFile.close();


  // Finally, regenerate && save new sif:
  if(glWidget->mesh != NULL) {
    logMessage("Regenerating and saving the solver input file...");

    generateSifSlot();

    QFile file;
    QString sifName = generalSetup->ui.solverInputFileEdit->text().trimmed();
    file.setFileName(sifName);
    file.open(QIODevice::WriteOnly);
    QTextStream sif(&file);    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    sif << sifWindow->textEdit->toPlainText();
    QApplication::restoreOverrideCursor();
    file.close();
    
    file.setFileName("ELMERSOLVER_STARTINFO");
    file.open(QIODevice::WriteOnly);
    QTextStream startinfo(&file);
    startinfo << sifName.toAscii() << "\n1\n";    
    file.close();
  }

  logMessage("Ready");
}


// Helper function for load project
//--------------------------------------------------------------------------------------------
void MainWindow::loadContents(QString fileName, DynamicEditor *editor, int Nmax, QString Mname)
{
  QString line = "";
  QString line1 = "";
  QString line2 = "";
  QStringList splittedLine;
  QStringList splittedLine1;
  QStringList splittedLine2;

  QFile file(fileName);
  if(!file.open(QIODevice::ReadOnly)) {
    logMessage("Unable to open " + fileName + " for reading");
    return;
  }

  QTextStream stream(&file);

  for(int i = 0; i < Nmax; i++) {
    DynamicEditor *de = &editor[i];

    // Menu item number and activation flag:
    line = stream.readLine();
    splittedLine = line.split("/");

    if(splittedLine.count() != 2) {
      logMessage("Unexpected line in dat-file - aborting");
      return;
    }

    // int itemNumber = splittedLine.at(0).toInt();
    int menuActionDefined = splittedLine.at(1).toInt();

    // set up tabs and add to menu
    if(menuActionDefined > 0) {

      // name of the item
      QString name = stream.readLine();

      de->setupTabs(*elmerDefs, Mname, i);
      de->nameEdit->setText(name);
      de->applyButton->setText("Update");
      de->applyButton->setIcon(QIcon(":/icons/dialog-ok-apply.png"));
      de->discardButton->setText("Remove");
      de->discardButton->setIcon(QIcon(":/icons/list-remove.png"));

      const QString &tmpName = name;
      QAction *act = new QAction(tmpName, this);

      if(Mname == "Equation") {
	connect(de, SIGNAL(dynamicEditorReady(int,int)), this, SLOT(pdeEditorFinishedSlot(int,int)));
	de->spareButton->setText("Edit Solver Settings");
	de->spareButton->show();
	de->spareButton->setIcon(QIcon(":/icons/tools-wizard.png"));      
	connect(de, SIGNAL(dynamicEditorSpareButtonClicked(int, int)), this, SLOT(editNumericalMethods(int, int)));
	equationMenu->addAction(act);
      }

      if(Mname == "Material") {
	connect(de, SIGNAL(dynamicEditorReady(int,int)), this, SLOT(matEditorFinishedSlot(int,int)));
	materialMenu->addAction(act);
      }

      if(Mname == "BodyForce") {
	connect(de, SIGNAL(dynamicEditorReady(int,int)), this, SLOT(bodyForceEditorFinishedSlot(int,int)));
	bodyForceMenu->addAction(act);
      }

      if(Mname == "InitialCondition") {
	connect(de, SIGNAL(dynamicEditorReady(int,int)), this, SLOT(initialConditionEditorFinishedSlot(int,int)));
	initialConditionMenu->addAction(act);
      }

      if(Mname == "BoundaryCondition") {
	connect(de, SIGNAL(dynamicEditorReady(int,int)), this, SLOT(boundaryConditionEditorFinishedSlot(int,int)));
	boundaryConditionMenu->addAction(act);
      }

      de->menuAction = act;      

      if(Mname == "Equation") 
	createBodyCheckBoxes(BODY_EQUATION, de);

      if(Mname == "Material") 
	createBodyCheckBoxes(BODY_MATERIAL, de);

      if(Mname == "BodyForce") 
	createBodyCheckBoxes(BODY_FORCE, de);

      if(Mname == "InitialCondition") 
	createBodyCheckBoxes(BODY_INITIAL, de);

      if(Mname == "BoundaryCondition") 
	createBoundaryCheckBoxes(de);
    }

    // set contents:
    line1 = stream.readLine();
    while(line1.trimmed() != "End") {
      line2 = stream.readLine();
      if(line2.trimmed() == "End")
	break;

      splittedLine1 = line1.split("/");
      splittedLine2 = line2.split(":");

      bool match_found = false;

      for(int j = 0; j < de->hash.count(); j++) {
	QString key = de->hash.keys().at(j);
	QStringList splittedKey = key.split("/");
	hash_entry_t value = de->hash.values().at(j);
	QWidget *widget = value.widget;
	QDomElement elem = value.elem;
	
	if((splittedLine1.at(1) == splittedKey.at(1)) &&
	   (splittedLine1.at(2) == splittedKey.at(2)) &&
	   (splittedLine1.at(3) == splittedKey.at(3))) {

	  // match:
	  match_found = true;

	  if(elem.attribute("Widget", "") == "CheckBox") {
	    if(splittedLine2.at(0).trimmed() != "CheckBox")
	      logMessage("Load project: type mismatch with checkBox");
	    QCheckBox *checkBox = (QCheckBox*)widget;
	    if(splittedLine2.at(1).toInt() == 1)
	      checkBox->setChecked(true);
	    else
	      checkBox->setChecked(false);
	  } else if(elem.attribute("Widget", "") == "Edit") {
	    if(splittedLine2.at(0).trimmed() != "Edit")
	      logMessage("Load project: type mismatch with lineEdit");
	    QLineEdit *lineEdit = (QLineEdit*)widget;
	    lineEdit->setText(splittedLine2.at(1));
	  } else if(elem.attribute("Widget", "") == "Combo") {
	    if(splittedLine2.at(0).trimmed() != "Combo")
	      logMessage("Load project: type mismatch with comboBox");
	    QComboBox *comboBox = (QComboBox*)widget;
	    for(int k = 0; k < comboBox->count(); k++) {
	      QString current = comboBox->itemText(k).trimmed();
	      if(current == splittedLine2.at(1).trimmed())
		comboBox->setCurrentIndex(k);
	    }
	  }
	}
      }

      if(!match_found) {
	cout << "Error: Unable to set menu entry" << endl;
	cout.flush();
      }

      line1 = stream.readLine();
    }
  }
  file.close();
}



// Export mesh files in elmer-format:
//-----------------------------------------------------------------------------
void MainWindow::saveElmerMesh(QString dirName)
{
  logMessage("Saving elmer mesh files");

  statusBar()->showMessage(tr("Saving..."));

  QDir dir(dirName);
  if ( !dir.exists() ) dir.mkdir(dirName);
  dir.setCurrent(dirName);

  QFile file;
  mesh_t *mesh = glWidget->mesh;
  
  // Elmer's elements codes are smaller than 1000
  int maxcode = 1000;
  int *bulk_by_type = new int[maxcode];
  int *boundary_by_type = new int[maxcode];

  for(int i=0; i<maxcode; i++) {
    bulk_by_type[i] = 0;
    boundary_by_type[i] = 0;
  }

  for(int i=0; i < mesh->elements; i++) {
    element_t *e = &mesh->element[i];

    if(e->nature == PDE_BULK) 
      bulk_by_type[e->code]++;

    if(e->nature == PDE_BOUNDARY)
      boundary_by_type[e->code]++;
  }

  for(int i=0; i < mesh->surfaces; i++) {
    surface_t *s = &mesh->surface[i];

    if(s->nature == PDE_BULK)
      bulk_by_type[s->code]++;

    if(s->nature == PDE_BOUNDARY)
      boundary_by_type[s->code]++;
  }

  for(int i=0; i < mesh->edges; i++) {
    edge_t *e = &mesh->edge[i];

    if(e->nature == PDE_BULK)
      bulk_by_type[e->code]++;

    if(e->nature == PDE_BOUNDARY)
      boundary_by_type[e->code]++;
  }

  for(int i=0; i < mesh->points; i++) {
    point_t *p = &mesh->point[i];

    if(p->nature == PDE_BULK)
      bulk_by_type[p->code]++;

    if(p->nature == PDE_BOUNDARY)
      boundary_by_type[p->code]++;
  }

  int bulk_elements = 0;
  int boundary_elements = 0;
  int element_types = 0;

  for(int i=0; i<maxcode; i++) {
    bulk_elements += bulk_by_type[i];
    boundary_elements += boundary_by_type[i];

    if((bulk_by_type[i]>0) || (boundary_by_type[i]>0))
      element_types++;
  }

  // Header:
  file.setFileName("mesh.header");
  file.open(QIODevice::WriteOnly);
  QTextStream mesh_header(&file);

  cout << "Saving " << mesh->nodes << " nodes\n";
  cout << "Saving " << bulk_elements << " elements\n";
  cout << "Saving " << boundary_elements << " boundary elements\n";
  cout.flush();

  mesh_header << mesh->nodes << " ";
  mesh_header << bulk_elements << " ";
  mesh_header << boundary_elements << "\n";

  mesh_header << element_types << "\n";

  for(int i=0; i<maxcode; i++) {
    int j = bulk_by_type[i] + boundary_by_type[i];
    if(j > 0) 
      mesh_header << i << " " << j << "\n";
  }

  file.close();

  // Nodes:
  file.setFileName("mesh.nodes");
  file.open(QIODevice::WriteOnly);
  QTextStream nodes(&file);
  
  for(int i=0; i < mesh->nodes; i++) {
    node_t *node = &mesh->node[i];

    int index = node->index;

    nodes << i+1 << " " << index << " ";
    nodes << node->x[0] << " ";
    nodes << node->x[1] << " ";
    nodes << node->x[2] << "\n";
  }

  file.close();

  // Elements:
  file.setFileName("mesh.elements");
  file.open(QIODevice::WriteOnly);
  QTextStream mesh_element(&file);

  int current = 0;

  for(int i=0; i < mesh->elements; i++) {
    element_t *e = &mesh->element[i];
    int index = e->index;
    if(index < 1)
      index = 1;
    if(e->nature == PDE_BULK) {
      mesh_element << ++current << " ";
      mesh_element << index << " ";
      mesh_element << e->code << " ";
      for(int j=0; j < e->nodes; j++) 
	mesh_element << e->node[j]+1 << " ";
      mesh_element << "\n";
    }
  }

  for(int i=0; i < mesh->surfaces; i++) {
    surface_t *s = &mesh->surface[i];
    int index = s->index;
    if(index < 1)
      index = 1;
    if(s->nature == PDE_BULK) {
      mesh_element << ++current << " ";
      mesh_element << index << " ";
      mesh_element << s->code << " ";
      for(int j=0; j < s->nodes; j++) 
	mesh_element << s->node[j]+1 << " ";
      mesh_element << "\n";
    }
  }

  for(int i=0; i < mesh->edges; i++) {
    edge_t *e = &mesh->edge[i];
    int index = e->index;
    if(index < 1)
      index = 1;
    if(e->nature == PDE_BULK) {
      mesh_element << ++current << " ";
      mesh_element << index << " ";
      mesh_element << e->code << " ";
      for(int j=0; j<e->nodes; j++)
	mesh_element << e->node[j]+1 << " ";
      mesh_element << "\n";
    }
  }

  for(int i=0; i < mesh->points; i++) {
    point_t *p = &mesh->point[i];
    int index = p->index;
    if(index < 1)
      index = 1;
    if(p->nature == PDE_BULK) {
      mesh_element << ++current << " ";
      mesh_element << index << " ";
      mesh_element << p->code << " ";
      for(int j=0; j < p->nodes; j++)
	mesh_element << p->node[j]+1 << " ";
      mesh_element << "\n";
    }
  }

  file.close();
  
  // Boundary elements:
  file.setFileName("mesh.boundary");
  file.open(QIODevice::WriteOnly);
  QTextStream mesh_boundary(&file);

  current = 0;

  for(int i=0; i < mesh->surfaces; i++) {
    surface_t *s = &mesh->surface[i];
    int e0 = s->element[0] + 1;
    int e1 = s->element[1] + 1;
    if(e0 < 0)
      e0 = 0;
    if(e1 < 0)
      e1 = 0;
    int index = s->index;
    if(index < 1)
      index = 1;
    if(s->nature == PDE_BOUNDARY) {
      mesh_boundary << ++current << " ";
      mesh_boundary << index << " ";
      mesh_boundary << e0 << " " << e1 << " ";
      mesh_boundary << s->code << " ";
      for(int j=0; j < s->nodes; j++) 
	mesh_boundary << s->node[j]+1 << " ";
      mesh_boundary << "\n";
    }
  }

  for(int i=0; i < mesh->edges; i++) {
    edge_t *e = &mesh->edge[i];
    int s0 = e->surface[0] + 1;
    int s1 = e->surface[1] + 1;
    if(s0 < 0)
      s0 = 0;
    if(s1 < 0)
      s1 = 0;
    int index = e->index;
    if(index < 1)
      index = 1;
    if(e->nature == PDE_BOUNDARY) {
      mesh_boundary << ++current << " ";
      mesh_boundary << index << " ";
      mesh_boundary << s0 << " " << s1 << " ";
      mesh_boundary << e->code << " ";
      for(int j=0; j < e->nodes; j++) 
	mesh_boundary << e->node[j]+1 << " ";
      mesh_boundary << "\n";
    }
  }

  for(int i=0; i < mesh->points; i++) {
    point_t *p = &mesh->point[i];
    int e0 = p->edge[0] + 1;
    int e1 = p->edge[1] + 1;
    if(e0 < 0)
      e0 = 0;
    if(e1 < 0)
      e1 = 0;
    int index = p->index;
    if(index < 1)
      index = 1;
    if(p->nature == PDE_BOUNDARY) {
      mesh_boundary << ++current << " ";
      mesh_boundary << index << " ";
      mesh_boundary << e0 << " " << e1 << " ";
      mesh_boundary << p->code << " ";
      for(int j=0; j < p->nodes; j++) 
	mesh_boundary << p->node[j]+1 << " ";
      mesh_boundary << "\n";
    }
  }

  file.close();

  // Sif:
  QString sifName = generalSetup->ui.solverInputFileEdit->text().trimmed();
  file.setFileName(sifName);
  file.open(QIODevice::WriteOnly);
  QTextStream sif(&file);

  QApplication::setOverrideCursor(Qt::WaitCursor);
  sif << sifWindow->textEdit->toPlainText();
  QApplication::restoreOverrideCursor();

  file.close();

  // ELMERSOLVER_STARTINFO:
  file.setFileName("ELMERSOLVER_STARTINFO");
  file.open(QIODevice::WriteOnly);
  QTextStream startinfo(&file);

  startinfo << sifName.toAscii() << "\n1\n";

  file.close();

  delete [] bulk_by_type;
  delete [] boundary_by_type;

  statusBar()->showMessage(tr("Ready"));
}


// File -> Exit
//-----------------------------------------------------------------------------
void MainWindow::closeMainWindowSlot()
{
  this->close();
  return;

#if 0
  sifWindow->close();
  solverLogWindow->close();
  meshControl->close();
  boundaryDivide->close();

  for(int i = 0; i < limit->maxBcs(); i++)
    boundaryConditionEditor[i].close();

  for(int i = 0; i < limit->maxMaterials(); i++)
    materialEditor[i].close();

  for(int i = 0; i < limit->maxEquations(); i++)
    equationEditor[i].close();

  for(int i = 0; i < limit->maxBodyforces(); i++)
    bodyForceEditor[i].close();

  for(int i = 0; i < limit->maxInitialconditions(); i++)
    initialConditionEditor[i].close();

  delete [] boundaryConditionEditor;
  delete [] initialConditionEditor;
  delete [] materialEditor;
  delete [] equationEditor;
  delete [] bodyForceEditor;
  
  this->close();
  // exit(0);
#endif
}


// File -> Save picture as...
//-----------------------------------------------------------------------------
void MainWindow::savePictureSlot()
{
  bool withAlpha = false;

  glReadBuffer(GL_FRONT);

  QImage image = glWidget->grabFrameBuffer(withAlpha);

  QString fileName = QFileDialog::getSaveFileName(this,
	tr("Save picture"), "", tr("Picture files (*.bmp *.jpg *.png *.pbm *.pgm *.ppm)"));
  
  if(fileName == "") {
    logMessage("File name is empty");
    return;
  }

  QFileInfo fi(fileName);
  QString suffix = fi.suffix();
  suffix.toUpper();
  
  bool ok = image.save(fileName, suffix.toAscii(), 95); // fixed quality
  
  if(!ok) {
    cout << "Failed writing picture" << endl;
    cout.flush();
  }
}


//*****************************************************************************
//
//                                Model MENU
//
//*****************************************************************************


// Model -> Setup...
//-----------------------------------------------------------------------------
void MainWindow::modelSetupSlot()
{
  generalSetup->show();
}

//-----------------------------------------------------------------------------
void MainWindow::createBodyCheckBoxes(int which, DynamicEditor *pe)
{
  if  (!glWidget->mesh ) return;

  if ( pe->spareScroll->widget() ) {
    delete pe->spareScroll->widget();
  }

  QGridLayout *slayout = new QGridLayout;
  QLabel *l = new QLabel(tr("Apply to bodies:"));

  int count = 0, even = 0;

  slayout->addWidget(l,count,0);
  count++;

  for( int i=0; i<glWidget->bodyMap.count(); i++ )
  {
     int n=glWidget->bodyMap.key(i);
     if ( n >= 0 ) {
        int m=glWidget->bodyMap.value(n);

        BodyPropertyEditor *body = &bodyPropertyEditor[m];
        populateBodyComboBoxes(body);

        QString title = body->ui.nameEdit->text().trimmed();
        QCheckBox *a;

        if ( title == "" )
          a = new QCheckBox("Body " + QString::number(n));
        else
          a = new QCheckBox(title);

        DynamicEditor *p = NULL;

        switch(which) {
          case BODY_MATERIAL:
            p=body->material;
            connect(a, SIGNAL(stateChanged(int)), this, SLOT(materialBodyChanged(int)));
          break;
          case BODY_INITIAL:
            p=body->initial;
            connect(a, SIGNAL(stateChanged(int)), this, SLOT(initialBodyChanged(int)));
          break;
          case BODY_FORCE:
            p=body->force;
            connect(a, SIGNAL(stateChanged(int)), this, SLOT(forceBodyChanged(int)));
          break;
          case BODY_EQUATION:
            p=body->equation;
            connect(a, SIGNAL(stateChanged(int)), this, SLOT(equationBodyChanged(int)));
          break;
        }

        a->setProperty( "body", (unsigned long long)body );
        a->setProperty( "editor", (unsigned long long)pe );

        if ( p==pe )
          a->setChecked(true);
        else if ( p != NULL )
          a->setEnabled(false);

        slayout->addWidget(a,count,even);
        even = 1 - even;
        if (!even) count++;
     }
  }

  for( int i=0; i < limit->maxBoundaries(); i++ )
  {
     BoundaryPropertyEditor *boundary=&boundaryPropertyEditor[i];
     if ( boundary->bodyProperties ) {
        BodyPropertyEditor *body = boundary->bodyProperties;
        populateBodyComboBoxes(body);

        QString title = body->ui.nameEdit->text().trimmed();
        QCheckBox *a;

        if ( title == "" )
          a = new QCheckBox("Body{Boundary " + QString::number(i)+ "}");
        else
          a = new QCheckBox(title);

        DynamicEditor *p = NULL;

        switch(which) {
          case BODY_MATERIAL:
            p=body->material;
            connect(a, SIGNAL(stateChanged(int)), this, SLOT(materialBodyChanged(int)));
          break;
          case BODY_INITIAL:
            p=body->initial;
            connect(a, SIGNAL(stateChanged(int)), this, SLOT(initialBodyChanged(int)));
          break;
          case BODY_FORCE:
            p=body->force;
            connect(a, SIGNAL(stateChanged(int)), this, SLOT(forceBodyChanged(int)));
          break;
          case BODY_EQUATION:
            p=body->equation;
            connect(a, SIGNAL(stateChanged(int)), this, SLOT(equationBodyChanged(int)));
          break;
        }

        a->setProperty( "body", (unsigned long long)body );
        a->setProperty( "editor", (unsigned long long)pe );

        if ( p==pe )
          a->setChecked(true);
        else if ( p != NULL )
          a->setEnabled(false);

        slayout->addWidget(a,count,even);
        even = 1-even;
        if (!even) count++;
     }
  }

  QGroupBox *box = new QGroupBox;
  box->setLayout(slayout);

  pe->spareScroll->setWidget(box);
  pe->spareScroll->setMinimumHeight(80);
  pe->spareScroll->show();
}


//-----------------------------------------------------------------------------

//*****************************************************************************

// Model -> Equation -> Add...
//-----------------------------------------------------------------------------
void MainWindow::addEquationSlot()
{
  // use the first free slot in equationEditor array:
  int current = 0;
  bool found = false;
  
  DynamicEditor *pe = NULL;

  for(int i = 0; i < limit->maxEquations(); i++) {
    pe = &equationEditor[i];
    if(pe->menuAction == NULL) {
      found = true;
      current = i;
      break;
    }
  }
  
  if(!found) {
    logMessage("Equation max limit reached - unable to add equation");
    return;
  }
  
  pe->setupTabs(*elmerDefs, "Equation", current);

  pe->applyButton->setText("Add");
  pe->applyButton->setIcon(QIcon(":/icons/list-add.png"));
  pe->discardButton->setText("Cancel");
  pe->discardButton->setIcon(QIcon(":/icons/dialog-close.png"));
  pe->show();
  
  connect(pe, SIGNAL(dynamicEditorReady(int,int)),
	  this, SLOT(pdeEditorFinishedSlot(int,int)));

  pe->spareButton->setText("Edit Solver Settings");
  pe->spareButton->show();
  pe->spareButton->setIcon(QIcon(":/icons/tools-wizard.png"));

  // connect( pe->spareButton, SIGNAL(clicked()), this, SLOT(editNumericalMethods()) );

  connect(pe, SIGNAL(dynamicEditorSpareButtonClicked(int, int)),
	  this, SLOT(editNumericalMethods(int, int)));

  // Equation is new - add to menu:
  const QString &equationName = pe->nameEdit->text().trimmed();
  QAction *act = new QAction(equationName, this);
  equationMenu->addAction(act);
  pe->menuAction = act;

  createBodyCheckBoxes(BODY_EQUATION,pe);
}


// signal (int, int) emitted by dynamic editor when "spare button" clicked:
//-----------------------------------------------------------------------------
void MainWindow::editNumericalMethods(int current, int id)
{

  QString title="";

  for( int i=0; i < limit->maxEquations(); i++ ) {
    if ( equationEditor[i].ID == id ) {
      title = equationEditor[i].tabWidget->tabText(current);
      break;
    }
  }

  if ( title == "General" ) {
    logMessage( "No solver controls for 'General' equation options" );
    return;
  }

  SolverParameterEditor *spe = &solverParameterEditor[current];

  spe->setWindowTitle("Solver control for " + title );

  spe->solverName = title;

  if ( spe->generalOptions == NULL )
  {
    spe->generalOptions = new DynamicEditor;
    spe->generalOptions->setupTabs(*elmerDefs, "Solver", current );

    for( int i=0; i<spe->generalOptions->tabWidget->count(); i++ )
    {
      if ( spe->generalOptions->tabWidget->tabText(i) == title )
      {
//      spe->ui.solverControlTabs->removeTab(0);
        spe->ui.solverControlTabs->insertTab(0,spe->generalOptions->tabWidget->widget(i),
                     "Solver specific options");
        break;
      }
    }
  }

  spe->show();
  spe->raise();
}


// signal (int,int) emitted by equation editor when ready:
//-----------------------------------------------------------------------------
void MainWindow::pdeEditorFinishedSlot(int signal, int id)
{
  
  DynamicEditor *pe = &equationEditor[id];
  
  const QString &equationName = pe->nameEdit->text().trimmed();

  bool signalOK = signal==MAT_OK || signal==MAT_APPLY;

  if((equationName == "") && signalOK ) {
    logMessage("Refusing to add/update equation without name");
    return;
  }
  
  if( signalOK ) {
    if(pe->menuAction != NULL) {
      pe->menuAction->setText(equationName);
      logMessage("Equation updated");
      if ( signal==MAT_OK ) pe->close();
    }
  } else if (signal==MAT_NEW) {
    addEquationSlot();

  } else if(signal == MAT_DELETE) {

    for( int i=0; i < limit->maxBodies(); i++ ) {
       BodyPropertyEditor *body = &bodyPropertyEditor[i];
       if ( body->equation == pe ) body->equation=NULL;
    }

    // Equation is not in menu:
    if(pe->menuAction == NULL) {
      logMessage("Ready");
      pe->close();
      return;
    }

    // Delete from menu:
    delete pe->menuAction;
    pe->menuAction = NULL;
    pe->close();
    logMessage("Equation deleted");
  }
}

// signal (QAction*) emitted by equationMenu when an item has been selected:
//-----------------------------------------------------------------------------
void MainWindow::equationSelectedSlot(QAction* act)
{
  // Edit the selected material:
  for(int i = 0; i < limit->maxEquations(); i++) {
    DynamicEditor *pe = &equationEditor[i];
    if(pe->menuAction == act) {
      pe->applyButton->setText("Update");
      pe->applyButton->setIcon(QIcon(":/icons/dialog-ok-apply.png"));
      pe->discardButton->setText("Remove");
      pe->discardButton->setIcon(QIcon(":/icons/list-remove.png"));
      createBodyCheckBoxes(BODY_EQUATION,pe);
      pe->show(); pe->raise();
    }
  }
}

//-----------------------------------------------------------------------------
void MainWindow::equationBodyChanged(int state)
{
  QWidget *a = (QWidget *)QObject::sender();
  if  (glWidget->mesh ) {
     DynamicEditor *mat  = (DynamicEditor *)a->property("editor").toULongLong();
     BodyPropertyEditor *body = (BodyPropertyEditor *)a->property("body").toULongLong();
 
     QString mat_name = mat->nameEdit->text().trimmed();
     int ind = body->ui.equationCombo->findText(mat_name);
     body->equation = NULL;
     if ( state ) {
       body->touched = true;
       body->equation = mat;
       body->ui.equationCombo->setCurrentIndex(ind);
     } else {
       body->ui.equationCombo->setCurrentIndex(-1);
     }
  }
}


//*****************************************************************************

// Model -> Material -> Add...
//-----------------------------------------------------------------------------
void MainWindow::addMaterialSlot()
{
  // use the first free slot in materialEditor array:
  int current = 0;
  bool found = false;

  DynamicEditor *pe = NULL;
  for(int i = 0; i < limit->maxMaterials(); i++) {
    pe = &materialEditor[i];
    if(pe->menuAction == NULL) {
      found = true;
      current = i;
      break;
    }
  }
  
  if(!found) {
    logMessage("Material max limit reached - unable to add material");
    return;
  }
  
  pe->setupTabs(*elmerDefs, "Material", current );

  pe->applyButton->setText("Add");
  pe->applyButton->setIcon(QIcon(":/icons/list-add.png"));
  pe->discardButton->setText("Cancel");
  pe->discardButton->setIcon(QIcon(":/icons/dialog-close.png"));

  connect(pe, SIGNAL(dynamicEditorReady(int,int)),
	  this, SLOT(matEditorFinishedSlot(int,int)));

  // Material is new - add to menu:
  const QString &materialName = pe->nameEdit->text().trimmed();
  QAction *act = new QAction(materialName, this);
  materialMenu->addAction(act);
  pe->menuAction = act;

  createBodyCheckBoxes(BODY_MATERIAL,pe);
  pe->show(); pe->raise();
}

// signal (int,int) emitted by material editor when ready:
//-----------------------------------------------------------------------------
void MainWindow::matEditorFinishedSlot(int signal, int id)
{

  DynamicEditor *pe = &materialEditor[id];
  
  const QString &materialName = pe->nameEdit->text().trimmed();

  bool signalOK = signal==MAT_OK || signal==MAT_APPLY;
  if((materialName == "") && signalOK ) {
    logMessage("Refusing to add/update material with no name");
    return;
  }
  
  if( signalOK ) {
    if(pe->menuAction != NULL) {
      pe->menuAction->setText(materialName);
      logMessage("Material updated");
      if ( signal == MAT_OK ) pe->close();
      return;
    }
  } else if ( signal==MAT_NEW ) {
    addMaterialSlot();

  } else if(signal == MAT_DELETE) {

    for( int i=0; i < limit->maxBodies(); i++ ) {
       BodyPropertyEditor *body = &bodyPropertyEditor[i];
       if ( body->material == pe ) body->material=NULL;
    }

    // Material is not in menu:
    if(pe->menuAction == NULL) {
      logMessage("Ready");
      pe->close();
      return;
    }

    // Delete from menu:
    delete pe->menuAction;
    pe->menuAction = NULL;
    pe->close();
    logMessage("Material deleted");
  }
}

// signal (QAction*) emitted by materialMenu when an item has been selected:
//-----------------------------------------------------------------------------
void MainWindow::materialSelectedSlot(QAction* act)
{
  // Edit the selected material:
  for(int i = 0; i < limit->maxMaterials(); i++) {
    DynamicEditor *pe = &materialEditor[i];
    if(pe->menuAction == act) {
      pe->applyButton->setText("Update");
      pe->applyButton->setIcon(QIcon(":/icons/dialog-ok-apply.png"));
      pe->discardButton->setText("Remove");
      pe->discardButton->setIcon(QIcon(":/icons/list-remove.png"));
      createBodyCheckBoxes(BODY_MATERIAL, pe);
      pe->show(); pe->raise();
    }
  }
}


void MainWindow::materialBodyChanged(int state)
{
  QWidget *a = (QWidget *)QObject::sender();
  if  (glWidget->mesh ) {
     DynamicEditor *mat  = (DynamicEditor *)a->property("editor").toULongLong();
     BodyPropertyEditor *body = (BodyPropertyEditor *)a->property("body").toULongLong();
 
     QString mat_name = mat->nameEdit->text().trimmed();
     int ind = body->ui.materialCombo->findText(mat_name);

     body->material = NULL;
     if ( state ) {
       body->touched = true;
       body->material = mat;
       body->ui.materialCombo->setCurrentIndex(ind);
     } else {
       body->ui.materialCombo->setCurrentIndex(-1);
     }
  }
}


//*****************************************************************************

// Model -> Body force -> Add...
//-----------------------------------------------------------------------------
void MainWindow::addBodyForceSlot()
{
  // use the first free slot in bodyForceEditor array:
  int current = 0;
  bool found = false;

  DynamicEditor *pe = NULL;
  for(int i = 0; i < limit->maxBodyforces(); i++) {
    pe = &bodyForceEditor[i];
    if(pe->menuAction == NULL) {
      found = true;
      current = i;
      break;
    }
  }
  
  if(!found) {
    logMessage("Body force max limit reached - unable to add");
    return;
  }
  
  pe->setupTabs(*elmerDefs, "BodyForce", current );

  pe->applyButton->setText("Add");
  pe->applyButton->setIcon(QIcon(":/icons/list-add.png"));
  pe->discardButton->setText("Cancel");
  pe->discardButton->setIcon(QIcon(":/icons/dialog-close.png"));

  connect(pe, SIGNAL(dynamicEditorReady(int,int)),
	  this, SLOT(bodyForceEditorFinishedSlot(int,int)));

  // Body force is new - add to menu:
  const QString &bodyForceName = pe->nameEdit->text().trimmed();
  QAction *act = new QAction(bodyForceName, this);
  bodyForceMenu->addAction(act);
  pe->menuAction = act;

  createBodyCheckBoxes( BODY_FORCE, pe );
  pe->show(); pe->raise();
}

// signal (int,int) emitted by body force editor when ready:
//-----------------------------------------------------------------------------
void MainWindow::bodyForceEditorFinishedSlot(int signal, int id)
{
  DynamicEditor *pe = &bodyForceEditor[id];
  
  const QString &bodyForceName = pe->nameEdit->text().trimmed();

  bool signalOK = signal==MAT_OK || signal==MAT_APPLY;
  
  if((bodyForceName == "") && signalOK ) {
    logMessage("Refusing to add/update body force with no name");
    return;
  }
  
  if( signalOK ) {
    if(pe->menuAction != NULL) {
      pe->menuAction->setText(bodyForceName);
      logMessage("Body force updated");
      if ( signal==MAT_OK ) pe->close();
    }

  } else if (signal==MAT_NEW) {
     addBodyForceSlot(); 

  } else if(signal == MAT_DELETE) {
    for( int i=0; i < limit->maxBodies(); i++ ) {
       BodyPropertyEditor *body = &bodyPropertyEditor[i];
       if ( body->force == pe ) body->force=NULL;
    }

    if(pe->menuAction == NULL) {
      logMessage("Ready");
      pe->close();
      return;
    }
    
    // Delete from menu:
    delete pe->menuAction;
    pe->menuAction = NULL;
    pe->close();
    logMessage("Body force deleted");
  }
}

// signal (QAction*) emitted by bodyForceMenu when an item has been selected:
//-----------------------------------------------------------------------------
void MainWindow::bodyForceSelectedSlot(QAction* act)
{
  // Edit the selected body force:
  for(int i = 0; i < limit->maxBodyforces(); i++) {
    DynamicEditor *pe = &bodyForceEditor[i];
    if(pe->menuAction == act) {
      pe->applyButton->setText("Update");
      pe->applyButton->setIcon(QIcon(":/icons/dialog-ok-apply.png"));
      pe->discardButton->setText("Remove");
      pe->discardButton->setIcon(QIcon(":/icons/list-remove.png"));
      createBodyCheckBoxes( BODY_FORCE, pe );
      pe->show(); pe->raise();
    }
  }
}

//-----------------------------------------------------------------------------
void MainWindow::forceBodyChanged(int state)
{
  QWidget *a = (QWidget *)QObject::sender();
  if  (glWidget->mesh ) {
     DynamicEditor *mat  = (DynamicEditor *)a->property("editor").toULongLong();
     BodyPropertyEditor *body = (BodyPropertyEditor *)a->property("body").toULongLong();
 
     QString mat_name = mat->nameEdit->text().trimmed();
     int ind = body->ui.bodyForceCombo->findText(mat_name);
     body->force = NULL;
     if ( state ) {
       body->touched = true;
       body->force = mat;
       body->ui.bodyForceCombo->setCurrentIndex(ind);
     } else {
       body->ui.bodyForceCombo->setCurrentIndex(-1);
     }
  }
}


//*****************************************************************************

// Model -> Initial condition -> Add...
//-----------------------------------------------------------------------------
void MainWindow::addInitialConditionSlot()
{
  // use the first free slot in initialConditionEditor array:
  int current = 0;
  bool found = false;
  
  DynamicEditor *pe = NULL;
  for(int i = 0; i < limit->maxInitialconditions(); i++) {
    pe = &initialConditionEditor[i];
    if(pe->menuAction == NULL) {
      found = true;
      current = i;
      break;
    }
  }
  
  if(!found) {
    logMessage("Initial condition max limit reached - unable to add");
    return;
  }
  
  pe->setupTabs(*elmerDefs, "InitialCondition", current );

  pe->applyButton->setText("Add");
  pe->applyButton->setIcon(QIcon(":/icons/list-add.png"));
  pe->discardButton->setText("Cancel");
  pe->discardButton->setIcon(QIcon(":/icons/dialog-close.png"));
  
  connect(pe, SIGNAL(dynamicEditorReady(int,int)),
	  this, SLOT(initialConditionEditorFinishedSlot(int,int)));

    // Initial condition is new - add to menu:
  const QString &initialConditionName = pe->nameEdit->text().trimmed();
  QAction *act = new QAction(initialConditionName, this);
  initialConditionMenu->addAction(act);
  pe->menuAction = act;

  createBodyCheckBoxes( BODY_INITIAL, pe );
  pe->show(); pe->raise();
}

// signal (int,int) emitted by initial condition editor when ready:
//-----------------------------------------------------------------------------
void MainWindow::initialConditionEditorFinishedSlot(int signal, int id)
{
  DynamicEditor *pe = &initialConditionEditor[id];
  
  const QString &initialConditionName = pe->nameEdit->text().trimmed();
  
  bool signalOK = signal==MAT_OK || signal==MAT_APPLY;
  if((initialConditionName == "") && signalOK ) {
    logMessage("Refusing to add/update initial condition with no name");
    return;
  }
  
  if( signalOK ) {
    if(pe->menuAction != NULL) {
      pe->menuAction->setText(initialConditionName);
      logMessage("Initial condition updated");
      if ( signal==MAT_OK ) pe->close();
    }
  } else if (signal==MAT_NEW ) {
     addInitialConditionSlot();

  } else if(signal == MAT_DELETE) {

    for( int i=0; i < limit->maxBodies(); i++ ) {
       BodyPropertyEditor *body = &bodyPropertyEditor[i];
       if ( body->initial == pe ) body->initial=NULL;
    }

    // Initial condition is not in menu:
    if(pe->menuAction == NULL) {
      logMessage("Ready");
      pe->close();
      return;
    }
    
    // Delete from menu:
    delete pe->menuAction;
    pe->menuAction = NULL;
    pe->close();
    logMessage("Initial condition deleted");
  }
}

// signal (QAction*) emitted by initialConditionMenu when item selected:
//-----------------------------------------------------------------------------
void MainWindow::initialConditionSelectedSlot(QAction* act)
{
  // Edit the selected initial condition:
  for(int i = 0; i < limit->maxInitialconditions(); i++) {
    DynamicEditor *pe = &initialConditionEditor[i];
    if(pe->menuAction == act) {
      pe->applyButton->setText("Update");
      pe->applyButton->setIcon(QIcon(":/icons/dialog-ok-apply.png"));
      pe->discardButton->setText("Remove");
      pe->discardButton->setIcon(QIcon(":/icons/list-remove.png"));
      createBodyCheckBoxes( BODY_INITIAL, pe );
      pe->show(); pe->raise();
    }
  }
}

//-----------------------------------------------------------------------------
void MainWindow::initialBodyChanged(int state)
{
  QWidget *a = (QWidget *)QObject::sender();
  if  (glWidget->mesh ) {
     DynamicEditor *mat  = (DynamicEditor *)a->property("editor").toULongLong();
     BodyPropertyEditor *body = (BodyPropertyEditor *)a->property("body").toULongLong();
 
     QString mat_name = mat->nameEdit->text().trimmed();
     int ind = body->ui.initialConditionCombo->findText(mat_name);
     body->initial = NULL;
     if ( state ) {
       body->touched = true;
       body->initial = mat;
       body->ui.initialConditionCombo->setCurrentIndex(ind);
     } else {
       body->ui.initialConditionCombo->setCurrentIndex(-1);
     }
  }
}


//*****************************************************************************
//-----------------------------------------------------------------------------
void MainWindow::createBoundaryCheckBoxes(DynamicEditor *pe)
{
  if  (!glWidget->mesh ) return;

  if ( pe->spareScroll->widget() ) {
    delete pe->spareScroll->widget();
  }

  QGridLayout *slayout = new QGridLayout;
  QLabel *l = new QLabel(tr("Apply to boundaries:"));
  int count=0,even=0;

  slayout->addWidget(l,count,0);
  count++;


  for( int i=0; i<glWidget->boundaryMap.count(); i++ )
  {
     int n=glWidget->boundaryMap.key(i);
     if ( n >= 0 ) {
        int m=glWidget->boundaryMap.value(n);

        BoundaryPropertyEditor *boundary = &boundaryPropertyEditor[m];
        populateBoundaryComboBoxes(boundary);

        QString title =  ""; // boundary->ui.nameEdit->text().trimmed();
        QCheckBox *a;

        if ( title == "" )
          a = new QCheckBox("Boundary " + QString::number(n));
        else
          a = new QCheckBox(title);

        DynamicEditor *p = NULL;

        p=boundary->condition;
        connect(a, SIGNAL(stateChanged(int)), this, SLOT(bcBoundaryChanged(int)));

        a->setProperty( "boundary", (unsigned long long)boundary );
        a->setProperty( "condition", (unsigned long long)pe );

        if ( p==pe )
          a->setChecked(true);
        else if ( p != NULL )
          a->setEnabled(false);

        slayout->addWidget(a,count,even);
        even = 1-even;
        if (!even) count++;
     }
  }

  QGroupBox *box = new QGroupBox;
  box->setLayout(slayout);

  pe->spareScroll->setWidget(box);
  pe->spareScroll->setMinimumHeight(80);
  pe->spareScroll->show();
}


//-----------------------------------------------------------------------------

// Model -> Boundary condition -> Add...
//-----------------------------------------------------------------------------
void MainWindow::addBoundaryConditionSlot()
{
  // use the first free slot in boundaryConditionEditor array:
  int current = 0;
  bool found = false;
  
  DynamicEditor *pe = NULL;
  for(int i = 0; i < limit->maxBcs(); i++) {
    pe = &boundaryConditionEditor[i];
    if(pe->menuAction == NULL) {
      found = true;
      current = i;
      break;
    }
  }
  
  if(!found) {
    logMessage("Boundary condition max limit reached - unable to add");
    return;
  }
  
  pe->setupTabs(*elmerDefs, "BoundaryCondition", current );
  
  pe->applyButton->setText("Add");
  pe->applyButton->setIcon(QIcon(":/icons/list-add.png"));
  pe->discardButton->setText("Cancel");
  pe->discardButton->setIcon(QIcon(":/icons/dialog-close.png"));
  pe->show();
  
  connect(pe, SIGNAL(dynamicEditorReady(int,int)),
	  this, SLOT(boundaryConditionEditorFinishedSlot(int,int)));

  // Boundary condition is new - add to menu:
  const QString &boundaryConditionName = pe->nameEdit->text().trimmed();
  QAction *act = new QAction(boundaryConditionName, this);
  boundaryConditionMenu->addAction(act);
  pe->menuAction = act;

  createBoundaryCheckBoxes(pe);
}

// signal (int,int) emitted by boundary condition editor when ready:
//-----------------------------------------------------------------------------
void MainWindow::boundaryConditionEditorFinishedSlot(int signal, int id)
{
  DynamicEditor *pe = &boundaryConditionEditor[id];
  
  const QString &boundaryConditionName = pe->nameEdit->text().trimmed();
  
  bool signalOK = signal==MAT_OK || signal==MAT_APPLY;

  if((boundaryConditionName == "") && signalOK ) {
    logMessage("Refusing to add/update boundary condition with no name");
    return;
  }
  
  if( signalOK ) {
    if(pe->menuAction != NULL) {
      pe->menuAction->setText(boundaryConditionName);
      logMessage("Boundary condition updated");
      if ( signal==MAT_OK ) pe->close();
    }
  } else if ( signal==MAT_NEW ) {
    addBoundaryConditionSlot();

  } else if(signal == MAT_DELETE) {

    for( int i=0; i < limit->maxBoundaries(); i++ ) {
       BoundaryPropertyEditor *bndry = &boundaryPropertyEditor[i];
       if ( bndry->condition == pe ) bndry->condition=NULL;
    }

    // Bopundary condition is not in menu:
    if(pe->menuAction == NULL) {
      logMessage("Ready");
      pe->close();
      return;
    }
    
    // Delete from menu:
    delete pe->menuAction;
    pe->menuAction = NULL;
    pe->close();

    logMessage("Boundary condition deleted");
  }
}

// signal (QAction*) emitted by boundaryConditionMenu when item selected:
//-----------------------------------------------------------------------------
void MainWindow::boundaryConditionSelectedSlot(QAction* act)
{
  // Edit the selected boundary condition:
  for(int i = 0; i < limit->maxBcs(); i++) {
    DynamicEditor *pe = &boundaryConditionEditor[i];
    if(pe->menuAction == act) {
      pe->applyButton->setText("Update");
      pe->applyButton->setIcon(QIcon(":/icons/dialog-ok-apply.png"));
      pe->discardButton->setText("Remove");
      pe->discardButton->setIcon(QIcon(":/icons/list-remove.png"));
      createBoundaryCheckBoxes(pe);
      pe->show(); pe->raise();
    }
  }
}

//-----------------------------------------------------------------------------
void MainWindow::bcBoundaryChanged(int state)
{
  QWidget *a = (QWidget *)QObject::sender();
  if  (glWidget->mesh ) {
     DynamicEditor *mat  = (DynamicEditor *)a->property("condition").toULongLong();
     BoundaryPropertyEditor *boundary = 
           (BoundaryPropertyEditor *)a->property("boundary").toULongLong();
 
     QString mat_name = mat->nameEdit->text().trimmed();
     int ind = boundary->ui.boundaryConditionCombo->findText(mat_name);
     boundary->condition = NULL;
     if ( state ) {
       boundary->touched = true;
       boundary->condition = mat;
       boundary->ui.boundaryConditionCombo->setCurrentIndex(ind);
     } else {
       boundary->ui.boundaryConditionCombo->setCurrentIndex(-1);
     }
  }
}






// Model -> Set body properties
//-----------------------------------------------------------------------------
void MainWindow::bodyEditSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Unable to open body editor - there is no mesh");
    bodyEditActive = false;
    synchronizeMenuToState();
    return;
  }

  bodyEditActive = !bodyEditActive;
  glWidget->bodyEditActive = bodyEditActive;

  if(bodyEditActive)
    bcEditActive = false;

  synchronizeMenuToState();

  if(bodyEditActive)
    logMessage("Double click a boundary to edit body properties");
}



// Model -> Set boundary conditions
//-----------------------------------------------------------------------------
void MainWindow::bcEditSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Unable to open BC editor - there is no mesh");
    bcEditActive = false;
    synchronizeMenuToState();
    return;
  }

  bcEditActive = !bcEditActive;

  if(bcEditActive)
    bodyEditActive = false;

  synchronizeMenuToState();

  if(bcEditActive)
    logMessage("Double click a boundary to edit BCs");
}



// Model -> Summary...
//-----------------------------------------------------------------------------
void MainWindow::modelSummarySlot()
{
  mesh_t *mesh = glWidget->mesh;
  QTextEdit *te = summaryEditor->ui.summaryEdit;
  te->clear();
  summaryEditor->show();

  if(mesh == NULL) {
    te->append("No mesh");
    return;
  }
  
  te->append("FINITE ELEMENT MESH");
  te->append("Mesh dimension: " + QString::number(mesh->cdim));
  te->append("Leading element dimension: " + QString::number(mesh->dim));
  te->append("Nodes: " + QString::number(mesh->nodes));
  te->append("Volume elements: " + QString::number(mesh->elements));
  te->append("Surface elements: " + QString::number(mesh->surfaces));
  te->append("Edge elements: " + QString::number(mesh->edges));
  te->append("Point elements: " + QString::number(mesh->points));
  te->append("");

  // This is almost duplicate info with the above, they might be fused in some way...
  te->append("ELEMENT TYPES");
  int *elementtypes = new int[828];
  for(int i=0;i<=827;i++)
    elementtypes[i] = 0;
  for(int i=0;i<mesh->elements;i++)
    elementtypes[mesh->element[i].code] += 1;
  for(int i=0;i<mesh->surfaces;i++)
    elementtypes[mesh->surface[i].code] += 1;
  for(int i=0;i<mesh->edges;i++)
    elementtypes[mesh->edge[i].code] += 1;
  for(int i=0;i<mesh->points;i++)
    elementtypes[mesh->point[i].code] += 1;
  for(int i=827;i>0;i--)
    if(elementtypes[i])  te->append(QString::number(i) + ": " + QString::number(elementtypes[i]));
  te->append("");
  delete [] elementtypes;


  te->append("BOUNDING BOX");
  QString coordnames="XYZ";
  for(int j=0;j<3;j++) {
    double mincoord, maxcoord, coord;
    mincoord = maxcoord = mesh->node[0].x[j];
    for(int i=0;i<mesh->nodes;i++) {
      coord = mesh->node[i].x[j];
      if(mincoord > coord) mincoord = coord;
      if(maxcoord < coord) maxcoord = coord;
    }
    te->append(coordnames[j]+"-coordinate: [ " + QString::number(mincoord) + " ,  " + QString::number(maxcoord)+" ]");
  }
  te->append("");



  // Check equations:
  int count = 0;
  for(int i = 0; i < limit->maxEquations(); i++) {
    if(equationEditor[i].menuAction != NULL)
      count++;
  }
  te->append("GENERAL");
  te->append("Equations: " + QString::number(count));

  // Check materials:
  count = 0;
  for(int i = 0; i < limit->maxMaterials(); i++) {
    if(materialEditor[i].menuAction != NULL)
      count++;
  }
  te->append("Materials: " + QString::number(count));

  // Check boundary conditions:
  count = 0;
  for(int i = 0; i < limit->maxBcs(); i++) {
    if( boundaryConditionEditor[i].touched) count++;
  }
  te->append("Boundary conditions: " + QString::number(count));

  // Check body properties:
  count = 0;
  for(int i = 0; i < limit->maxBodies(); i++) {
    if(bodyPropertyEditor[i].touched)
      count++;
  }
  te->append("Body properties: " + QString::number(count));
  te->append("");

  // Count volume bodies:
  //---------------------
  int undetermined = 0;
  int *tmp = new int[mesh->elements];
  for(int i = 0; i < mesh->elements; i++)
    tmp[i] = 0;

  for(int i = 0; i < mesh->elements; i++) {
    element_t *e = &mesh->element[i];
    if(e->nature == PDE_BULK) {
      if(e->index >= 0)
	tmp[e->index]++;
      else
	undetermined++;
    }
  }

  te->append("VOLUME BODIES");
  count = 0;
  for(int i = 0; i<mesh->elements; i++) {
    if( tmp[i]>0 ) {
      count++;
      QString qs = "Body " + QString::number(i) + ": " 
	+ QString::number(tmp[i]) + " volume elements";
      if(bodyPropertyEditor[i].touched) 
	qs.append(" (Body property set)");
      te->append(qs);
    }
  }
  te->append("Undetermined: " + QString::number(undetermined));
  te->append("Total: " + QString::number(count) + " volume bodies");
  te->append("");

  delete [] tmp;

  // Count surface bodies:
  //---------------------
  undetermined = 0;
  tmp = new int[mesh->surfaces];
  for(int i = 0; i < mesh->surfaces; i++)
    tmp[i] = 0;

  for(int i = 0; i < mesh->surfaces; i++) {
    surface_t *s = &mesh->surface[i];
    if(s->nature == PDE_BULK) {
      if(s->index >= 0)
	tmp[s->index]++;
      else
	undetermined++;
    }
  }

  te->append("SURFACE BODIES");
  count = 0;
  for(int i = 0; i<mesh->surfaces; i++) {
    if( tmp[i]>0 ) {
      count++;
      QString qs = "Body " + QString::number(i) + ": " 
	+ QString::number(tmp[i]) + " surface elements";
      if(bodyPropertyEditor[i].touched) 
	qs.append(" (Body property set)");
      te->append(qs);
    }
  }
  te->append("Undetermined: " + QString::number(undetermined));
  te->append("Total: " + QString::number(count) + " surface bodies");
  te->append("");

  delete [] tmp;


  // Count surface boundaries:
  //--------------------------
  undetermined = 0;
  tmp = new int[mesh->surfaces];
  for(int i = 0; i < mesh->surfaces; i++)
    tmp[i] = 0;
  
  for(int i = 0; i < mesh->surfaces; i++) {
    surface_t *s = &mesh->surface[i];
    if(s->nature == PDE_BOUNDARY) {
      if(s->index >= 0)
	tmp[s->index]++;
      else
	undetermined++;
    }
  }

  te->append("SURFACE BOUNDARIES");
  count = 0;
  for(int i = 0; i<mesh->surfaces; i++) {
    if( tmp[i]>0 ) {
      count++;
      QString qs = "Boundary " + QString::number(i) + ": " 
	+ QString::number(tmp[i]) + " surface elements";
      if(boundaryConditionEditor[i].touched) qs.append(" (BC set)");
      te->append(qs);
    }
  }
  te->append("Undetermined: " + QString::number(undetermined));
  te->append("Total: " + QString::number(count) + " surface boundaries");
  te->append("");

  delete [] tmp;


  // Count edge bodies:
  //---------------------
  undetermined = 0;
  tmp = new int[mesh->edges];
  for(int i = 0; i < mesh->edges; i++)
    tmp[i] = 0;

  for(int i = 0; i < mesh->edges; i++) {
    edge_t *e = &mesh->edge[i];
    if(e->nature == PDE_BULK) {
      if(e->index >= 0)
	tmp[e->index]++;
      else
	undetermined++;
    }
  }

  te->append("EDGE BODIES");
  count = 0;
  for(int i = 0; i<mesh->edges; i++) {
    if( tmp[i]>0 ) {
      count++;
      QString qs = "Body " + QString::number(i) + ": " 
	+ QString::number(tmp[i]) + " edge elements";
      if(bodyPropertyEditor[i].touched) 
	qs.append(" (Body property set)");
      te->append(qs);
    }
  }
  te->append("Undetermined: " + QString::number(undetermined));
  te->append("Total: " + QString::number(count) + " edge bodies");
  te->append("");

  delete [] tmp;

  // Count edge boundaries:
  //--------------------------
  undetermined = 0;
  tmp = new int[mesh->edges];
  for(int i = 0; i < mesh->edges; i++)
    tmp[i] = 0;
  
  for(int i = 0; i < mesh->edges; i++) {
    edge_t *e = &mesh->edge[i];
    if(e->nature == PDE_BOUNDARY) {
      if(e->index >= 0)
	tmp[e->index]++;
      else
	undetermined++;
    }
  }

  te->append("EDGE BOUNDARIES");
  count = 0;
  for(int i = 0; i<mesh->edges; i++) {
    if( tmp[i]>0 ) {
      count++;
      QString qs = "Boundary " + QString::number(i) + ": " 
	+ QString::number(tmp[i]) + " edge elements";
      if( boundaryConditionEditor[i].touched) qs.append(" (BC set)");
      te->append(qs);
    }
  }
  te->append("Undetermined: " + QString::number(undetermined));
  te->append("Total: " + QString::number(count) + " edge boundaries");
  te->append("");

  delete [] tmp;
}


// Model -> Clear
//-----------------------------------------------------------------------------
void MainWindow::modelClearSlot()
{
  // clear equations:
  for(int i = 0; i < limit->maxEquations(); i++) {
    DynamicEditor *pe = &equationEditor[i];
    if(pe->menuAction != NULL)
      delete pe->menuAction;
  }
  delete [] equationEditor;
  equationEditor = new DynamicEditor[limit->maxEquations()];

  // clear materials:
  for(int i = 0; i < limit->maxMaterials(); i++) {
    DynamicEditor *de = &materialEditor[i];
    if(de->menuAction != NULL)
      delete de->menuAction;
  }
  delete [] materialEditor;
  materialEditor = new DynamicEditor[limit->maxMaterials()];

  // clear body forces:
  for(int i = 0; i < limit->maxBodyforces(); i++) {
    DynamicEditor *de = &bodyForceEditor[i];
    if(de->menuAction != NULL)
      delete de->menuAction;
  }
  delete [] bodyForceEditor;
  bodyForceEditor = new DynamicEditor[limit->maxBodyforces()];

  // clear initial conditions:
  for(int i = 0; i < limit->maxInitialconditions(); i++) {
    DynamicEditor *de = &initialConditionEditor[i];
    if(de->menuAction != NULL)
      delete de->menuAction;
  }
  delete [] initialConditionEditor;
  initialConditionEditor = new DynamicEditor[limit->maxInitialconditions()];

  // clear boundary conditions:
  for(int i = 0; i < limit->maxBcs(); i++) {
    DynamicEditor *de = &boundaryConditionEditor[i];
    if(de->menuAction != NULL)
      delete de->menuAction;
  }
  delete [] boundaryConditionEditor;
  boundaryConditionEditor = new DynamicEditor[limit->maxBcs()];

  // clear boundary setting:
  delete [] boundaryPropertyEditor;
  boundaryPropertyEditor = new BoundaryPropertyEditor[limit->maxBoundaries()];

  // clear body settings:
  delete [] bodyPropertyEditor;
  bodyPropertyEditor = new BodyPropertyEditor[limit->maxBodies()];
}


//*****************************************************************************
//
//                                View MENU
//
//*****************************************************************************


// View -> Surface mesh
//-----------------------------------------------------------------------------
void MainWindow::hidesurfacemeshSlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("There is no surface mesh to hide/show");
    return;
  }
  
  glWidget->stateDrawSurfaceMesh = !glWidget->stateDrawSurfaceMesh;

  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->type == SURFACEMESHLIST) 
    {
      l->visible = glWidget->stateDrawSurfaceMesh;

      // do not set visible if the parent surface list is hidden
      int p = l->parent;
      if(p >= 0) {
	list_t *lp = &list[p];
	if(!lp->visible)
	  l->visible = false;
      }
    }
  }

  synchronizeMenuToState();  

  if(!glWidget->stateDrawSurfaceMesh) 
    logMessage("Surface mesh hidden");
  else
    logMessage("Surface mesh shown");
}


// View -> Volume mesh
//-----------------------------------------------------------------------------
void MainWindow::hidevolumemeshSlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("There is no volume mesh to hide/show");
    return;
  }

  glWidget->stateDrawVolumeMesh = !glWidget->stateDrawVolumeMesh;


  for(int i = 0; i < lists; i++) {
    list_t *l = &list[i];
    if(l->type == VOLUMEMESHLIST)
      l->visible = glWidget->stateDrawVolumeMesh;
  }


  synchronizeMenuToState();  

  if(!glWidget->stateDrawVolumeMesh) 
    logMessage("Volume mesh hidden");
  else
    logMessage("Volume mesh shown");

}


// View -> Sharp edges
//-----------------------------------------------------------------------------
void MainWindow::hidesharpedgesSlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("There are no sharp edges to hide/show");
    return;
  }

  glWidget->stateDrawSharpEdges = !glWidget->stateDrawSharpEdges;

  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->type == SHARPEDGELIST)  
      l->visible = glWidget->stateDrawSharpEdges;
  }

  
  synchronizeMenuToState();

  if ( !glWidget->stateDrawSharpEdges ) 
    logMessage("Sharp edges hidden");
  else 
    logMessage("Sharp edges shown");
}


// View -> Coordinates
//-----------------------------------------------------------------------------
void MainWindow::viewCoordinatesSlot()
{
  if( glWidget->toggleCoordinates() )
    logMessage("Coordinates shown");
  else 
    logMessage("Cordinates hidden");

  synchronizeMenuToState();
}



// View -> Select defined edges
//-----------------------------------------------------------------------------
void MainWindow::selectDefinedEdgesSlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("There are no entities from which to select");
    return;
  }

  // At the moment only edges are included in search:
  int nmax = 0;
  for( int i=0; i<glWidget->boundaryMap.count(); i++ ) {
    int n = glWidget->boundaryMap.key(i);
    if(n > nmax) nmax = n;
  }

  bool *activeboundary = new bool[nmax+1];
  for (int i=0;i<=nmax;i++)
    activeboundary[i] = false;

  for( int i=0; i<glWidget->boundaryMap.count(); i++ ) {
    int n=glWidget->boundaryMap.key(i);
    if ( n >= 0 ) {
      int m = glWidget->boundaryMap.value(n);
      BoundaryPropertyEditor *boundary = &boundaryPropertyEditor[m];
      activeboundary[n] = boundary->condition;
    }
  }

  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->type == EDGELIST) {
      int j = l->index;
      if( j < 0 ) continue;
      
      // *** TODO ***
      //
      // This is wrong: Comparing body indices with boundary indexes
      if( activeboundary[j] ) l->selected = true;
    }
  }

  for( int i = 0; i < mesh->edges; i++ ) {
    edge_t *edge = &mesh->edge[i];
    if( edge->nature == PDE_BOUNDARY ) { 
      int j = edge->index;
      if( j < 0) continue;
      if( activeboundary[j] ) edge->selected = true;
    }
  }
  delete [] activeboundary;

  glWidget->rebuildEdgeLists();
  glWidget->updateGL();
  
  logMessage("Defined edges selected");
}


// View -> Select defined surfaces
//-----------------------------------------------------------------------------
void MainWindow::selectDefinedSurfacesSlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("There are no entities from which to select");
    return;
  }

  // At the moment only surfaces are included in search:
  int nmax = 0;
  for( int i=0; i<glWidget->bodyMap.count(); i++ ) {
    int n = glWidget->bodyMap.key(i);
    if(n > nmax) nmax = n;
  }

  bool *activebody = new bool[nmax+1];
  for (int i=0;i<=nmax;i++)
    activebody[i] = false;

  for( int i=0; i<glWidget->bodyMap.count(); i++ ) {
    int n=glWidget->bodyMap.key(i);
    if ( n >= 0 ) {
      int m = glWidget->bodyMap.value(n);
      BodyPropertyEditor *body = &bodyPropertyEditor[m];
      activebody[n] = body->material && body->equation;
    }
  }

  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->type == SURFACELIST) {
      int j = l->index;
      if( j < 0 ) continue;

      // *** TODO ***
      //
      // This is wrong: Comparing body indices with boundary indexes
      if( activebody[j] ) l->selected = true;
    }
  }

  for( int i = 0; i < mesh->surfaces; i++ ) {
    surface_t *surface = &mesh->surface[i];
    if( surface->nature == PDE_BULK ) { 
      int j = surface->index;
      if( j < 0) continue;
      if( activebody[j] ) surface->selected = true;
    }
  }
  delete [] activebody;

  glWidget->rebuildSurfaceLists();
  glWidget->updateGL();
  
  logMessage("Defined surfaces selected");
}



// View -> Select all surfaces
//-----------------------------------------------------------------------------
void MainWindow::selectAllSurfacesSlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("There are no surfaces to select");
    return;
  }

  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->type == SURFACELIST)
    {
      l->selected = true;
      for( int j=0; j<mesh->surfaces; j++ ) {
        surface_t *surf = &mesh->surface[j];
        if( l->index == surf->index )
          surf->selected=l->selected;
      }
    }
  }

  glWidget->rebuildSurfaceLists();
  glWidget->updateGL();
  
  logMessage("All surfaces selected");
}



// View -> Select all edges
//-----------------------------------------------------------------------------
void MainWindow::selectAllEdgesSlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("There are no edges to select");
    return;
  }

  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->type == EDGELIST)
      l->selected = true;
      for( int j=0; j<mesh->edges; j++ ) {
        edge_t *edge = &mesh->edge[j];
        if( l->index == edge->index )
          edge->selected = l->selected;
      }
  }

  glWidget->rebuildEdgeLists();
  glWidget->updateGL();
  
  logMessage("All edges selected");
}



// View -> Hide/Show selected
//-----------------------------------------------------------------------------
void MainWindow::hideselectedSlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("There is nothing to hide/show");
    return;
  }

  bool something_selected = false;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    something_selected |= l->selected;
  }

  if(!something_selected) {
    logMessage("Nothing selected");
    return;
  }

  bool vis = false;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->selected) {
      l->visible = !l->visible;
      if(l->visible)
	vis = true;

      // hide the child surface edge list if parent is hidden
      int c = l->child;
      if(c >= 0) {
	list_t *lc = &list[c];
	lc->visible = l->visible;
	if(!glWidget->stateDrawSurfaceMesh)
	  lc->visible = false;
      }
    }
  }
  glWidget->updateGL();
  
  if( !vis )
    logMessage("Selected objects hidden");
  else 
    logMessage("Selected objects shown");
}



// View -> Show all
//-----------------------------------------------------------------------------
void MainWindow::showallSlot()
{
  int lists = glWidget->lists;
  list_t *list = glWidget->list;
  
  glWidget->stateDrawSurfaceMesh = true;
  glWidget->stateDrawSharpEdges = true;
  glWidget->stateDrawSurfaceElements = true;
  glWidget->stateDrawEdgeElements = true;

  synchronizeMenuToState();

  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    l->visible = true;
  }

  logMessage("All objects visible");
}



// View -> Reset model view
//-----------------------------------------------------------------------------
void MainWindow::resetSlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;
  
  if(mesh == NULL) {
    logMessage("There is nothing to reset");
    return;
  }

  glWidget->stateFlatShade = true;
  glWidget->stateDrawSurfaceMesh = true;
  glWidget->stateDrawSharpEdges = true;
  glWidget->stateDrawSurfaceElements = true;
  glWidget->stateDrawEdgeElements = true;
  glWidget->stateDrawSurfaceNumbers = false;
  glWidget->stateDrawEdgeNumbers = false;
  glWidget->stateDrawNodeNumbers = false;

  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    l->visible = true;
    l->selected = false;

    for( int j=0; j<mesh->surfaces; j++ ) {
      surface_t *surf = &mesh->surface[j];
      if( l->index == surf->index )
        surf->selected=l->selected;
    }
    for( int j=0; j<mesh->edges; j++ ) {
      edge_t *edge = &mesh->edge[j];
      if( l->index == edge->index )
        edge->selected=l->selected;
    }
  }

  glWidget->stateBcColors = false;
  glWidget->stateBodyColors = false;

  glLoadIdentity();
  glWidget->rebuildLists();
  glWidget->updateGL();

  synchronizeMenuToState();
  logMessage("Reset model view");
}



// View -> Shade model -> Flat
//-----------------------------------------------------------------------------
void MainWindow::flatShadeSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Refusing to change shade model when mesh is empty");
    return;
  }

  glWidget->stateFlatShade = true;
  glWidget->rebuildSurfaceLists();
  glWidget->updateGL();

  synchronizeMenuToState();
  logMessage("Shade model: flat");
}


// View -> Shade model -> Smooth
//-----------------------------------------------------------------------------
void MainWindow::smoothShadeSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Refusing to change shade model when mesh is empty");
    return;
  }

  glWidget->stateFlatShade = false;
  glWidget->rebuildSurfaceLists();
  glWidget->updateGL();

  synchronizeMenuToState();
  logMessage("Shade model: smooth");
}


// View -> Show numbering -> Surface numbering
//-----------------------------------------------------------------------------
void MainWindow::showSurfaceNumbersSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Refusing to show surface element numbering when mesh is empty");
    return;
  }
  glWidget->stateDrawSurfaceNumbers = !glWidget->stateDrawSurfaceNumbers;
  glWidget->updateGL();
  synchronizeMenuToState();

  if(glWidget->stateDrawSurfaceNumbers) 
    logMessage("Surface element numbering turned on");
  else
    logMessage("Surface element numbering turned off");   
}

// View -> Show numbering -> Edge numbering
//-----------------------------------------------------------------------------
void MainWindow::showEdgeNumbersSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Refusing to show edge element numbering when mesh is empty");
    return;
  }
  glWidget->stateDrawEdgeNumbers = !glWidget->stateDrawEdgeNumbers;
  glWidget->updateGL();
  synchronizeMenuToState();

  if(glWidget->stateDrawEdgeNumbers) 
    logMessage("Edge element numbering turned on");
  else
    logMessage("Edge element numbering turned off");   
}

// View -> Numbering -> Node numbers
//-----------------------------------------------------------------------------
void MainWindow::showNodeNumbersSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Refusing to show node numbering when mesh is empty");
    return;
  }
  glWidget->stateDrawNodeNumbers = !glWidget->stateDrawNodeNumbers;
  glWidget->updateGL();
  synchronizeMenuToState();
  
  if(glWidget->stateDrawNodeNumbers) 
    logMessage("Node numbering turned on");
  else 
    logMessage("Node numbering turned off");    
}

// View -> Numbering -> Boundary index
//-----------------------------------------------------------------------------
void MainWindow::showBoundaryIndexSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Refusing to show boundary indices when mesh is empty");
    return;
  }
  glWidget->stateDrawBoundaryIndex = !glWidget->stateDrawBoundaryIndex;
  glWidget->updateGL();
  synchronizeMenuToState();
  
  if(glWidget->stateDrawBoundaryIndex) 
    logMessage("Boundary indices visible");
  else 
    logMessage("Boundary indices hidden");    
}


// View -> Numbering -> Body index
//-----------------------------------------------------------------------------
void MainWindow::showBodyIndexSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Refusing to show body indices when mesh is empty");
    return;
  }

  glWidget->stateDrawBodyIndex = !glWidget->stateDrawBodyIndex;
  glWidget->updateGL();
  synchronizeMenuToState();
  
  if(glWidget->stateDrawBodyIndex) 
    logMessage("Body indices visible");
  else 
    logMessage("Body indices hidden");    
}


// View -> Colors -> GL controls
//-----------------------------------------------------------------------------
void MainWindow::glControlSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("No mesh - unable to set GL parameters when the mesh is empty");
    return;
  }

  glControl->glWidget = this->glWidget;
  glControl->show();
}


// View -> Colors -> Boundaries
//-----------------------------------------------------------------------------
void MainWindow::colorizeBoundarySlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("No mesh - unable to colorize boundaries");
    return;
  }

  glWidget->stateBcColors = !glWidget->stateBcColors;

  if(glWidget->stateBcColors)
    glWidget->stateBodyColors = false;

  glWidget->rebuildLists();
  synchronizeMenuToState();
}

// View -> Colors -> Bodies
//-----------------------------------------------------------------------------
void MainWindow::colorizeBodySlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("No mesh - unable to colorize bodies");
    return;
  }

  glWidget->stateBodyColors = !glWidget->stateBodyColors;

  if(glWidget->stateBodyColors)
    glWidget->stateBcColors = false;

  glWidget->rebuildLists();
  synchronizeMenuToState();
}


// View -> Colors -> Background
//-----------------------------------------------------------------------------
void MainWindow::backgroundColorSlot()
{
  QColor newColor = QColorDialog::getColor(glWidget->backgroundColor, this);
  glWidget->qglClearColor(newColor);
  glWidget->backgroundColor = newColor;
}



// View -> Colors -> Surface
//-----------------------------------------------------------------------------
void MainWindow::surfaceColorSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Unable to change surface color when the mesh is empty");
    return;
  }

  QColor newColor = QColorDialog::getColor(glWidget->surfaceColor, this);
  glWidget->surfaceColor = newColor;
  glWidget->rebuildLists();
}


// View -> Colors -> Edge
//-----------------------------------------------------------------------------
void MainWindow::edgeColorSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Unable to change edge color when the mesh is empty");
    return;
  }

  QColor newColor = QColorDialog::getColor(glWidget->edgeColor, this);
  glWidget->edgeColor = newColor;
  glWidget->rebuildLists();
}


// View -> Colors -> Surface mesh
//-----------------------------------------------------------------------------
void MainWindow::surfaceMeshColorSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Unable to change surface mesh color when the mesh is empty");
    return;
  }

  QColor newColor = QColorDialog::getColor(glWidget->surfaceMeshColor, this);
  glWidget->surfaceMeshColor = newColor;
  glWidget->rebuildLists();
}


// View -> Colors -> Sharp edges
//-----------------------------------------------------------------------------
void MainWindow::sharpEdgeColorSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("Unable to change sharp edge colors when the mesh is empty");
    return;
  }

  QColor newColor = QColorDialog::getColor(glWidget->sharpEdgeColor, this);
  glWidget->sharpEdgeColor = newColor;
  glWidget->rebuildLists();
}


// View -> Cad model...
//-----------------------------------------------------------------------------
void MainWindow::showCadModelSlot()
{
#ifdef OCC62
/*    if(cadView->shapes.IsNull() || cadView->shapes->IsEmpty()) {
		logMessage("There are no shapes to show. Open a cad file first.");
		return;
    }

	cadView->show();
	cadView->drawModel();*/
#endif
}



//*****************************************************************************
//
//                                Mesh MENU
//
//*****************************************************************************


// Mesh -> Control...
//-----------------------------------------------------------------------------
void MainWindow::meshcontrolSlot()
{
  meshControl->tetlibPresent = this->tetlibPresent;
  meshControl->nglibPresent = this->nglibPresent;

  if(!tetlibPresent) {
    meshControl->tetlibPresent = false;
    meshControl->ui.nglibRadioButton->setChecked(true);
    meshControl->ui.tetlibRadioButton->setEnabled(false);
    meshControl->ui.tetlibStringEdit->setEnabled(false);
  }

  if(!nglibPresent) {
    meshControl->nglibPresent = false;
    meshControl->ui.tetlibRadioButton->setChecked(true);
    meshControl->ui.nglibRadioButton->setEnabled(false);
    meshControl->ui.nglibMaxHEdit->setEnabled(false);
    meshControl->ui.nglibFinenessEdit->setEnabled(false);
    meshControl->ui.nglibBgmeshEdit->setEnabled(false);
  }

  if(!tetlibPresent && !nglibPresent) 
    meshControl->ui.elmerGridRadioButton->setChecked(true);  

  meshControl->show();
}



// Mesh -> Remesh
//-----------------------------------------------------------------------------
void MainWindow::remeshSlot()
{
  if(activeGenerator == GEN_UNKNOWN) {
    logMessage("Unable to (re)mesh: no input data or mesh generator");
    return;
  }
  
    
  // ***** ELMERGRID *****

  if(activeGenerator == GEN_ELMERGRID) {
    meshutils->clearMesh(glWidget->mesh);
    glWidget->mesh = new mesh_t;
    mesh_t *mesh = glWidget->mesh;
    
    elmergridAPI->createElmerMeshStructure(mesh, 
             meshControl->elmerGridControlString.toAscii());
    
    if(mesh->surfaces == 0) meshutils->findSurfaceElements(mesh);
    
    for(int i=0; i<mesh->surfaces; i++ )
      {
	surface_t *surface = &mesh->surface[i];
	
	surface->edges = (int)(surface->code/100);
	surface->edge = new int[surface->edges];
	for(int j=0; j<surface->edges; j++)
	  surface->edge[j] = -1;
      }

    meshutils->findSurfaceElementEdges(mesh);
    meshutils->findSurfaceElementNormals(mesh);

    glWidget->rebuildLists();
    applyOperations();
    
    return;
  }
  
  // ***** Threaded generators *****

  if(!remeshAct->isEnabled()) {
    logMessage("Meshing thread is already running - aborting");
    return;
  }

  if(activeGenerator == GEN_TETLIB) {

    if(!tetlibPresent) {
      logMessage("tetlib functionality unavailable");
      return;
    }
    
    if(!tetlibInputOk) {
      logMessage("Remesh: error: no input data for tetlib");
      return;
    }

    // must have "J" in control string:
    tetlibControlString = meshControl->tetlibControlString;

  } else if(activeGenerator == GEN_NGLIB) {

    if(!nglibPresent) {
      logMessage("nglib functionality unavailable");
      return;
    }

    if(!nglibInputOk) {
      logMessage("Remesh: error: no input data for nglib");
      return;
    }

    char backgroundmesh[1024];
    sprintf(backgroundmesh, "%s",
	    (const char*)(meshControl->nglibBackgroundmesh.toAscii()));
    
    ngmesh = nglibAPI->Ng_NewMesh();
    
    mp->maxh = meshControl->nglibMaxH.toDouble();
    mp->fineness = meshControl->nglibFineness.toDouble();
    mp->secondorder = 0;
    mp->meshsize_filename = backgroundmesh;

  } else {

    logMessage("Remesh: uknown generator type");
    return;

  }

  // ***** Start meshing thread *****

  logMessage("Sending start request to mesh generator...");

  meshutils->clearMesh(glWidget->mesh);
  glWidget->mesh = new mesh_t;
  mesh_t *mesh = glWidget->mesh;

  // Re-enable when finished() or terminated() signal is received:
  remeshAct->setEnabled(false);
  stopMeshingAct->setEnabled(true);
  glWidget->enableIndicator(true);

  meshingThread->generate(activeGenerator, tetlibControlString,
			  tetlibAPI, ngmesh, nggeom, mp, nglibAPI);
}



// Mesh -> Kill generator
//-----------------------------------------------------------------------------
void MainWindow::stopMeshingSlot()
{
  if(remeshAct->isEnabled()) {
    logMessage("Mesh generator is not running");
    return;
  }

  logMessage("Sending termination request to mesh generator...");
  meshingThread->stopMeshing();
}



// Meshing has started (signaled by meshingThread):
//-----------------------------------------------------------------------------
void MainWindow::meshingStartedSlot()
{
  logMessage("Mesh generator started");

  updateSysTrayIcon("Mesh generator started",
		    "Use Mesh->Terminate to stop processing");

  statusBar()->showMessage(tr("Mesh generator started"));
}


// Meshing has been terminated (signaled by meshingThread):
//-----------------------------------------------------------------------------
void MainWindow::meshingTerminatedSlot()
{
  logMessage("Mesh generator terminated");

  updateSysTrayIcon("Mesh generator terminated",
		    "Use Mesh->Remesh to restart");

  statusBar()->showMessage(tr("Ready"));
  
  // clean up:
  if(activeGenerator == GEN_TETLIB) {
    cout << "Cleaning up...";
    out->deinitialize();
    cout << "done" << endl;
    cout.flush();
  }

  remeshAct->setEnabled(true);
  stopMeshingAct->setEnabled(false);
  glWidget->enableIndicator(false);
}

// Mesh is ready (signaled by meshingThread):
//-----------------------------------------------------------------------------
void MainWindow::meshingFinishedSlot()
{
  logMessage("Mesh generation ready");

  if(activeGenerator == GEN_TETLIB) {

    makeElmerMeshFromTetlib();

  } else if(activeGenerator == GEN_NGLIB) {

    makeElmerMeshFromNglib();

  } else {
    
    logMessage("MeshOk: error: unknown mesh generator");

  }

  applyOperations();

  statusBar()->showMessage(tr("Ready"));
  
  updateSysTrayIcon("Mesh generator has finished",
		    "Select Model->Summary for statistics");

  remeshAct->setEnabled(true);
  stopMeshingAct->setEnabled(false);
  glWidget->enableIndicator(false);
}


// Mesh -> Divide surface...
//-----------------------------------------------------------------------------
void MainWindow::surfaceDivideSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("There is nothing to divide - mesh is empty");
    return;
  }

  boundaryDivide->target = TARGET_SURFACES;
  boundaryDivide->show();
}



// Make surface division by sharp edges (signalled by boundaryDivide)...
//-----------------------------------------------------------------------------
void MainWindow::doDivideSurfaceSlot(double angle)
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("No mesh to divide");
    return;
  }
  
  operations++;
  operation_t *p = new operation_t, *q;
  for( q=&operation; q->next; q=q->next );
  q->next = p;
  p->next = NULL;

  p->type = OP_DIVIDE_SURFACE;
  p->angle = angle;

  int selected=0;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->selected && l->type==SURFACELIST && l->nature==PDE_BOUNDARY)
      selected++;
  }
  p->selected = selected;
  p->select_set = new int[selected];
  selected = 0;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];    
    if(l->selected && l->type == SURFACELIST && l->nature==PDE_BOUNDARY)
      p->select_set[selected++] = i;
  }
  

  meshutils->findSharpEdges(mesh, angle);
  int parts = meshutils->divideSurfaceBySharpEdges(mesh);

  QString qs = "Surface divided into " + QString::number(parts) + " parts";
  statusBar()->showMessage(qs);
  
  synchronizeMenuToState();
  glWidget->rebuildLists();
  glWidget->updateGL();
}



// Mesh -> Unify surface
//-----------------------------------------------------------------------------
void MainWindow::surfaceUnifySlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("No surfaces to unify");
    return;
  }
  
  int targetindex = -1, selected=0;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->selected && (l->type == SURFACELIST) && (l->nature == PDE_BOUNDARY)) {
      selected++;
      if(targetindex < 0) targetindex = l->index;
    }
  }
  
  if(targetindex < 0) {
    logMessage("No surfaces selected");
    return;
  }


  operations++;
  operation_t *p = new operation_t, *q;
  for( q=&operation; q->next; q=q->next );
  q->next = p;
  p->next = NULL;
  p->type = OP_UNIFY_SURFACE;
  p->selected=selected;
  p->select_set = new int[selected]; 
  
  selected = 0;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];    
    if(l->selected && (l->type == SURFACELIST) && (l->nature == PDE_BOUNDARY)) {
      p->select_set[selected++] = i;
      for(int j=0; j < mesh->surfaces; j++) {
	surface_t *s = &mesh->surface[j];
	if((s->index == l->index) && (s->nature == PDE_BOUNDARY)) 
	  s->index = targetindex;
      }
    }
  }
  
  cout << "Selected surfaces marked with index " << targetindex << endl;
  cout.flush();

  glWidget->rebuildLists();

  logMessage("Selected surfaces unified");
}


void MainWindow::applyOperations()
{
  mesh_t *mesh = glWidget->mesh;

  cout << "Apply " << operations << " operations" << endl;
  cout.flush();

  operation_t *p = operation.next;
  for( ; p; p=p->next )
  {
    int lists = glWidget->lists;
    list_t *list = glWidget->list;

    for(int i=0; i<p->selected; i++) {
      list_t *l = &list[p->select_set[i]];

      l->selected = true;
      if ( p->type < OP_UNIFY_EDGE ) {
        for( int j=0; j<mesh->surfaces; j++ ) {
          surface_t *surf = &mesh->surface[j];
          if( l->index == surf->index )
            surf->selected=l->selected;
        }
      } else {
        for( int j=0; j<mesh->edges; j++ ) {
          edge_t *edge = &mesh->edge[j];
          if( l->index == edge->index )
            edge->selected=l->selected;
        }
      }
    }

    if ( p->type == OP_DIVIDE_SURFACE ) {
      meshutils->findSharpEdges(mesh, p->angle);
      int parts = meshutils->divideSurfaceBySharpEdges(mesh);
      QString qs = "Surface divided into " + QString::number(parts) + " parts";
      statusBar()->showMessage(qs);

    } else if ( p->type == OP_DIVIDE_EDGE ) {
      meshutils->findEdgeElementPoints(mesh);
      meshutils->findSharpPoints(mesh, p->angle);
      int parts = meshutils->divideEdgeBySharpPoints(mesh);
      QString qs = "Edges divided into " + QString::number(parts) + " parts";
      statusBar()->showMessage(qs);

    } else if (p->type == OP_UNIFY_SURFACE ) {
      int targetindex = -1;
      for(int i=0; i<lists; i++) {
        list_t *l = &list[i];
        if(l->selected && (l->type == SURFACELIST) && (l->nature == PDE_BOUNDARY)) {
          if(targetindex < 0) {
            targetindex = l->index;
            break;
          }
        }
      }
      for(int i=0; i<lists; i++) {
        list_t *l = &list[i];    
        if(l->selected && (l->type == SURFACELIST) && (l->nature == PDE_BOUNDARY)) {
          for(int j=0; j < mesh->surfaces; j++) {
            surface_t *s = &mesh->surface[j];
            if((s->index == l->index) && (s->nature == PDE_BOUNDARY)) 
              s->index = targetindex;
          }
        }
      }
      cout << "Selected surfaces marked with index " << targetindex << endl;
      cout.flush();

    } else if (p->type == OP_UNIFY_EDGE ) {
      int targetindex = -1;
      for(int i=0; i<lists; i++) {
        list_t *l = &list[i];
        if(l->selected && l->type == EDGELIST && l->nature == PDE_BOUNDARY) {
          if(targetindex < 0) {
            targetindex = l->index;
            break;
          }
        }
      }
      for(int i=0; i<lists; i++) {
        list_t *l = &list[i];    
        if(l->selected && l->type == EDGELIST && l->nature == PDE_BOUNDARY) {
          for(int j=0; j < mesh->edges; j++) {
            edge_t *e = &mesh->edge[j];
            if(e->index == l->index && e->nature == PDE_BOUNDARY)
              e->index = targetindex;
          }
        }
      }
      cout << "Selected edges marked with index " << targetindex << endl;
      cout.flush();
    }
    glWidget->rebuildLists();
  }
  

  synchronizeMenuToState();
  glWidget->updateGL();
}



// Mesh -> Divide edge...
//-----------------------------------------------------------------------------
void MainWindow::edgeDivideSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("There is nothing to divide - mesh is empty");
    return;
  }

  boundaryDivide->target = TARGET_EDGES;
  boundaryDivide->show();
}



// Make edge division by sharp points (signalled by boundaryDivide)...
//-----------------------------------------------------------------------------
void MainWindow::doDivideEdgeSlot(double angle)
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("No mesh to divide");
    return;
  }

  operations++;
  operation_t *p = new operation_t, *q;
  for( q=&operation; q->next; q=q->next );
  q->next = p;
  p->next = NULL;

  p->type = OP_DIVIDE_EDGE;
  p->angle = angle;

  int selected=0;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->selected && l->type==EDGELIST && l->nature==PDE_BOUNDARY)
      selected++;
  }
  p->selected = selected;
  p->select_set = new int[selected];
  selected = 0;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];    
    if(l->selected && l->type == EDGELIST && l->nature==PDE_BOUNDARY)
      p->select_set[selected++] = i;
  }
  

  meshutils->findEdgeElementPoints(mesh);
  meshutils->findSharpPoints(mesh, angle);
  int parts = meshutils->divideEdgeBySharpPoints(mesh);
  
  QString qs = "Edge divided into " + QString::number(parts) + " parts";
  statusBar()->showMessage(qs);

  synchronizeMenuToState();
  glWidget->rebuildLists();
  glWidget->updateGL(); 
}




// Mesh -> Unify edge
//-----------------------------------------------------------------------------
void MainWindow::edgeUnifySlot()
{
  mesh_t *mesh = glWidget->mesh;
  int lists = glWidget->lists;
  list_t *list = glWidget->list;

  if(mesh == NULL) {
    logMessage("No edges to unify");
    return;
  }
  
  int targetindex = -1, selected=0;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];
    if(l->selected && l->type == EDGELIST && l->nature == PDE_BOUNDARY) {
      selected++;
      if(targetindex < 0) targetindex = l->index;
    }
  }
  

  if(targetindex < 0) {
    logMessage("No edges selected");
    return;
  }

  operations++;
  operation_t *p = new operation_t, *q;
  for( q=&operation; q->next; q=q->next );
  q->next = p;
  p->next = NULL;
  p->type = OP_UNIFY_EDGE;
  p->selected=selected;
  p->select_set = new int[selected]; 
  
  selected = 0;
  for(int i=0; i<lists; i++) {
    list_t *l = &list[i];    
    if(l->selected && l->type == EDGELIST && l->nature == PDE_BOUNDARY) {
      p->select_set[selected++] = i;
      for(int j=0; j < mesh->edges; j++) {
	edge_t *e = &mesh->edge[j];
	if(e->index == l->index && e->nature == PDE_BOUNDARY) 
	  e->index = targetindex;
      }
    }
  }
  
  cout << "Selected edges marked with index " << targetindex << endl;
  cout.flush();

  glWidget->rebuildLists();

  logMessage("Selected edges unified");
}


// Mesh -> Clean up
//-----------------------------------------------------------------------------
void MainWindow::cleanHangingSharpEdgesSlot()
{
  mesh_t *mesh = glWidget->mesh;

  if(mesh == NULL)
    return;

  int count = meshutils->cleanHangingSharpEdges(mesh);

  cout << "Removed " << count << " hanging sharp edges" << endl;
  cout.flush();

  glWidget->rebuildLists();
}


//*****************************************************************************
//
//                                Edit MENU
//
//*****************************************************************************


// Edit -> Sif...
//-----------------------------------------------------------------------------
void MainWindow::showsifSlot()
{
  // QFont sansFont("Courier", 10);
  // sifWindow->textEdit->setCurrentFont(sansFont);
  sifWindow->show();
}


// Edit -> Generate sif
//-----------------------------------------------------------------------------
void MainWindow::generateSifSlot()
{
  mesh_t *mesh = glWidget->mesh;

  if(mesh == NULL) {
    logMessage("Unable to create SIF: no mesh");
    return;
  }
  
  if((mesh->dim < 1) || (mesh->cdim < 1)) {
    logMessage("Model dimension inconsistent with SIF syntax");
    return;
  }

  // Clear SIF text editor:
  //------------------------
  sifWindow->textEdit->clear();
  sifWindow->firstTime = true;
  sifWindow->found = false;
  // QFont sansFont("Courier", 10);
  // sifWindow->textEdit->setCurrentFont(sansFont);

  // Set up SIF generator:
  //-----------------------
  sifGenerator->mesh = mesh;
  sifGenerator->te = sifWindow->textEdit;
  sifGenerator->dim = mesh->dim;
  sifGenerator->cdim = mesh->cdim;

  sifGenerator->generalSetup = generalSetup;
  sifGenerator->equationEditor = equationEditor;
  sifGenerator->materialEditor = materialEditor;
  sifGenerator->bodyForceEditor = bodyForceEditor;
  sifGenerator->initialConditionEditor = initialConditionEditor;
  sifGenerator->boundaryConditionEditor = boundaryConditionEditor;

  sifGenerator->solverParameterEditor = solverParameterEditor;

  sifGenerator->boundaryPropertyEditor = boundaryPropertyEditor;
  sifGenerator->bodyPropertyEditor = bodyPropertyEditor;

  sifGenerator->meshControl = meshControl;

  sifGenerator->elmerDefs = elmerDefs;
  sifGenerator->bodyMap = glWidget->bodyMap;
  sifGenerator->boundaryMap = glWidget->boundaryMap;

  // Make SIF:
  //----------
  sifGenerator->makeHeaderBlock();
  sifGenerator->makeSimulationBlock();
  sifGenerator->makeConstantsBlock();
  sifGenerator->makeBodyBlocks();
  sifGenerator->makeEquationBlocks();
  // sifGenerator->makeSolverBlocks();
  sifGenerator->makeMaterialBlocks();
  sifGenerator->makeBodyForceBlocks();
  sifGenerator->makeInitialConditionBlocks();
  sifGenerator->makeBoundaryBlocks();
}



// Boundady selected by double clicking (signaled by glWidget::select):
//-----------------------------------------------------------------------------
void MainWindow::boundarySelectedSlot(list_t *l)
{
  QString qs;

  if(l->index < 0) {
    statusBar()->showMessage("Ready");    
    return;
  }

  if(l->selected) {
    if(l->type == SURFACELIST) {
      qs = "Selected surface " + QString::number(l->index);
    } else if(l->type == EDGELIST) {
      qs = "Selected edge " + QString::number(l->index);
    } else {
      qs = "Selected object " + QString::number(l->index) + " (type unknown)";
    }
  } else {
    if(l->type == SURFACELIST) {
      qs = "Unselected surface " + QString::number(l->index);
    } else if(l->type == EDGELIST) {
      qs = "Unselected edge " + QString::number(l->index);
    } else {
      qs = "Unselected object " + QString::number(l->index) + " (type unknown)";
    }
  }

  logMessage(qs);
  

  // Open bc property sheet for selected boundary:
  //-----------------------------------------------
  if(l->selected && (glWidget->altPressed || bcEditActive)) {
    glWidget->ctrlPressed = false;
    glWidget->shiftPressed = false;
    glWidget->altPressed = false;
    
    // renumbering:
    int n = glWidget->boundaryMap.value(l->index);
    if(n >= limit->maxBcs()) {
      logMessage("Error: index exceeds MAX_BCS (increase it in egini.xml)");
      return;
    }
    
    BoundaryPropertyEditor *boundaryEdit = &boundaryPropertyEditor[n];
    populateBoundaryComboBoxes(boundaryEdit);

    connect( boundaryEdit, SIGNAL(BoundaryAsABodyChanged(BoundaryPropertyEditor *,int)),
          this, SLOT(boundaryAsABodyChanged(BoundaryPropertyEditor *,int)) );
    
    if(boundaryEdit->touched) {
      boundaryEdit->ui.applyButton->setText("Update");
      // boundaryEdit->ui.discardButton->setText("Remove");
      boundaryEdit->ui.discardButton->setText("Cancel");
      boundaryEdit->ui.applyButton->setIcon(QIcon(":/icons/dialog-ok-apply.png"));
      // boundaryEdit->ui.discardButton->setIcon(QIcon(":/icons/list-remove.png"));
      boundaryEdit->ui.discardButton->setIcon(QIcon(":/icons/dialog-close.png"));
    } else {
      boundaryEdit->ui.applyButton->setText("Add");
      boundaryEdit->ui.discardButton->setText("Cancel");
      boundaryEdit->ui.applyButton->setIcon(QIcon(":/icons/list-add.png"));
      boundaryEdit->ui.discardButton->setIcon(QIcon(":/icons/dialog-close.png"));
    }

    boundaryEdit->setWindowTitle("Properties for boundary " + QString::number(l->index));
    boundaryEdit->show();
  }

  BodyPropertyEditor *bodyEdit = NULL;
  int current = -1, n =-1;

  // boundary as a body treatment
  // ----------------------------
  if(l->selected && glWidget->ctrlPressed ) {

    // renumbering:
    int n = glWidget->boundaryMap.value(l->index);
    if(n >= limit->maxBcs()) {
      logMessage("Error: index exceeds MAX_BCS (increase it in egini.xml)");
      return;
    }
    BoundaryPropertyEditor *boundaryEdit = &boundaryPropertyEditor[n];
    bodyEdit = boundaryEdit->bodyProperties;
    if ( bodyEdit ) {
      glWidget->ctrlPressed = false;
      glWidget->shiftPressed = false;
      glWidget->altPressed = false;

      bodyEdit->setWindowTitle("Properties for body " + QString::number(current));
      bodyEdit->ui.nameEdit->setText("Body Property{Boundary " + QString::number(n+1) +  "}");
    }
  }

  // Open body property sheet for selected body:
  //---------------------------------------------
  if( (glWidget->currentlySelectedBody >= 0) &&
      (glWidget->shiftPressed || bodyEditActive) ) {
    
    glWidget->ctrlPressed = false;
    glWidget->shiftPressed = false;
    glWidget->altPressed = false;
    
    current = glWidget->currentlySelectedBody;
    
    cout << "Current selection uniquely determines body: " << current << endl;
    cout.flush();
 
    // renumbering:
    n = glWidget->bodyMap.value(current);
    if(n >= limit->maxBodies()) {
      logMessage("Error: index exceeds MAX_BODIES (increase it in egini.xml)");
      return;
    }
     
    bodyEdit =  &bodyPropertyEditor[n];
    bodyEdit->setWindowTitle("Properties for body " + QString::number(current));
    bodyEdit->ui.nameEdit->setText("Body Property " + QString::number(n+1));
  }

  if ( bodyEdit ) {
    populateBodyComboBoxes(bodyEdit);
    
    if(bodyEdit->touched) {
      bodyEdit->ui.applyButton->setText("Update");
      // bodyEdit->ui.discardButton->setText("Remove");
      bodyEdit->ui.discardButton->setText("Cancel");
      bodyEdit->ui.applyButton->setIcon(QIcon(":/icons/dialog-ok-apply.png"));
      // bodyEdit->ui.discardButton->setIcon(QIcon(":/icons/list-remove.png"));
      bodyEdit->ui.discardButton->setIcon(QIcon(":/icons/dialog-close.png"));
    } else {
      bodyEdit->ui.applyButton->setText("Add");
      bodyEdit->ui.discardButton->setText("Cancel");
      bodyEdit->ui.applyButton->setIcon(QIcon(":/icons/list-add.png"));
      bodyEdit->ui.discardButton->setIcon(QIcon(":/icons/dialog-close.png"));
    }

    bodyEdit->show();
  }
}


// Populate boundary editor's comboboxes:
//---------------------------------------
void MainWindow::populateBoundaryComboBoxes(BoundaryPropertyEditor *boundary)
{
  boundary->disconnect(SIGNAL(BoundaryComboChanged(BoundaryPropertyEditor *,QString)));
  while(boundary && boundary->ui.boundaryConditionCombo && boundary->ui.boundaryConditionCombo->count() > 0) 
    boundary->ui.boundaryConditionCombo->removeItem(0);
    
  int takethis = 1; //-1;
  int count = 1;
  boundary->ui.boundaryConditionCombo->insertItem(count++, "");

  for(int i = 0; i < limit->maxBcs(); i++) {
    DynamicEditor *bcEdit = &boundaryConditionEditor[i];
    if(bcEdit->menuAction != NULL) {
      const QString &name = bcEdit->nameEdit->text().trimmed();
      boundary->ui.boundaryConditionCombo->insertItem(count, name);
      if ( boundary->condition == bcEdit ) takethis = count;
      count++;
    }
  }
  connect( boundary,SIGNAL(BoundaryComboChanged(BoundaryPropertyEditor *,QString)), 
        this, SLOT(boundaryComboChanged(BoundaryPropertyEditor *,QString)) );

  boundary->ui.boundaryConditionCombo->setCurrentIndex(takethis-1);
}

//-----------------------------------------------------------------------------
void MainWindow::boundaryComboChanged(BoundaryPropertyEditor *b, QString text)
{
  b->condition = 0;
  b->touched = false;

  for( int i=0; i < limit->maxBcs(); i++ )
  {
    DynamicEditor *bc = &boundaryConditionEditor[i];
    if ( bc->ID >= 0 ) {
       if ( bc->nameEdit->text().trimmed()==text ) {
         b->condition = bc; 
         b->touched = true;
         break;
       }
    }
  }
}

//-----------------------------------------------------------------------------
void MainWindow::boundaryAsABodyChanged(BoundaryPropertyEditor *b, int status)
{
  int indx=glWidget->bodyMap.count();

  if ( status ) {
    for( int i=0; i < limit->maxBoundaries(); i++ )
      if ( boundaryPropertyEditor[i].bodyProperties ) indx++;
    b->bodyProperties = &bodyPropertyEditor[indx];
  } else {
    b->bodyProperties = NULL;
  }
}

    
// Populate body editor's comboboxes:
//-----------------------------------
void MainWindow::populateBodyComboBoxes(BodyPropertyEditor *bodyEdit)
{
  // Equation:
  // =========
  bodyEdit->disconnect(SIGNAL(BodyEquationComboChanged(BodyPropertyEditor *,QString)));
  while(bodyEdit->ui.equationCombo->count() > 0) 
    bodyEdit->ui.equationCombo->removeItem(0);

  int count = 1;
  int takethis = 1; // -1
  bodyEdit->ui.equationCombo->insertItem(count++, "");

  for(int i = 0; i < limit->maxEquations(); i++) {
    DynamicEditor *eqEdit = &equationEditor[i];
    if(eqEdit->menuAction != NULL) {
      const QString &name = eqEdit->nameEdit->text().trimmed();
      bodyEdit->ui.equationCombo->insertItem(count, name);
      if ( bodyEdit->equation == eqEdit ) takethis = count;
      count++;
    }
  }
  connect( bodyEdit,SIGNAL(BodyEquationComboChanged(BodyPropertyEditor *,QString)), 
        this, SLOT(equationComboChanged(BodyPropertyEditor *,QString)) );
  bodyEdit->ui.equationCombo->setCurrentIndex(takethis-1);
    
  // Material
  // =========
  bodyEdit->disconnect(SIGNAL(BodyMaterialComboChanged(BodyPropertyEditor *,QString)));
  while(bodyEdit->ui.materialCombo->count() > 0) 
    bodyEdit->ui.materialCombo->removeItem(0);

  count = 1;
  takethis = 1; // -1
  bodyEdit->ui.materialCombo->insertItem(count++, "");

  for(int i = 0; i < limit->maxMaterials(); i++) {
    DynamicEditor *matEdit = &materialEditor[i];
    if(matEdit->menuAction != NULL) {
      const QString &name = matEdit->nameEdit->text().trimmed();
      bodyEdit->ui.materialCombo->insertItem(count, name);
      if ( bodyEdit->material == matEdit ) takethis = count;
      count++;
    }
  }
  connect( bodyEdit,SIGNAL(BodyMaterialComboChanged(BodyPropertyEditor *,QString)), 
        this, SLOT(materialComboChanged(BodyPropertyEditor *,QString)) );
  bodyEdit->ui.materialCombo->setCurrentIndex(takethis-1);
    

  // Bodyforce:
  //===========
  bodyEdit->disconnect(SIGNAL(BodyForceComboChanged(BodyPropertyEditor *,QString)));
  while(bodyEdit->ui.bodyForceCombo->count() > 0) 
    bodyEdit->ui.bodyForceCombo->removeItem(0);

  count = 1;
  takethis = 1; // -1
  bodyEdit->ui.bodyForceCombo->insertItem(count++, "");

  for(int i = 0; i < limit->maxBodyforces(); i++) {
    DynamicEditor *bodyForceEdit = &bodyForceEditor[i];
    if(bodyForceEdit->menuAction != NULL) {
      const QString &name = bodyForceEdit->nameEdit->text().trimmed();
      bodyEdit->ui.bodyForceCombo->insertItem(count, name);
      if ( bodyEdit->force == bodyForceEdit ) takethis = count;
      count++;
    }
  }
  connect( bodyEdit,SIGNAL(BodyForceComboChanged(BodyPropertyEditor *,QString)), 
        this, SLOT(forceComboChanged(BodyPropertyEditor *,QString)) );
  bodyEdit->ui.bodyForceCombo->setCurrentIndex(takethis-1);
    
  // Initial Condition:
  //====================
  bodyEdit->disconnect(SIGNAL(BodyInitialComboChanged(BodyPropertyEditor *,QString)));
  while(bodyEdit->ui.initialConditionCombo->count() > 0) 
    bodyEdit->ui.initialConditionCombo->removeItem(0);

  count = 1;
  takethis = 1; // -1
  bodyEdit->ui.initialConditionCombo->insertItem(count++, "");

  for(int i = 0; i < limit->maxInitialconditions(); i++) {
    DynamicEditor *initialConditionEdit = &initialConditionEditor[i];
    if(initialConditionEdit->menuAction != NULL) {
      const QString &name = initialConditionEdit->nameEdit->text().trimmed();
      bodyEdit->ui.initialConditionCombo->insertItem(count, name);
      if ( bodyEdit->initial == initialConditionEdit ) takethis = count;
      count++;
    }
  }
  connect( bodyEdit,SIGNAL(BodyInitialComboChanged(BodyPropertyEditor *,QString)), 
        this, SLOT(initialComboChanged(BodyPropertyEditor *,QString)) );
  bodyEdit->ui.initialConditionCombo->setCurrentIndex(takethis-1);
}

//-----------------------------------------------------------------------------
void MainWindow::materialComboChanged(BodyPropertyEditor *b, QString text)
{
  b->material = 0;
  b->touched = false;

  for(int i=0; i < limit->maxMaterials(); i++)
  {
    DynamicEditor *mat = &materialEditor[i];
    if ( mat->ID >= 0 ) {
       if ( mat->nameEdit->text().trimmed()==text ) {
         b->material = mat; 
         b->touched = true;
         break;
       }
    }
  }
}

//-----------------------------------------------------------------------------
void MainWindow::initialComboChanged(BodyPropertyEditor *b, QString text)
{
  b->initial = 0;
  b->touched = false;

  for( int i=0; i < limit->maxInitialconditions(); i++ )
  {
    DynamicEditor *ic = &initialConditionEditor[i];
    if ( ic->ID >= 0 ) {
       if ( ic->nameEdit->text().trimmed()==text ) {
         b->initial = ic; 
         b->touched = true;
         break;
      }
    }
  }
}

//-----------------------------------------------------------------------------
void MainWindow::forceComboChanged(BodyPropertyEditor *b, QString text)
{
  b->force = 0;
  b->touched = false;

  for( int i=0; i < limit->maxBodyforces(); i++ )
  {
    DynamicEditor *bf = &bodyForceEditor[i];
    if ( bf->ID >= 0 ) {
       if ( bf->nameEdit->text().trimmed()==text ) {
         b->force = bf; 
         b->touched = true;
         break;
       }
    }
  }
}

//-----------------------------------------------------------------------------
void MainWindow::equationComboChanged(BodyPropertyEditor *b, QString text)
{
  b->equation = 0;
  b->touched = false;

  for( int i=0; i < limit->maxEquations(); i++ )
  {
    DynamicEditor *equ = &equationEditor[i];
    if ( equ->ID >= 0 ) {
       if ( equ->nameEdit->text().trimmed() == text ) {
         b->equation = equ; 
         b->touched = true;
         break;
       }
    }
  }
}


// Edit -> Definitions...
//-----------------------------------------------------------------------------
void MainWindow::editDefinitionsSlot()
{
  if(elmerDefs == NULL)
    return;

  edfEditor->show();
}


//*****************************************************************************
//
//                                Solver MENU
//
//*****************************************************************************


// Solver -> Parallel settings
//-----------------------------------------------------------------------------
void MainWindow::parallelSettingsSlot()
{
  parallel->show();
}


// Solver -> Run solver
//-----------------------------------------------------------------------------
void MainWindow::runsolverSlot()
{
  if(glWidget->mesh == NULL) {
    logMessage("No mesh - unable to start solver");
    return;
  }
  
  if(solver->state() == QProcess::Running) {
    logMessage("Solver is already running - returning");
    return;
  }

  // Parallel solution:
  //====================
  Ui::parallelDialog ui = parallel->ui;
  bool parallelActive = ui.parallelActiveCheckBox->isChecked();
  bool partitioningActive = !ui.skipPartitioningCheckBox->isChecked();
  int nofProcessors = ui.nofProcessorsSpinBox->value();

  if(parallelActive) {

    // Set up log window:
    solverLogWindow->setWindowTitle(tr("Solver log"));
    solverLogWindow->textEdit->clear();
    solverLogWindow->found = false;
    solverLogWindow->show();
    
    if(!partitioningActive) {
      
      // skip splitting:
      meshSplitterFinishedSlot(0);

    } else {

      // split mesh:
      if(meshSplitter->state() == QProcess::Running) {
	logMessage("Mesh partitioner is already running - aborted");
	return;
      }
      
      if (saveDirName.isEmpty()) {
	logMessage("Please save the mesh before running the parallel solver - aborted");
	return;
      }
      
      QString partitioningCommand = ui.divideLineEdit->text().trimmed();
      partitioningCommand.replace(QString("%msh"), saveDirName);
      partitioningCommand.replace(QString("%n"), QString::number(nofProcessors));
 
      logMessage("Executing: " + partitioningCommand);
      
      meshSplitter->start(partitioningCommand);
      
      if(!meshSplitter->waitForStarted()) {
	logMessage("Unable to start ElmerGrid for mesh paritioning - aborted");
	return;
      }
    }
    
    // the rest is done in meshSplitterFinishedSlot:
    return;
  }

  // Scalar solution:
  //==================
  solver->start("ElmerSolver");
  killsolverAct->setEnabled(true);

  if(!solver->waitForStarted()) {
    logMessage("Unable to start solver");
    return;
  }
  
  solverLogWindow->setWindowTitle(tr("Solver log"));
  solverLogWindow->textEdit->clear();
  solverLogWindow->found = false;
  solverLogWindow->show();

  // convergence plot:
  convergenceView->removeData();
  convergenceView->title = "Convergence history";

  logMessage("Solver started");

  runsolverAct->setIcon(QIcon(":/icons/Solver-red.png"));

  updateSysTrayIcon("ElmerSolver started",
		    "Use Run->Kill solver to stop processing");
}


// meshSplitter emits (int) when ready...
//-----------------------------------------------------------------------------
void MainWindow::meshSplitterFinishedSlot(int exitCode)
{
  if(exitCode != 0) {
    solverLogWindow->textEdit->append("MeshSplitter failed - aborting");
    logMessage("MeshSplitter failed - aborting");
    return;
  }

  logMessage("MeshSplitter ready");

  // Prepare for parallel solution:
  Ui::parallelDialog ui = parallel->ui;

  int nofProcessors = ui.nofProcessorsSpinBox->value();

  QString parallelExec = ui.parallelExecLineEdit->text().trimmed();
#ifdef WIN32
  parallelExec = "\"" + parallelExec + "\"";
#endif

  QString parallelArgs = ui.parallelArgsLineEdit->text().trimmed();
  parallelArgs.replace(QString("%n"), QString::number(nofProcessors));

  QString parallelCmd = parallelExec + " " + parallelArgs;

  logMessage("Executing: " + parallelCmd);

  solver->start(parallelCmd);
  killsolverAct->setEnabled(true);

  if(!solver->waitForStarted()) {
    solverLogWindow->textEdit->append("Unable to start parallel solver");
    logMessage("Unable to start parallel solver");
    return;
  }
  
  // Set up convergence plot:
  convergenceView->removeData();
  convergenceView->title = "Convergence history";
  logMessage("Parallel solver started");
  runsolverAct->setIcon(QIcon(":/icons/Solver-red.png"));

  updateSysTrayIcon("ElmerSolver started",
		    "Use Run->Kill solver to stop processing");
}


// meshSplitter emits (void) when there is something to read from stdout:
//-----------------------------------------------------------------------------
void MainWindow::meshSplitterStdoutSlot()
{
  QString qs = meshSplitter->readAllStandardOutput();

  while( qs.at(qs.size()-1).unicode()=='\n' ) qs.chop(1);
  solverLogWindow->textEdit->append(qs);
}

// meshSplitter emits (void) when there is something to read from stderr:
//-----------------------------------------------------------------------------
void MainWindow::meshSplitterStderrSlot()
{
  QString qs = meshSplitter->readAllStandardError();

  while( qs.at(qs.size()-1).unicode()=='\n' ) qs.chop(1);
  solverLogWindow->textEdit->append(qs);
}

// meshUnifier emits (int) when ready...
//-----------------------------------------------------------------------------
void MainWindow::meshUnifierFinishedSlot(int exitCode)
{
  QStringList args;

  if(exitCode != 0) {
    solverLogWindow->textEdit->append("MeshUnifier failed - aborting");
    logMessage("MeshUnifier failed - aborting");
    return;
  }

  logMessage("MeshUnifier ready");

  // Prepare for post processing parallel reults:
  //----------------------------------------------
  QString postName = generalSetup->ui.postFileEdit->text().trimmed();
  QFile file(postName);

  if(!file.exists()) {
    solverLogWindow->textEdit->append("Elmerpost input file does not exist.");
    logMessage("Elmerpost input file does not exist.");
    return;
  }

  file.open(QIODevice::ReadOnly);
  QTextStream header(&file);

  int nn, ne, nt, nf;
  QString type, name;

  header >> nn >> ne >> nf >> nt >> type >> name;
  if ( type == "vector:" )
    name = name + "_abs";

  file.close();

  args << "readfile " + postName + "; "
    "set ColorScaleY -0.85; "
    "set ColorScaleEntries  4;"
    "set ColorScaleDecimals 2;"
    "set ColorScaleColor " + name + ";"
    "set DisplayStyle(ColorScale) 1; "
    "set MeshStyle 1; "
    "set MeshColor " + name + ";"
    "set DisplayStyle(ColorMesh) 1; "
    "translate -y 0.2; "
    "UpdateObject; ";

  post->start("ElmerPost", args);
  killresultsAct->setEnabled(true);
  
  if(!post->waitForStarted()) {
    logMessage("Unable to start post processor");
    return;
  }
  
  resultsAct->setIcon(QIcon(":/icons/Post-red.png"));
  
  logMessage("Post processor started");

  updateSysTrayIcon("Postprocessor started",
		    "Use Run->Kill Postprocessor to stop processing");
}


// meshUnifier emits (void) when there is something to read from stdout:
//-----------------------------------------------------------------------------
void MainWindow::meshUnifierStdoutSlot()
{
  QString qs = meshUnifier->readAllStandardOutput();

  while( qs.at(qs.size()-1).unicode()=='\n' ) qs.chop(1);
  solverLogWindow->textEdit->append(qs);
}

// meshUnifier emits (void) when there is something to read from stderr:
//-----------------------------------------------------------------------------
void MainWindow::meshUnifierStderrSlot()
{
  QString qs = meshUnifier->readAllStandardError();

  while( qs.at(qs.size()-1).unicode()=='\n' ) qs.chop(1);
  solverLogWindow->textEdit->append(qs);
}


// solver process emits (void) when there is something to read from stdout:
//-----------------------------------------------------------------------------
void MainWindow::solverStdoutSlot()
{
  QString qs = solver->readAllStandardOutput();
  static QString qs_save="";

  if ( qs_save != "" ) qs = qs_save + qs;

  int n = qs.lastIndexOf('\n');
  if ( n>=0 && n<qs.size()-1 ) {
    qs_save = qs.mid(n+1);
    qs = qs.mid(0,n);
  } else if ( n<0 ) {
      qs_save=qs;
      return;
  } else qs_save="";

  while( qs.at(qs.size()-1).unicode()=='\n' ) qs.chop(1);
  if ( qs=="" ) return;

  solverLogWindow->textEdit->append(qs);

  if(!showConvergence) {

    // hide convergence plot
    //----------------------
    convergenceView->hide();

  } else {

    // draw convergence plot
    //----------------------
    if(!convergenceView->isVisible())
      convergenceView->show();
    
    QStringList qsl = qs.split("\n");
    for(int i = 0; i < qsl.count(); i++) {
      QString tmp = qsl.at(i).trimmed();

      if(tmp.contains("Time:")) {
	QStringList tmpSplitted = tmp.split(" ");
	int last = tmpSplitted.count() - 1;
	QString timeString = tmpSplitted.at(last);
	double timeDouble = timeString.toDouble();
	convergenceView->title = "Convergence history (time="
	  + QString::number(timeDouble) + ")";
      }   

      if(tmp.contains("ComputeChange")) { // && tmp.contains("NS")) {
	QString copyOfTmp = tmp;

	// check solver name:
	QStringList tmpSplitted = tmp.split(":");
	int last = tmpSplitted.count() - 1;
	QString name = tmpSplitted.at(last).trimmed();

	// parse rest of the line:
	double res1 = 0.0;
	double res2 = 0.0;
        int n = tmp.indexOf("NRM,RELC");
        tmp = tmp.mid(n);
	tmpSplitted = tmp.split("(");

	if(tmpSplitted.count() >= 2) {
	  QString tmp2 = tmpSplitted.at(1).trimmed();
	  QStringList tmp2Splitted = tmp2.split(" ");
	  QString qs1 = tmp2Splitted.at(0).trimmed();
	  res1 = qs1.toDouble();
	  int pos = 1;
	  while(tmp2Splitted.at(pos).trimmed() == "") {
	    pos++;
	    if(pos > tmp2Splitted.count())
	      break;
	  }
	  QString qs2 = tmp2Splitted.at(pos).trimmed();
	  res2 = max( qs2.toDouble(), 1.0e-16 );
	}

	// res1 = norm, res2 = relative change
	if(copyOfTmp.contains("NS"))	
	  convergenceView->appendData(res2, "NS/" + name);

	if(copyOfTmp.contains("SS"))
	  convergenceView->appendData(res2, "SS/" + name);
      }
    }
  }
}


// solver process emits (void) when there is something to read from stderr:
//-----------------------------------------------------------------------------
void MainWindow::solverStderrSlot()
{
  QString qs = solver->readAllStandardError();

  while( qs.at(qs.size()-1).unicode()=='\n' ) qs.chop(1);
  solverLogWindow->textEdit->append(qs);
}



// solver process emits (int) when ready...
//-----------------------------------------------------------------------------
void MainWindow::solverFinishedSlot(int)
{
  logMessage("Solver ready");
  runsolverAct->setIcon(QIcon(":/icons/Solver.png"));
  updateSysTrayIcon("ElmerSolver has finished",
		    "Use Run->Postprocessor to view results");
  killsolverAct->setEnabled(false);
}


// solver process emits (QProcess::ProcessError) when error occurs...
//-----------------------------------------------------------------------------
void MainWindow::solverErrorSlot(QProcess::ProcessError error)
{
  logMessage("Solver emitted error signal: " + QString::number(error));
  solver->kill();
  runsolverAct->setIcon(QIcon(":/icons/Solver.png"));
}

// solver process emits (QProcess::ProcessState) when state changed...
//-----------------------------------------------------------------------------
void MainWindow::solverStateChangedSlot(QProcess::ProcessState state)
{
  logMessage("Solver emitted signal: QProcess::ProcessState: " + QString::number(state));
  // solver->kill();
  // runsolverAct->setIcon(QIcon(":/icons/Solver.png"));
}


// Solver -> Kill solver
//-----------------------------------------------------------------------------
void MainWindow::killsolverSlot()
{
  solver->kill();

  logMessage("Solver killed");
  runsolverAct->setIcon(QIcon(":/icons/Solver.png"));
}

// Solver -> Show convergence
//-----------------------------------------------------------------------------
void MainWindow::showConvergenceSlot()
{
  solver->kill();

  showConvergence = !showConvergence;
  synchronizeMenuToState();
}



// Solver -> Run post process
//-----------------------------------------------------------------------------
void MainWindow::resultsSlot()
{
  QStringList args;
  
  if(post->state() == QProcess::Running) {
    logMessage("Post processor is already running");
    return;
  }

  // Parallel solution:
  //====================
  Ui::parallelDialog ui = parallel->ui;
  bool parallelActive = ui.parallelActiveCheckBox->isChecked();

  if(parallelActive) {
    
    // unify mesh:
    if(meshUnifier->state() == QProcess::Running) {
      logMessage("Mesh unifier is already running - aborted");
      return;
    }
    
    if(saveDirName.isEmpty()) {
      logMessage("saveDirName is empty - unable to locate result files");
      return;
    }

    // Set up log window:
    solverLogWindow->setWindowTitle(tr("ElmerGrid log"));
    solverLogWindow->textEdit->clear();
    solverLogWindow->found = false;
    solverLogWindow->show();


    QString postName = generalSetup->ui.postFileEdit->text().trimmed();
    QStringList postNameSplitted = postName.split(".");
    int nofProcessors = ui.nofProcessorsSpinBox->value();

    QString unifyingCommand = ui.mergeLineEdit->text().trimmed();
    unifyingCommand.replace(QString("%ep"), postNameSplitted.at(0).trimmed());
    unifyingCommand.replace(QString("%n"), QString::number(nofProcessors));
    
    logMessage("Executing: " + unifyingCommand);
    
    meshUnifier->start(unifyingCommand);
    
    if(!meshUnifier->waitForStarted()) {
      solverLogWindow->textEdit->append("Unable to start ElmerGrid for mesh unification - aborted");
      logMessage("Unable to start ElmerGrid for mesh unification - aborted");
      return;
    }
    
    // The rest is done in meshUnifierFinishedSlot:
    return;
  }
   
  // Scalar solution:
  //==================
  QString postName = generalSetup->ui.postFileEdit->text().trimmed();
  QFile file(postName);
  if(!file.exists()) {
    logMessage("Elmerpost input file does not exist.");
    return;
  }

  file.open(QIODevice::ReadOnly);
  QTextStream header(&file);

  int nn, ne, nt, nf;
  QString type, name;

  header >> nn >> ne >> nf >> nt >> type >> name;
  if ( type == "vector:" )
    name = name + "_abs";

  file.close();

  args << "readfile " + postName + "; "
    "set ColorScaleY -0.85; "
    "set ColorScaleEntries  4;"
    "set ColorScaleDecimals 2;"
    "set ColorScaleColor " + name + ";"
    "set DisplayStyle(ColorScale) 1; "
    "set MeshStyle 1; "
    "set MeshColor " + name + ";"
    "set DisplayStyle(ColorMesh) 1; "
    "translate -y 0.2; "
    "UpdateObject; ";

  post->start("ElmerPost", args);
  killresultsAct->setEnabled(true);
  
  if(!post->waitForStarted()) {
    logMessage("Unable to start post processor");
    return;
  }
  
  resultsAct->setIcon(QIcon(":/icons/Post-red.png"));
  
  logMessage("Post processor started");

  updateSysTrayIcon("Postprocessor started",
		    "Use Run->Kill Postprocessor to stop processing");
}


// Signal (int) emitted by postProcess when finished:
//-----------------------------------------------------------------------------
void MainWindow::postProcessFinishedSlot(int)
{
  logMessage("Post processor finished");
  resultsAct->setIcon(QIcon(":/icons/Post.png"));
  updateSysTrayIcon("Postprocessor has finished",
		    "Use Run->Run Postprocessor to restart");
  killresultsAct->setEnabled(false);
}


// Solver -> Kill post process
//-----------------------------------------------------------------------------
void MainWindow::killresultsSlot()
{
  post->kill();

  logMessage("Post process killed");
  resultsAct->setIcon(QIcon(":/icons/Post.png"));
}

// Solver -> Compile...
//-----------------------------------------------------------------------------
void MainWindow::compileSolverSlot()
{
  QString fileName = QFileDialog::getOpenFileName(this,
       tr("Open source file"), "", tr("F90 files (*.f90)"));

  if (!fileName.isEmpty()) {
    QFileInfo fi(fileName);
    QString absolutePath = fi.absolutePath();
    QDir::setCurrent(absolutePath);
  } else {
    logMessage("Unable to open file: file name is empty");
    return;
  }

  if(compiler->state() == QProcess::Running) {
    logMessage("Compiler is currently running");
    return;
  }

  QStringList args;
  args << fileName;

#ifdef WIN32
  compiler->start("elmerf90.bat", args);
#else
  logMessage("Run->compiler is currently not implemented on this platform");
  return;
#endif
  
  if(!compiler->waitForStarted()) {
    logMessage("Unable to start compiler");
    return;
  }

  solverLogWindow->setWindowTitle(tr("Compiler log"));
  solverLogWindow->textEdit->clear();
  solverLogWindow->found = false;
  solverLogWindow->show();
  solverLogWindow->statusBar()->showMessage("Compiling...");

  logMessage("Compiling...");
}


// compiler process emits (void) when there is something to read from stdout:
//-----------------------------------------------------------------------------
void MainWindow::compilerStdoutSlot()
{
  QString qs = compiler->readAllStandardOutput();
  while( qs.at(qs.size()-1).unicode()=='\n' ) qs.chop(1);
  solverLogWindow->textEdit->append(qs);
}


// compiler process emits (void) when there is something to read from stderr:
//-----------------------------------------------------------------------------
void MainWindow::compilerStderrSlot()
{
  QString qs = compiler->readAllStandardError();
  while( qs.at(qs.size()-1).unicode()=='\n' ) qs.chop(1);
  solverLogWindow->textEdit->append(qs);
}

// Signal (int) emitted by compiler when finished:
//-----------------------------------------------------------------------------
void MainWindow::compilerFinishedSlot(int)
{
  logMessage("Ready");
  solverLogWindow->statusBar()->showMessage("Ready");
  solverLogWindow->textEdit->append("Ready");
}



//*****************************************************************************
//
//                                Help MENU
//
//*****************************************************************************


// About dialog...
//-----------------------------------------------------------------------------
void MainWindow::showaboutSlot()
{
  QMessageBox::about(this, tr("Information about ElmerGUI"),
		     tr("ElmerGUI is a preprocessor for three dimensional "
			"modeling with Elmer finite element software. "
			"The program uses elmergrid, and optionally "
			"tetlib or nglib, as finite element mesh generators:\n\n"
			"http://www.csc.fi/elmer/\n"
			"http://tetgen.berlios.de/\n"
			"http://www.hpfem.jku.at/netgen/\n\n"
			"ElmerGUI uses the Qt4 Cross-Platform "
			"Application Framework by Trolltech:\n\n"
			"http://trolltech.com/products/qt\n\n"
#ifdef OCC62
			"This version of ElmerGUI has been compiled with the "
			"OpenCascade solids modeling library using the "
			"QtOpenCascade integration framework:\n\n"
			"http://www.opencascade.org/\n"
			"http://sourceforge.net/projects/qtocc/\n\n"
#endif

#ifdef MPICH2
			"The parallel solver of this package has been linked "
			"against the MPICH2 library v. 1.0.7 from Argonne "
			"national laboratory. In order to use the parallel "
			"solver, the MPICH2 runtime environment should be "
			"installed and configured on your system. For more "
			"details, see:\n\n"
			"http://www.mcs.anl.gov/research/projects/mpich2/\n\n"
#endif
			"The GPL-licensed source code of ElmerGUI is available "
			"from the SVN repository at\n\n"
			"http://sourceforge.net/projects/elmerfem\n\n"
			"Written by Mikko Lyly, Juha Ruokolainen, and "
			"Peter R�back, 2008"));
}



//*****************************************************************************
//
//                           Auxiliary non-menu items
//
//*****************************************************************************


// Log message...
//-----------------------------------------------------------------------------
void MainWindow::logMessage(QString message)
{
  cout << string(message.toAscii()) << endl;
  statusBar()->showMessage(message, 0);
  cout.flush();
}



// Synchronize menu to GL glwidget state variables:
//-----------------------------------------------------------------------------
void MainWindow::synchronizeMenuToState()
{
  // glwidget state variables:
  if(glWidget->stateDrawSurfaceMesh)
    hidesurfacemeshAct->setIcon(iconChecked);
  else
    hidesurfacemeshAct->setIcon(iconEmpty);
  
  if(glWidget->stateDrawVolumeMesh)
    hidevolumemeshAct->setIcon(iconChecked);
  else
    hidevolumemeshAct->setIcon(iconEmpty);
  
  if(glWidget->stateDrawSharpEdges)
    hidesharpedgesAct->setIcon(iconChecked);
  else
    hidesharpedgesAct->setIcon(iconEmpty);
  
  if(glWidget->stateFlatShade) {
    flatShadeAct->setIcon(iconChecked);
    smoothShadeAct->setIcon(iconEmpty);
  } else {
    flatShadeAct->setIcon(iconEmpty);
    smoothShadeAct->setIcon(iconChecked);
  }

  if(glWidget->stateDrawSurfaceNumbers) 
    showSurfaceNumbersAct->setIcon(iconChecked);
  else 
    showSurfaceNumbersAct->setIcon(iconEmpty);   

  if(glWidget->stateDrawEdgeNumbers) 
    showEdgeNumbersAct->setIcon(iconChecked);
  else 
    showEdgeNumbersAct->setIcon(iconEmpty);   

  if(glWidget->stateDrawNodeNumbers) 
    showNodeNumbersAct->setIcon(iconChecked);
  else 
    showNodeNumbersAct->setIcon(iconEmpty);   

  if(glWidget->stateDrawBoundaryIndex)
    showBoundaryIndexAct->setIcon(iconChecked);
  else 
    showBoundaryIndexAct->setIcon(iconEmpty);   

  if(glWidget->stateDrawBodyIndex)
    showBodyIndexAct->setIcon(iconChecked);
  else 
    showBodyIndexAct->setIcon(iconEmpty);   

  if(glWidget->stateDrawCoordinates) 
    viewCoordinatesAct->setIcon(iconChecked);
  else 
    viewCoordinatesAct->setIcon(iconEmpty);

  if(bodyEditActive)
    bodyEditAct->setIcon(iconChecked);
  else
    bodyEditAct->setIcon(iconEmpty);

  if(bcEditActive)
    bcEditAct->setIcon(iconChecked);
  else
    bcEditAct->setIcon(iconEmpty);

  if(showConvergence)
    showConvergenceAct->setIcon(iconChecked);
  else
    showConvergenceAct->setIcon(iconEmpty);

  if(glWidget->stateBcColors)
    showBoundaryColorAct->setIcon(iconChecked);
  else
    showBoundaryColorAct->setIcon(iconEmpty);

  if(glWidget->stateBodyColors)
    showBodyColorAct->setIcon(iconChecked);
  else
    showBodyColorAct->setIcon(iconEmpty);
}


// Load definitions...
//-----------------------------------------------------------------------------
void MainWindow::loadDefinitions()
{

  // Determine edf-file location and name:
  //--------------------------------------
  char *elmerGuiHome = NULL;

#ifdef __APPLE__
  QString generalDefs = this->homePath +  "/edf/edf.xml";          
#else
  QString generalDefs = "edf/edf.xml";
  elmerGuiHome = getenv("ELMERGUI_HOME");
  if(elmerGuiHome != NULL) 
    generalDefs = QString(elmerGuiHome) + "/edf/edf.xml";
#endif

  // Load general definitions file:
  //--------------------------------
  cout << "Load " << string(generalDefs.toAscii()) << "... ";
  cout.flush();
  updateSplash("Loading general definitions...");

  QFile file(generalDefs);
  
  QString errStr;
  int errRow;
  int errCol;
  
  if(!file.exists()) {

    elmerDefs = NULL;
    QMessageBox::information(window(), tr("Edf loader: ") + generalDefs,
			     tr("Definitions file does not exist"));
    return;

  } else {  

    if(!elmerDefs->setContent(&file, true, &errStr, &errRow, &errCol)) {
      QMessageBox::information(window(), tr("Edf loader: ") + generalDefs,
			       tr("Parse error at line %1, col %2:\n%3")
			       .arg(errRow).arg(errCol).arg(errStr));
      file.close();
      return;

    } else {

      if(elmerDefs->documentElement().tagName() != "edf") {
	QMessageBox::information(window(), tr("Edf loader: ") + generalDefs,
				 tr("This is not an edf file"));
	delete elmerDefs;
	file.close();	
	return;

      }
    }
  }

  edfEditor->setupEditor(*elmerDefs);
  file.close();

  cout << "done" << endl;
  cout.flush();

  // load additional definitions:
  //-----------------------------
#ifdef __APPLE__
  QDirIterator iterator( homePath+"/edf", QDirIterator::Subdirectories);
#else
  QString additionalEdfs = "edf";
  if(elmerGuiHome != NULL) 
    additionalEdfs = QString(elmerGuiHome) + "/edf";
  QDirIterator iterator( additionalEdfs, QDirIterator::Subdirectories);
#endif

  while (iterator.hasNext()) {
    QString fileName = iterator.next();

    QFileInfo fileInfo(fileName);
    QString fileSuffix = fileInfo.suffix();

    // The name "egini" is reserved for the ini file:
    if(fileInfo.completeBaseName() == "egini")
      continue;

    if((fileSuffix == "xml") && (fileName != generalDefs)) {

      cout << "Load " << string(fileName.toAscii()) << "... ";
      cout.flush();

      updateSplash("Loading " + fileName + "...");

      file.setFileName(fileName);

      QDomDocument tmpDoc;
      tmpDoc.clear();

      if(!tmpDoc.setContent(&file, true, &errStr, &errRow, &errCol)) {
	QMessageBox::information(window(), tr("Edf loader: ") + fileName,
				 tr("Parse error at line %1, col %2:\n%3")
				 .arg(errRow).arg(errCol).arg(errStr));
	file.close();
	return;

      } else {

	if(tmpDoc.documentElement().tagName() != "edf") {
	  QMessageBox::information(window(), tr("Edf loader: ") + fileName,
				   tr("This is not an edf file"));
	  file.close();
	  return;      
	}
      }
      
      // add new elements to the document
      QDomElement root = elmerDefs->documentElement();
      QDomElement tmpRoot = tmpDoc.documentElement();
      
      QDomElement element = tmpRoot.firstChildElement();

      while(!element.isNull()) {
	root.appendChild(element);
	element = tmpRoot.firstChildElement();
      }
      
      edfEditor->setupEditor(*elmerDefs);

      file.close();

      cout << "done" << endl;
      cout.flush();
    }
  }

  // Load qss:
  //-----------
  QString qssFileName = "elmergui.qss";

  #ifdef __APPLE__
	qssFileName = homePath + "/elmergui.qss";
  #else
  	if(elmerGuiHome != NULL) 
    	qssFileName = QString(elmerGuiHome) + "/elmergui.qss";
  #endif

  QFile qssFile(qssFileName);
  
  if(qssFile.exists()) {
    cout << "Loading QSS style sheet... ";
    qssFile.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(qssFile.readAll());
    qssFile.close();
    qApp->setStyleSheet(styleSheet);    
    cout << "done" << endl;
  }
}

// Setup splash...
//-----------------------------------------------------------------------------
void MainWindow::setupSplash()
{
  if(egIni->isSet("splashscreen")) {
    pixmap.load(":/images/splash.png");
    splash.setPixmap(pixmap);
    splash.show();
    qApp->processEvents();
  }
}


// Update splash...
//-----------------------------------------------------------------------------
void MainWindow::updateSplash(QString text)
{
  if(!egIni->isSet("splashscreen"))
    return;

  if(splash.isVisible()) {
    splash.showMessage(text, Qt::AlignBottom);
    qApp->processEvents();
  }
}

// Finalize splash...
//-----------------------------------------------------------------------------
void MainWindow::finalizeSplash()
{
  if(!egIni->isSet("splashscreen"))
    return;

  if(splash.isVisible())
    splash.finish(this);
}

// Setup system tray icon...
//-----------------------------------------------------------------------------
void MainWindow::setupSysTrayIcon()
{
  sysTrayIcon = NULL;

  if(!egIni->isSet("systrayicon"))
    return;

  if(QSystemTrayIcon::isSystemTrayAvailable()) {
    sysTrayIcon = new QSystemTrayIcon(this);
    sysTrayIcon->setIcon(QIcon(":/icons/Mesh3D.png"));
    sysTrayIcon->setVisible(true);
    sysTrayIcon->setContextMenu(sysTrayMenu);
  }
}

// Update system tray icon...
//-----------------------------------------------------------------------------
void MainWindow::updateSysTrayIcon(QString label, QString msg)
{
  int duration = 3000;

  if(!sysTrayIcon)
    return;

  if(!egIni->isSet("systraymessages"))
    return;
  
  if(egIni->isPresent("systraymsgduration"))
    duration = egIni->value("systraymsgduration").toInt();

  if(sysTrayIcon->supportsMessages())
    sysTrayIcon->showMessage(label, msg, QSystemTrayIcon::Information, duration);

}

// Finalize system tray icon...
//-----------------------------------------------------------------------------
void MainWindow::finalizeSysTrayIcon()
{
}
