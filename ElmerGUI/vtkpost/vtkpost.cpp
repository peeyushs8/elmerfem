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
 *  ElmerGUI vtkpost                                                         *
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
#include <iostream>

#include "epmesh.h"
#include "vtkpost.h"
#include "surface.h"
#include "isocontour.h"
#include "isosurface.h"
#include "colorbar.h"
#include "preferences.h"
#include "vector.h"
#include "streamline.h"
#include "timestep.h"

#include <QVTKWidget.h>

#include <vtkLookupTable.h>
#include <vtkActor.h>
#include <vtkActor2D.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCamera.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkLookupTable.h>
#include <vtkProperty.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkScalarBarActor.h>
#include <vtkTextMapper.h>
#include <vtkScalarBarActor.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkTetra.h>
#include <vtkHexahedron.h>
#include <vtkTriangle.h>
#include <vtkQuad.h>
#include <vtkLine.h>
#include <vtkUnstructuredGrid.h>
#include <vtkDataSetMapper.h>
#include <vtkContourFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkOutlineFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkExtractEdges.h>
#include <vtkFeatureEdges.h>
#include <vtkGeometryFilter.h>
#include <vtkGlyph3D.h>
#include <vtkArrowSource.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkCellDerivatives.h>
#include <vtkCellDataToPointData.h>
#include <vtkSphereSource.h>
#include <vtkCylinderSource.h>
#include <vtkStreamLine.h>
#include <vtkRungeKutta4.h>
#include <vtkPointSource.h>
#include <vtkLineSource.h>
#include <vtkRibbonFilter.h>
#include <vtkPlane.h>
#include <vtkClipPolyData.h>
#include <vtkCellPicker.h>
#include <vtkCallbackCommand.h>
#include <vtkAbstractPicker.h>
#include <vtkObject.h>
#include <vtkCommand.h>

#ifdef MATC
#include "matc.h"
#include "mc.h"
extern "C" VARIABLE *com_curl(VARIABLE *);
extern "C" VARIABLE *com_div(VARIABLE *);
extern "C" VARIABLE *com_grad(VARIABLE *);
extern "C" VARIABLE *var_new(char *,int,int,int);
extern "C" VARIABLE *var_check(char *);
extern "C" VARIABLE *var_temp_new(int,int,int);
extern "C" void var_delete(char *);
extern "C" char *mtc_domath(const char *);
extern "C" void mtc_init(FILE *,FILE *,FILE *);
extern "C" void com_init(char *,int,int,VARIABLE *(*)(VARIABLE *),int,int,char*);
#endif

using namespace std;

// Pick event handler (place cursor on cell && press 'p' to pick):
//-----------------------------------------------------------------
static void pickEventHandler(vtkObject *caller, unsigned long eid, 
			     void* clientdata, void *calldata)
{
  VtkPost* vtkPost = reinterpret_cast<VtkPost*>(clientdata);
  QVTKWidget* qvtkWidget = vtkPost->GetQVTKWidget();
  vtkAbstractPicker* picker = qvtkWidget->GetInteractor()->GetPicker();
  vtkCellPicker* cellPicker = vtkCellPicker::SafeDownCast(picker);

  double pickPosition[3];
  cellPicker->GetPickPosition(pickPosition);
  vtkPost->SetCurrentPickPosition(pickPosition);
}

// Class VtkPost:
//-----------------------------------------------------------------
VtkPost::VtkPost(QWidget *parent)
  : QMainWindow(parent)
{
  // Initialize:
  //------------
  setWindowIcon(QIcon(":/icons/Mesh3D.png"));
  setWindowTitle("ElmerGUI postprocessor");

  createActions();
  createMenus();
  createToolbars();
  createStatusBar();

  // VTK:
  //-----
  volumeGrid = vtkUnstructuredGrid::New();
  surfaceGrid = vtkUnstructuredGrid::New();
  lineGrid = vtkUnstructuredGrid::New();
  isoContourActor = vtkActor::New();
  isoSurfaceActor = vtkActor::New();
  surfaceActor = vtkActor::New();
  meshEdgeActor = vtkActor::New();
  meshPointActor = vtkActor::New();
  colorBarActor = vtkScalarBarActor::New();
  featureEdgeActor = vtkActor::New();
  vectorActor = vtkActor::New();
  streamLineActor = vtkActor::New();

  // Default color map (from blue to red):
  //--------------------------------------
  currentLut = vtkLookupTable::New();
  currentLut->SetHueRange(0.6667, 0);
  currentLut->SetNumberOfColors(256);
  currentLut->Build();

  clipPlane = vtkPlane::New();

  // User interfaces:
  //-----------------
  surface = new Surface(this);
  connect(surface, SIGNAL(drawSurfaceSignal()), this, SLOT(drawSurfaceSlot()));
  connect(surface, SIGNAL(hideSurfaceSignal()), this, SLOT(hideSurfaceSlot()));

  vector = new Vector(this);
  connect(vector, SIGNAL(drawVectorSignal()), this, SLOT(drawVectorSlot()));
  connect(vector, SIGNAL(hideVectorSignal()), this, SLOT(hideVectorSlot()));
  
  isoContour = new IsoContour(this);
  connect(isoContour, SIGNAL(drawIsoContourSignal()), this, SLOT(drawIsoContourSlot()));
  connect(isoContour, SIGNAL(hideIsoContourSignal()), this, SLOT(hideIsoContourSlot()));

  isoSurface = new IsoSurface(this);
  connect(isoSurface, SIGNAL(drawIsoSurfaceSignal()), this, SLOT(drawIsoSurfaceSlot()));
  connect(isoSurface, SIGNAL(hideIsoSurfaceSignal()), this, SLOT(hideIsoSurfaceSlot()));

  colorBar = new ColorBar(this);
  connect(colorBar, SIGNAL(drawColorBarSignal()), this, SLOT(drawColorBarSlot()));
  connect(colorBar, SIGNAL(hideColorBarSignal()), this, SLOT(hideColorBarSlot()));

  streamLine = new StreamLine(this);
  connect(streamLine, SIGNAL(drawStreamLineSignal()), this, SLOT(drawStreamLineSlot()));
  connect(streamLine, SIGNAL(hideStreamLineSignal()), this, SLOT(hideStreamLineSlot()));

  preferences = new Preferences(this);
  connect(preferences, SIGNAL(redrawSignal()), this, SLOT(redrawSlot()));

  timeStep = new TimeStep(this);
  connect(timeStep, SIGNAL(timeStepChangedSignal()), this, SLOT(timeStepChangedSlot()));

#ifdef MATC
  matc = new Matc(this);
  connect(matc->ui.mcEdit, SIGNAL(returnPressed()), this, SLOT(domatcSlot()));
  connect(matc->ui.mcHistory, SIGNAL(selectionChanged()), this, SLOT(matcCutPasteSlot()));
  mtc_init( NULL, stdout, stderr ); 
  QString elmerGuiHome = getenv("ELMERGUI_HOME");
  QString mcIniLoad = "source(\"" + elmerGuiHome.replace("\\", "/") + "/edf/mc.ini\")";
  mtc_domath( mcIniLoad.toAscii().data() );
  com_init( (char *)"grad", FALSE, FALSE, com_grad, 1, 1,
            (char *)"r = grad(f): compute gradient of a scalar variable f.\n") ;
  com_init( (char *)"div", FALSE, FALSE, com_div, 1, 1,
            (char *)"r = div(f): compute divergence of a vector variable f.\n") ;
  com_init( (char *)"curl", FALSE, FALSE, com_curl, 1, 1,
            (char *)"r = curl(f): compute curl of a vector variable f.\n") ;
#endif

  // Ep-data:
  //----------
  epMesh = new EpMesh;
  postFileName = "";
  postFileRead = false;
  scalarFields = 0;
  scalarField = new ScalarField[MAX_SCALARS];

  // Central widget:
  //----------------
  qvtkWidget = new QVTKWidget(this);
  setCentralWidget(qvtkWidget);

  // VTK interaction:
  //------------------
  renderer = vtkRenderer::New();
  renderer->SetBackground(1, 1, 1);
  qvtkWidget->GetRenderWindow()->AddRenderer(renderer);
  renderer->GetRenderWindow()->Render();

  // Create a cell picker and set the callback & observer:
  //------------------------------------------------------
  vtkCellPicker *cellPicker = vtkCellPicker::New();
  qvtkWidget->GetInteractor()->SetPicker(cellPicker);
  cellPicker->Delete();

  vtkCallbackCommand *cbc = vtkCallbackCommand::New();
  cbc->SetClientData(this);
  cbc->SetCallback(pickEventHandler);
  vtkAbstractPicker *picker = qvtkWidget->GetInteractor()->GetPicker();
  picker->AddObserver(vtkCommand::EndPickEvent, cbc);
  cbc->Delete();
}

VtkPost::~VtkPost()
{
}

QVTKWidget* VtkPost::GetQVTKWidget()
{
  return qvtkWidget;
}

void VtkPost::SetCurrentPickPosition(double *p)
{
  currentPickPosition[0] = p[0];
  currentPickPosition[1] = p[1];
  currentPickPosition[2] = p[2];
}

QSize VtkPost::minimumSizeHint() const
{
  return QSize(64, 64);
}

QSize VtkPost::sizeHint() const
{
  return QSize(640, 480);
}

void VtkPost::createActions()
{
  // File menu:
  //-----------
  exitAct = new QAction(QIcon(":/icons/application-exit.png"), tr("&Quit"), this);
  exitAct->setShortcut(tr("Ctrl+Q"));
  exitAct->setStatusTip("Quit VTK widget");
  connect(exitAct, SIGNAL(triggered()), this, SLOT(exitSlot()));

  savePictureAct = new QAction(QIcon(""), tr("Save picture as..."), this);
  savePictureAct->setStatusTip("Save picture in file");
  connect(savePictureAct, SIGNAL(triggered()), this, SLOT(savePictureSlot()));

  // View menu:
  //------------
  drawMeshPointAct = new QAction(QIcon(""), tr("Mesh points"), this);
  drawMeshPointAct->setStatusTip("Draw mesh points");
  drawMeshPointAct->setCheckable(true);
  drawMeshPointAct->setChecked(false);
  connect(drawMeshPointAct, SIGNAL(triggered()), this, SLOT(drawMeshPointSlot()));
  connect(drawMeshPointAct, SIGNAL(toggled(bool)), this, SLOT(maybeRedrawSlot(bool)));

  drawMeshEdgeAct = new QAction(QIcon(""), tr("Mesh edges"), this);
  drawMeshEdgeAct->setStatusTip("Draw mesh edges");
  drawMeshEdgeAct->setCheckable(true);
  drawMeshEdgeAct->setChecked(false);
  connect(drawMeshEdgeAct, SIGNAL(triggered()), this, SLOT(drawMeshEdgeSlot()));
  connect(drawMeshEdgeAct, SIGNAL(toggled(bool)), this, SLOT(maybeRedrawSlot(bool)));

  drawFeatureEdgesAct = new QAction(QIcon(""), tr("Feature edges"), this);
  drawFeatureEdgesAct->setStatusTip("Draw feature edges");
  drawFeatureEdgesAct->setCheckable(true);
  drawFeatureEdgesAct->setChecked(true);
  connect(drawFeatureEdgesAct, SIGNAL(triggered()), this, SLOT(drawFeatureEdgesSlot()));
  connect(drawFeatureEdgesAct, SIGNAL(toggled(bool)), this, SLOT(maybeRedrawSlot(bool)));

  drawColorBarAct = new QAction(QIcon(""), tr("Colorbar"), this);
  drawColorBarAct->setStatusTip("Draw color bar");
  drawColorBarAct->setCheckable(true);
  drawColorBarAct->setChecked(false);
  connect(drawColorBarAct, SIGNAL(triggered()), this, SLOT(showColorBarDialogSlot()));
  connect(drawColorBarAct, SIGNAL(toggled(bool)), this, SLOT(maybeRedrawSlot(bool)));

  drawSurfaceAct = new QAction(QIcon(""), tr("Surfaces"), this);
  drawSurfaceAct->setStatusTip("Draw scalar fields on surfaces");
  drawSurfaceAct->setCheckable(true);
  drawSurfaceAct->setChecked(false);
  connect(drawSurfaceAct, SIGNAL(triggered()), this, SLOT(showSurfaceDialogSlot()));
  connect(drawSurfaceAct, SIGNAL(toggled(bool)), this, SLOT(maybeRedrawSlot(bool)));

  drawVectorAct = new QAction(QIcon(""), tr("Vectors"), this);
  drawVectorAct->setStatusTip("Visualize vector fields by arrows");
  drawVectorAct->setCheckable(true);
  drawVectorAct->setChecked(false);
  connect(drawVectorAct, SIGNAL(triggered()), this, SLOT(showVectorDialogSlot()));
  connect(drawVectorAct, SIGNAL(toggled(bool)), this, SLOT(maybeRedrawSlot(bool)));

  drawIsoContourAct = new QAction(QIcon(""), tr("Isocontours"), this);
  drawIsoContourAct->setStatusTip("Draw isocontours (2d)");
  drawIsoContourAct->setCheckable(true);
  drawIsoContourAct->setChecked(false);
  connect(drawIsoContourAct, SIGNAL(triggered()), this, SLOT(showIsoContourDialogSlot()));
  connect(drawIsoContourAct, SIGNAL(toggled(bool)), this, SLOT(maybeRedrawSlot(bool)));

  drawIsoSurfaceAct = new QAction(QIcon(""), tr("Isosurfaces"), this);
  drawIsoSurfaceAct->setStatusTip("Draw isosurfaces (3d)");
  drawIsoSurfaceAct->setCheckable(true);
  drawIsoSurfaceAct->setChecked(false);
  connect(drawIsoSurfaceAct, SIGNAL(triggered()), this, SLOT(showIsoSurfaceDialogSlot()));
  connect(drawIsoSurfaceAct, SIGNAL(toggled(bool)), this, SLOT(maybeRedrawSlot(bool)));

  drawStreamLineAct = new QAction(QIcon(""), tr("Streamlines"), this);
  drawStreamLineAct->setStatusTip("Draw stream lines");
  drawStreamLineAct->setCheckable(true);
  drawStreamLineAct->setChecked(false);
  connect(drawStreamLineAct, SIGNAL(triggered()), this, SLOT(showStreamLineDialogSlot()));
  connect(drawStreamLineAct, SIGNAL(toggled(bool)), this, SLOT(maybeRedrawSlot(bool)));

  redrawAct = new QAction(QIcon(""), tr("Redraw"), this);
  redrawAct->setShortcut(tr("Ctrl+R"));
  redrawAct->setStatusTip("Redraw");
  connect(redrawAct, SIGNAL(triggered()), this, SLOT(redrawSlot()));

  fitToWindowAct = new QAction(QIcon(""), tr("Fit to window"), this);
  fitToWindowAct->setStatusTip("Fit model to window");
  connect(fitToWindowAct, SIGNAL(triggered()), this, SLOT(fitToWindowSlot()));

  preferencesAct = new QAction(QIcon(""), tr("Preferences"), this);
  preferencesAct->setStatusTip("Show preferences");
  connect(preferencesAct, SIGNAL(triggered()), this, SLOT(showPreferencesDialogSlot()));

  // Edit menu:
  //------------
#ifdef MATC
  matcAct = new QAction(QIcon(""), tr("Matc..."), this);
  matcAct->setStatusTip("Matc window");
  connect(matcAct, SIGNAL(triggered()), this, SLOT(matcOpenSlot()));
#endif

  regenerateGridsAct = new QAction(QIcon(""), tr("Regenerate all..."), this);
  regenerateGridsAct->setStatusTip("Regerate all meshes");
  connect(regenerateGridsAct, SIGNAL(triggered()), this, SLOT(regenerateGridsSlot()));

  timeStepAct = new QAction(QIcon(""), tr("Time step control"), this);
  timeStepAct->setStatusTip("Time step control");
  connect(timeStepAct, SIGNAL(triggered()), this, SLOT(showTimeStepDialogSlot()));

}

void VtkPost::createMenus()
{
  // File menu:
  //-----------
  fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(savePictureAct);
  fileMenu->addSeparator();
  fileMenu->addAction(exitAct);

  // Edit menu:
  //-----------
  editMenu = menuBar()->addMenu(tr("&Edit"));
  editGroupsMenu = new QMenu(tr("Groups"));
  editMenu->addMenu(editGroupsMenu);
  editMenu->addSeparator();
  editMenu->addAction(timeStepAct);
#ifdef MATC
  editMenu->addSeparator();
  editMenu->addAction( matcAct );
#endif

  // View menu:
  //-----------
  viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(drawMeshPointAct);
  viewMenu->addAction(drawMeshEdgeAct);
  viewMenu->addAction(drawFeatureEdgesAct);
  viewMenu->addSeparator();
  viewMenu->addAction(drawSurfaceAct);
  viewMenu->addSeparator();
  viewMenu->addAction(drawIsoContourAct);
  viewMenu->addAction(drawIsoSurfaceAct);
  viewMenu->addSeparator();
  viewMenu->addAction(drawVectorAct);
  viewMenu->addSeparator();
  viewMenu->addAction(drawColorBarAct);
  viewMenu->addSeparator();
  viewMenu->addAction(drawStreamLineAct);
  viewMenu->addSeparator();
  viewMenu->addAction(preferencesAct);
  viewMenu->addSeparator();
  viewMenu->addAction(fitToWindowAct);
  viewMenu->addAction(redrawAct);
}

void VtkPost::createToolbars()
{
  viewToolBar = addToolBar(tr("View"));
  viewToolBar->addAction(drawSurfaceAct);
  viewToolBar->addAction(drawVectorAct);
  viewToolBar->addAction(drawIsoContourAct);
  viewToolBar->addAction(drawIsoSurfaceAct);
  viewToolBar->addAction(drawStreamLineAct);
  viewToolBar->addSeparator();
  viewToolBar->addAction(drawColorBarAct);
  viewToolBar->addSeparator();
  viewToolBar->addAction(preferencesAct);
  viewToolBar->addSeparator();
  viewToolBar->addAction(redrawAct);
}

void VtkPost::createStatusBar()
{
}

#ifdef MATC
void VtkPost::matcOpenSlot()
{
  matc->show();
}

void VtkPost::matcCutPasteSlot()
{
  matc->ui.mcHistory->copy();
  matc->ui.mcEdit->clear();
  matc->ui.mcEdit->paste();
}

void VtkPost::grad(double *in, double *out)
{
   vtkFloatArray *s = vtkFloatArray::New();
   s->SetNumberOfComponents(1);
   s->SetNumberOfTuples(epMesh->epNodes);
   for( int i=0;i<epMesh->epNodes; i++ )
      s->SetValue(i,in[i] );

   vtkCellDerivatives *cd = vtkCellDerivatives::New();
   if ( volumeGrid->GetNumberOfCells()>0 ) {
     volumeGrid->GetPointData()->SetScalars(s);
     cd->SetInput(volumeGrid);
   } else {
     surfaceGrid->GetPointData()->SetScalars(s);
     cd->SetInput(surfaceGrid);
   }
   cd->SetVectorModeToComputeGradient();
   cd->Update();

   vtkCellDataToPointData *nd = vtkCellDataToPointData::New();
   nd->SetInput(cd->GetOutput());
   nd->Update();

   vtkDataArray *da = nd->GetOutput()->GetPointData()->GetVectors();
   int ncomp = da->GetNumberOfComponents();
   for( int i=0; i<epMesh->epNodes; i++ )
     for( int j=0; j<ncomp; j++ )
        out[epMesh->epNodes*j+i] = da->GetComponent(i,j);

   cd->Delete();
   nd->Delete();
   s->Delete(); 
}

void VtkPost::div(double *in, double *out)
{
   int n=volumeGrid->GetNumberOfCells();
   int ncomp = 3;

   vtkFloatArray *s = vtkFloatArray::New();
   s->SetNumberOfComponents(ncomp);
   s->SetNumberOfTuples(epMesh->epNodes);

   for( int j=0;j<ncomp; j++ )
     for( int i=0;i<epMesh->epNodes; i++ )
      s->SetComponent(i,j,in[j*epMesh->epNodes+i] );

   vtkCellDerivatives *cd = vtkCellDerivatives::New();
   if ( n>0 ) {
     volumeGrid->GetPointData()->SetVectors(s);
     cd->SetInput(volumeGrid);
   } else {
     surfaceGrid->GetPointData()->SetVectors(s);
     cd->SetInput(surfaceGrid);
   }
   cd->SetTensorModeToComputeGradient();
   cd->Update();

   vtkCellDataToPointData *nd = vtkCellDataToPointData::New();
   nd->SetInput(cd->GetOutput());
   nd->Update();

   vtkDataArray *da = nd->GetOutput()->GetPointData()->GetTensors();
   ncomp = da->GetNumberOfComponents();
   for( int i=0; i<epMesh->epNodes; i++ )
   {
      out[i]  = da->GetComponent(i,0);
      out[i] += da->GetComponent(i,4);
      out[i] += da->GetComponent(i,8);
   }
   cd->Delete();
   nd->Delete();
   s->Delete(); 
}

void VtkPost::curl(double *in, double *out)
{
   int n=volumeGrid->GetNumberOfCells();
   int ncomp = 3;

   vtkFloatArray *s = vtkFloatArray::New();
   s->SetNumberOfComponents(ncomp);
   s->SetNumberOfTuples(epMesh->epNodes);

   for( int j=0;j<ncomp; j++ )
     for( int i=0;i<epMesh->epNodes; i++ )
      s->SetComponent(i,j,in[j*epMesh->epNodes+i] );

   vtkCellDerivatives *cd = vtkCellDerivatives::New();
   if ( n>0 ) {
     volumeGrid->GetPointData()->SetVectors(s);
     cd->SetInput(volumeGrid);
   } else {
     surfaceGrid->GetPointData()->SetVectors(s);
     cd->SetInput(surfaceGrid);
   }
   cd->SetTensorModeToComputeGradient();
   cd->Update();

   vtkCellDataToPointData *nd = vtkCellDataToPointData::New();
   nd->SetInput(cd->GetOutput());
   nd->Update();

   vtkDataArray *da = nd->GetOutput()->GetPointData()->GetTensors();
   for( int i=0; i<epMesh->epNodes; i++ )
   {
      double gx_x = da->GetComponent(i,0);
      double gx_y = da->GetComponent(i,3);
      double gx_z = da->GetComponent(i,6);
      double gy_x = da->GetComponent(i,1);
      double gy_y = da->GetComponent(i,4);
      double gy_z = da->GetComponent(i,7);
      double gz_x = da->GetComponent(i,2);
      double gz_y = da->GetComponent(i,5);
      double gz_z = da->GetComponent(i,8);
      out[i] = gz_y-gy_z;
      out[epMesh->epNodes+i] = gx_z-gz_x;
      out[2*epMesh->epNodes+i] = gy_x-gx_y;
   }

   cd->Delete();
   nd->Delete();
   s->Delete(); 
}

void VtkPost::domatcSlot()
{
   char *ptr;
   LIST *lst;
   int i;
   VARIABLE *var;

   QString cmd=matc->ui.mcEdit->text().trimmed();

   matc->ui.mcEdit->clear();

   ptr=mtc_domath(cmd.toAscii().data());
   matc->ui.mcHistory->append(cmd);
   if ( ptr ) matc->ui.mcOutput->append(ptr);

   QString vectorname;
   for( lst = listheaders[VARIABLES].next; lst; lst = NEXT(lst))
   {
     var = (VARIABLE *)lst;
     if ( !NAME(var) || (NCOL(var) % epMesh->epNodes != 0) ) continue;

     int found = false,n;
     for( int i=0; i<scalarFields; i++ )
     {
        ScalarField *sf = &scalarField[i]; 
        if ( NROW(var)==1 && sf->name == NAME(var) )
        {
           found = true;
           if ( sf->value != MATR(var) )
           {
             free(sf->value);
             sf->value = MATR(var);
           }
           sf->minVal =  1e99;
           sf->maxVal = -1e99;
           for(int j=0; j < sf->values; j++) {
             if(sf->value[j] > sf->maxVal) sf->maxVal = sf->value[j];
             if(sf->value[j] < sf->minVal) sf->minVal = sf->value[j];
           }
           break;
        } else if ( NROW(var)==3 ) {
          vectorname = "";
          n=sf->name.indexOf("_x");
          if ( n<=0 ) n=sf->name.indexOf("_y");
          if ( n<=0 ) n=sf->name.indexOf("_z");
          if ( n>0 ) vectorname=sf->name.mid(0,n);

          if ( vectorname==NAME(var) ) {
            found = true;
            if ( sf->name.indexOf("_x")>0 ) {
              if ( sf->value != &M(var,0,0) )
              {
                free(sf->value);
                sf->value = &M(var,0,0);
              }
            } else if ( sf->name.indexOf("_y")>0 ) {
               if ( sf->value != &M(var,1,0) )
               {
                 free(sf->value);
                 sf->value = &M(var,1,0);
               }
            } else if ( sf->name.indexOf("_z")>0 ) {
               if ( sf->value != &M(var,2,0) )
               {
                 free(sf->value);
                 sf->value = &M(var,2,0);
               }
            }
            sf->minVal =  1e99;
            sf->maxVal = -1e99;
            for(int j=0; j<sf->values; j++) {
              if(sf->value[j] > sf->maxVal) sf->maxVal = sf->value[j];
              if(sf->value[j] < sf->minVal) sf->minVal = sf->value[j];
            }
          }
        }
     }

     if ( !found ) 
     {
        if ( NROW(var) == 1 ) {
          ScalarField *sf = addScalarField( NAME(var),NCOL(var),NULL );
          for(int j = 0; j<NCOL(var); j++) {
             if(sf->value[j] > sf->maxVal) sf->maxVal = sf->value[j];
             if(sf->value[j] < sf->minVal) sf->minVal = sf->value[j];
          }
        } else if ( NROW(var) == 3 ) {
          QString qs = NAME(var);
          ScalarField *sf;
          sf = addScalarField( qs+"_x",NCOL(var), &M(var,0,0) );
          for(int j = 0; j<NCOL(var); j++) {
             if(sf->value[j] > sf->maxVal) sf->maxVal = sf->value[j];
             if(sf->value[j] < sf->minVal) sf->minVal = sf->value[j];
          }
          sf = addScalarField( qs+"_y",NCOL(var), &M(var,1,0) );
	  for(int j = 0; j<NCOL(var); j++) {
             if(sf->value[j] > sf->maxVal) sf->maxVal = sf->value[j];
             if(sf->value[j] < sf->minVal) sf->minVal = sf->value[j];
          }
          sf = addScalarField( qs+"_z",NCOL(var), &M(var,2,0) );
	  for(int j = 0; j<NCOL(var); j++) {
             if(sf->value[j] > sf->maxVal) sf->maxVal = sf->value[j];
             if(sf->value[j] < sf->minVal) sf->minVal = sf->value[j];
          }
        }
     }
   }

   int count=0,n;

   for( int i=0; i<scalarFields; i++ )
   {
      ScalarField *sf = &scalarField[i]; 

      vectorname = "";
      n=sf->name.indexOf("_x");
      if ( n<=0 ) n=sf->name.indexOf("_y");
      if ( n<=0 ) n=sf->name.indexOf("_z");
      if ( n>0 ) vectorname=sf->name.mid(0,n);

      for( lst = listheaders[VARIABLES].next; lst; lst = NEXT(lst))
      {
        var = (VARIABLE *)lst;
        if ( !NAME(var) || (NCOL(var) % epMesh->epNodes != 0) ) continue;

        if ( NROW(var)==1 && sf->name == NAME(var) )
        {
          if ( count != i ) scalarField[count]=*sf;
          count++;
          break;
        } else if ( NROW(var)==3 && vectorname==NAME(var) ) {
          if ( count != i ) scalarField[count]=*sf;
          count++;
        }
      }
   }
   if ( count<scalarFields ) scalarFields = count;
   
   // Populate widgets in user interface dialogs:
   //---------------------------------------------
   populateWidgetsSlot();
}
#endif

int VtkPost::NofNodes()
{
   return epMesh->epNodes;
}


// Populate widgets in user interface dialogs:
//----------------------------------------------------------------------
void VtkPost::populateWidgetsSlot()
{
  vector->populateWidgets(scalarField, scalarFields);
  surface->populateWidgets(scalarField, scalarFields);
  isoSurface->populateWidgets(scalarField, scalarFields);
  isoContour->populateWidgets(scalarField, scalarFields);
  surface->populateWidgets(scalarField, scalarFields);
  streamLine->populateWidgets(scalarField, scalarFields);
  colorBar->populateWidgets();
}


// Save picture:
//----------------------------------------------------------------------
void VtkPost::savePictureSlot()
{
  QString fileName = QFileDialog::getSaveFileName(this,
	 tr("Save picture"), "", tr("Picture files (*.png)"));
  
  if(fileName.isEmpty()) {
    cout << "File name is empty" << endl;
    return;
  }

  vtkWindowToImageFilter *image = vtkWindowToImageFilter::New();

  image->SetInput(qvtkWidget->GetRenderWindow());
  image->Update();

  vtkPNGWriter *writer = vtkPNGWriter::New();

  writer->SetInputConnection(image->GetOutputPort());
  writer->SetFileName(fileName.toAscii().data());

  qvtkWidget->GetRenderWindow()->Render();
  writer->Write();

  writer->Delete();
  image->Delete();
}


bool VtkPost::readPostFile(QString postFileName)
{
  QString tmpLine;
  QTextStream txtStream;

#define GET_TXT_STREAM                               \
  tmpLine = post.readLine().trimmed();               \
  while(tmpLine.isEmpty() || (tmpLine.at(0) == '#')) \
    tmpLine = post.readLine();                       \
  txtStream.setString(&tmpLine);

  // Open the post file:
  //=====================
  this->postFileName = postFileName;
  this->postFileRead = false;

  QFile postFile(postFileName);

  if(!postFile.open(QIODevice::ReadOnly | QIODevice::Text))
    return false;

  cout << "Loading ep-file" << endl;
  
  QTextStream post(&postFile);

  // Read in nodes, elements, timesteps, and scalar components:
  //-----------------------------------------------------------
  GET_TXT_STREAM

  int nodes, elements, timesteps, components;

  txtStream >> nodes >> elements >> components >> timesteps;

  cout << "Nodes: " << nodes << endl;
  cout << "Elements: " << elements << endl;
  cout << "Scalar components: " << components << endl;
  cout << "Timesteps: " << timesteps << endl;

  timeStep->maxSteps = timesteps;
  timeStep->ui.start->setValue(1);
  timeStep->ui.stop->setValue(timesteps);

  // Read field names & set up menu actions:
  //=========================================
  if(epMesh->epNode) delete [] epMesh->epNode;
  if(epMesh->epElement) delete [] epMesh->epElement;

  for(int i = 0; i < scalarFields; i++ ) {
     ScalarField *sf = &scalarField[i];
#ifdef MATC
     QByteArray nm = sf->name.trimmed().toAscii();
     var_delete( nm.data() );
#else
     if(sf->value) free(sf->value);
#endif
  }

  scalarFields = 0;

  // Add the null field:
  //--------------------
  QString fieldName = "Null";
  ScalarField *nullField = addScalarField(fieldName, nodes * timesteps, NULL);
  nullField->minVal = 0.0;
  nullField->maxVal = 0.0;

  // Add the scalar fields:
  //-----------------------
  for(int i = 0; i < components; i++) {
    QString fieldType, fieldName;
    txtStream >> fieldType >> fieldName;

    fieldType.replace(":", "");
    fieldType = fieldType.trimmed();
    fieldName = fieldName.trimmed();

    cout << "Field type: " << fieldType.toAscii().data() << endl;
    cout << "Field name: " << fieldName.toAscii().data() << endl;

    if(fieldType == "scalar")
      addScalarField(fieldName, nodes * timesteps, NULL);

    if(fieldType == "vector") {
      addVectorField(fieldName, nodes * timesteps);
      i += 2;
    }
  }

  // Nodes:
  //========
  epMesh->epNodes = nodes;
  epMesh->epNode = new EpNode[nodes];
  
  for(int i = 0; i < nodes; i++) {
    EpNode *epn = &epMesh->epNode[i];
    
    GET_TXT_STREAM

    for(int j = 0; j < 3; j++) 
      txtStream >> epn->x[j];
  }

  // Add nodes to field variables:
  //-------------------------------
  addVectorField("nodes", nodes);
  int index = -1;
  for(int i = 0; i < scalarFields; i++) {
    ScalarField *sf = &scalarField[i];
    if(sf->name == "nodes_x") {
      index = i;
      break;
    }
  }
  ScalarField *sfx = &scalarField[index+0];
  ScalarField *sfy = &scalarField[index+1];
  ScalarField *sfz = &scalarField[index+2];
  for( int i=0; i < nodes; i++ )
  {
    sfx->value[i] = epMesh->epNode[i].x[0];
    sfy->value[i] = epMesh->epNode[i].x[1];
    sfz->value[i] = epMesh->epNode[i].x[2];
  }

  // Elements:
  //==========
  epMesh->epElements = elements;
  epMesh->epElement = new EpElement[elements];

  for(int i = 0; i < elements; i++) {
    EpElement *epe = &epMesh->epElement[i];
    
    GET_TXT_STREAM
    
    txtStream >> epe->groupName >> epe->code;
    
    epe->indexes = epe->code % 100;
    epe->index = new int[epe->indexes];
    
    for(int j = 0; j < epe->indexes; j++) {
      QString tmpString = "";
      txtStream >> tmpString;
      if(tmpString.isEmpty()) {
	GET_TXT_STREAM
        txtStream >> tmpString;
      }
      epe->index[j] = tmpString.toInt();
    }
  }

  // Data:
  //=======
  ScalarField *sf;
  for(int i = 0; i < nodes * timesteps; i++) {
    GET_TXT_STREAM

    for(int j = 0; j < scalarFields-4; j++) { // - 4 = no nodes, no null field
      sf = &scalarField[j+1];                 // + 1 = skip null field
      txtStream >> sf->value[i];
    }
  }

  // Initial min & max values:
  //============================
  for( int f=0; f<scalarFields; f++  )
  {
     sf = &scalarField[f];
     for( int i=0; i < nodes * timesteps; i++ )
     {
       if(sf->value[i] > sf->maxVal)
         sf->maxVal = sf->value[i];
       if(sf->value[i] < sf->minVal)
         sf->minVal = sf->value[i];
     }
   }
  
  postFile.close();

  // Set up the group edit menu:
  //=============================
  groupActionHash.clear();
  editGroupsMenu->clear();

  for(int i = 0; i < elements; i++) {
    EpElement *epe = &epMesh->epElement[i];

    QString groupName = epe->groupName;
    
    if(groupActionHash.contains(groupName))
      continue;

    QAction *groupAction = new QAction(groupName, this);
    groupAction->setCheckable(true);
    groupAction->setChecked(true);
    editGroupsMenu->addAction(groupAction);
    groupActionHash.insert(groupName, groupAction);
  }

  // Populate the widgets in user interface dialogs:
  //-------------------------------------------------
  populateWidgetsSlot();

  this->postFileRead = true;

  groupChangedSlot(NULL);
  connect(editGroupsMenu, SIGNAL(triggered(QAction*)), this, SLOT(groupChangedSlot(QAction*)));
  
  editGroupsMenu->addSeparator();
  editGroupsMenu->addAction(regenerateGridsAct);

  // Set the null field active:
  //---------------------------
  drawSurfaceAct->setChecked(true);
  redrawSlot();

  renderer->ResetCamera();
  
  return true;
}

void VtkPost::addVectorField(QString fieldName, int nodes)
{
   
#ifdef MATC
    QByteArray nm=fieldName.trimmed().toAscii();

    char *name = (char *)malloc( nm.count()+1 );
    strcpy(name,nm.data());

    VARIABLE *var = var_check(name);
    if ( !var || NROW(var) != 3 || NCOL(var) != nodes )
      var = var_new( name, TYPE_DOUBLE, 3, nodes );
    free(name);

   addScalarField(fieldName+"_x", nodes, &M(var,0,0));
   addScalarField(fieldName+"_y", nodes, &M(var,1,0));
   addScalarField(fieldName+"_z", nodes, &M(var,2,0));
#else
   addScalarField(fieldName+"_x", nodes, NULL);
   addScalarField(fieldName+"_y", nodes, NULL);
   addScalarField(fieldName+"_z", nodes, NULL);
#endif
}


// Add a scalar field:
//----------------------------------------------------------------------
ScalarField* VtkPost::addScalarField(QString fieldName, int nodes, double *value)
{
  if(scalarFields >= MAX_SCALARS) {
    cout << "Max. scalar limit exceeded!" << endl;
    return NULL;
  }

  ScalarField *sf = &scalarField[scalarFields++];
  sf->name = fieldName;
  sf->values = nodes;
  sf->value = value;
 
  if ( !sf->value ) {
#ifdef MATC
    QByteArray nm=fieldName.trimmed().toAscii();

    char *name = (char *)malloc( nm.count()+1 );
    strcpy(name,nm.data());
    VARIABLE *var = var_check(name);
    if ( !var || NROW(var)!=1 || NCOL(var) != nodes )
      var = var_new( name, TYPE_DOUBLE, 1, nodes );
    sf->value = MATR(var);
    free(name);
#else
    sf->value = (double *)calloc(nodes,sizeof(double));
#endif
  }

  sf->minVal = +9.9e99;
  sf->maxVal = -9.9e99;

  return sf;
}


// Close the widget:
//----------------------------------------------------------------------
void VtkPost::exitSlot()
{
  close();
}



// Group selection changed:
//----------------------------------------------------------------------
void VtkPost::regenerateGridsSlot()
{
  groupChangedSlot(NULL);
}

void VtkPost::groupChangedSlot(QAction *groupAction)
{
  // Status of groupAction has changed: regenerate grids
  //-----------------------------------------------------
  volumeGrid->Delete();
  surfaceGrid->Delete();
  lineGrid->Delete();

  volumeGrid = vtkUnstructuredGrid::New();
  surfaceGrid = vtkUnstructuredGrid::New();
  lineGrid = vtkUnstructuredGrid::New();

  // Points:
  //---------
  int index = -1;
  for(int i = 0; i < scalarFields; i++) {
    ScalarField *sf = &scalarField[i];
    if(sf->name == "nodes_x") {
      index = i;
      break;
    }
  }
  
  if((index < 0) || (index + 2 > scalarFields - 1)) return;

  double x[3];
  ScalarField *sfx = &scalarField[index+0];
  ScalarField *sfy = &scalarField[index+1];
  ScalarField *sfz = &scalarField[index+2];
  
  vtkPoints *points = vtkPoints::New();
  points->SetNumberOfPoints(epMesh->epNodes);

  for(int i = 0; i < epMesh->epNodes; i++) {
    x[0] = sfx->value[i];
    x[1] = sfy->value[i];
    x[2] = sfz->value[i];
    points->InsertPoint(i, x);
  }
  volumeGrid->SetPoints(points);
  surfaceGrid->SetPoints(points);
  lineGrid->SetPoints(points);
  points->Delete();

  // Volume grid:
  //---------------
  vtkTetra *tetra = vtkTetra::New();
  vtkHexahedron *hexa = vtkHexahedron::New();

  for(int i = 0; i < epMesh->epElements; i++) {
    EpElement *epe = &epMesh->epElement[i];

    if(epe->code == 504) {
      QString groupName = epe->groupName;
      if(groupName.isEmpty()) continue;

      QAction *groupAction = groupActionHash.value(groupName);
      if(groupAction == NULL) continue;
      
      for(int j = 0; j < 4; j++)
	tetra->GetPointIds()->SetId(j, epe->index[j]);
      
      if(groupAction->isChecked())
	volumeGrid->InsertNextCell(tetra->GetCellType(), tetra->GetPointIds());
    }
    
    if(epe->code == 808) {
      QString groupName = epe->groupName;
      if(groupName.isEmpty()) continue;
      
      QAction *groupAction = groupActionHash.value(groupName);
      if(groupAction == NULL) continue;
      
      for(int j = 0; j < 8; j++)
	hexa->GetPointIds()->SetId(j, epe->index[j]);
      
      if(groupAction->isChecked())
	volumeGrid->InsertNextCell(hexa->GetCellType(), hexa->GetPointIds());
    }
  }
  tetra->Delete();
  hexa->Delete();

  // Surface grid:
  //---------------
  vtkTriangle *tria = vtkTriangle::New();
  vtkQuad *quad = vtkQuad::New();
  for(int i = 0; i < epMesh->epElements; i++) {
    EpElement *epe = &epMesh->epElement[i];

    if(epe->code == 303) {
      QString groupName = epe->groupName;
      if(groupName.isEmpty()) continue;

      QAction *groupAction = groupActionHash.value(groupName);
      if(groupAction == NULL) continue;
      
      for(int j = 0; j < 3; j++)
	tria->GetPointIds()->SetId(j, epe->index[j]);
      
      if(groupAction->isChecked())
	surfaceGrid->InsertNextCell(tria->GetCellType(), tria->GetPointIds());
    }

    if(epe->code == 404) {
      QString groupName = epe->groupName;
      if(groupName.isEmpty()) continue;

      QAction *groupAction = groupActionHash.value(groupName);
      if(groupAction == NULL) continue;
      
      for(int j = 0; j < 4; j++)
	quad->GetPointIds()->SetId(j, epe->index[j]);
      
      if(groupAction->isChecked())
	surfaceGrid->InsertNextCell(quad->GetCellType(), quad->GetPointIds());
    }

  }
  tria->Delete();
  quad->Delete();

  // Line grid:
  //---------------
  vtkLine *line = vtkLine::New();
  for(int i = 0; i < epMesh->epElements; i++) {
    EpElement *epe = &epMesh->epElement[i];

    if(epe->code == 202) {
      QString groupName = epe->groupName;
      if(groupName.isEmpty()) continue;

      QAction *groupAction = groupActionHash.value(groupName);
      if(groupAction == NULL) continue;
      
      for(int j = 0; j < 3; j++)
	line->GetPointIds()->SetId(j, epe->index[j]);
      
      if(groupAction->isChecked())
	lineGrid->InsertNextCell(line->GetCellType(), line->GetPointIds());
    }

  }
  line->Delete();

  redrawSlot();
}



// Show preferences dialog:
//----------------------------------------------------------------------
void VtkPost::showPreferencesDialogSlot()
{
  preferences->show();
}



// Maybe redraw:
//----------------------------------------------------------------------
void VtkPost::maybeRedrawSlot(bool value)
{
  if(!value) redrawSlot();
}


// Redraw:
//----------------------------------------------------------------------
void VtkPost::redrawSlot()
{  
  drawMeshPointSlot();
  drawMeshEdgeSlot();
  drawFeatureEdgesSlot();
  drawSurfaceSlot();
  drawVectorSlot();
  drawIsoContourSlot();
  drawIsoSurfaceSlot();
  drawStreamLineSlot();
  drawColorBarSlot();

  vtkRenderWindow *renderWindow = qvtkWidget->GetRenderWindow();

  renderWindow->Render();

  timeStep->canProceedWithNext(renderWindow);
}


// Draw color bar:
//----------------------------------------------------------------------
void VtkPost::showColorBarDialogSlot()
{
  qvtkWidget->GetRenderWindow()->Render();

  if(drawColorBarAct->isChecked()) {
    colorBar->show();
  } else {
    colorBar->close();
    drawColorBarSlot();
  }
}

void VtkPost::hideColorBarSlot()
{
  drawColorBarAct->setChecked(false);
  drawColorBarSlot();
}

void VtkPost::drawColorBarSlot()
{
  renderer->RemoveActor(colorBarActor);
  if(!drawColorBarAct->isChecked()) return;

  // Draw color bar:
  //----------------
  vtkTextMapper *tMapper = vtkTextMapper::New();
  colorBarActor->SetMapper(tMapper);

  QString actorName = colorBar->ui.colorCombo->currentText().trimmed();

  if(actorName.isEmpty()) return;

  QString fieldName = "";

  vtkScalarsToColors *lut = NULL;

  if(actorName == "Surface") {
    fieldName = currentSurfaceName;
    if(fieldName.isEmpty()) return;
    lut = surfaceActor->GetMapper()->GetLookupTable();
  }

  if(actorName == "Vector") {
    fieldName = currentVectorName;
    if(fieldName.isEmpty()) return;
    lut = vectorActor->GetMapper()->GetLookupTable();
  }

  if(actorName == "Isocontour") {
    fieldName = currentIsoContourName;
    if(fieldName.isEmpty()) return;
    lut = isoContourActor->GetMapper()->GetLookupTable();
  }

  if(actorName == "Isosurface") {
    fieldName = currentIsoSurfaceName;
    if(fieldName.isEmpty()) return;
    lut = isoSurfaceActor->GetMapper()->GetLookupTable();
  }

  if(actorName == "Streamline") {
    fieldName = currentStreamLineName;
    if(fieldName.isEmpty()) return;
    lut = streamLineActor->GetMapper()->GetLookupTable();
  }

  if(!lut) return;

  colorBarActor->SetLookupTable(lut);

  bool horizontal = colorBar->ui.horizontalRButton->isChecked();
  bool annotate = colorBar->ui.annotateBox->isChecked();
  int labels = colorBar->ui.labelsSpin->value();
  double width = colorBar->ui.widthEdit->text().toDouble();
  double height = colorBar->ui.heightEdit->text().toDouble();

  if(width < 0.01) width = 0.01;
  if(width > 1.00) width = 1.00;
  if(height < 0.01) height = 0.01;
  if(height > 1.00) height = 1.00;

  colorBarActor->SetPosition(0.05, 0.05);

  if(horizontal) {
    colorBarActor->SetOrientationToHorizontal();
    colorBarActor->SetWidth(height);
    colorBarActor->SetHeight(width);
  } else {
    colorBarActor->SetOrientationToVertical();
    colorBarActor->SetWidth(width);
    colorBarActor->SetHeight(height);
  }
  
  colorBarActor->SetNumberOfLabels(labels);

  colorBarActor->GetLabelTextProperty()->SetFontSize(16);
  colorBarActor->GetLabelTextProperty()->SetFontFamilyToArial();
  colorBarActor->GetLabelTextProperty()->BoldOn();
  colorBarActor->GetLabelTextProperty()->ItalicOn();
  colorBarActor->GetLabelTextProperty()->SetColor(0, 0, 1);
  
  colorBarActor->GetTitleTextProperty()->SetFontSize(16);
  colorBarActor->GetTitleTextProperty()->SetFontFamilyToArial();
  colorBarActor->GetTitleTextProperty()->BoldOn();
  colorBarActor->GetTitleTextProperty()->ItalicOn();
  colorBarActor->GetTitleTextProperty()->SetColor(0, 0, 1);
  
  if(annotate) {
    colorBarActor->SetTitle(fieldName.toAscii().data());
  } else {
    colorBarActor->SetTitle("");
  }

  renderer->AddActor(colorBarActor);
  qvtkWidget->GetRenderWindow()->Render();

  tMapper->Delete();
}


// Draw node points (labeled with node index):
//----------------------------------------------------------------------
void VtkPost::drawMeshPointSlot()
{
  renderer->RemoveActor(meshPointActor);
  if(!drawMeshPointAct->isChecked()) return;
  
  double length = surfaceGrid->GetLength();
  int pointQuality = preferences->ui.pointQuality->value();
  int pointSize = preferences->ui.pointSize->value();
  bool useSurfaceGrid = preferences->ui.meshPointsSurface->isChecked();

  vtkSphereSource *sphere = vtkSphereSource::New();
  sphere->SetRadius((double)pointSize * length / 2000.0);
  sphere->SetThetaResolution(pointQuality);
  sphere->SetPhiResolution(pointQuality);

  vtkUnstructuredGrid *grid = NULL;

  if(useSurfaceGrid) {
    grid = surfaceGrid;
  } else {
    grid = volumeGrid;
  }

  if(!grid) return;
  if(grid->GetNumberOfPoints() < 1) return;

  vtkGlyph3D *glyph = vtkGlyph3D::New();

  glyph->SetInput(grid);
  glyph->SetSourceConnection(sphere->GetOutputPort());

  vtkPolyDataMapper *mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(glyph->GetOutputPort());
  mapper->ScalarVisibilityOff();

  meshPointActor->SetMapper(mapper);
  meshPointActor->GetProperty()->SetColor(0.5, 0.5, 0.5);
  
  renderer->AddActor(meshPointActor);

  qvtkWidget->GetRenderWindow()->Render();

  glyph->Delete();
  sphere->Delete();
  mapper->Delete();
}


// Draw mesh edges:
//----------------------------------------------------------------------
void VtkPost::drawMeshEdgeSlot()
{
  renderer->RemoveActor(meshEdgeActor);
  if(!drawMeshEdgeAct->isChecked()) return;

  bool useSurfaceGrid = preferences->ui.meshEdgesSurface->isChecked();
  int lineWidth = preferences->ui.meshLineWidth->value();

  vtkUnstructuredGrid *grid = NULL;

  if(useSurfaceGrid) {
    grid = surfaceGrid;
  } else {
    grid = volumeGrid;
  }

  if(!grid) return;
  if(grid->GetNumberOfCells() < 1) return;

  vtkExtractEdges *edges = vtkExtractEdges::New();
  edges->SetInput(grid);

  vtkDataSetMapper *mapper = vtkDataSetMapper::New();
  mapper->SetInputConnection(edges->GetOutputPort());
  mapper->ScalarVisibilityOff();
  mapper->SetResolveCoincidentTopologyToPolygonOffset();
  // mapper->ImmediateModeRenderingOn();

  meshEdgeActor->GetProperty()->SetLineWidth(lineWidth);
  meshEdgeActor->GetProperty()->SetColor(0, 0, 0);
  meshEdgeActor->SetMapper(mapper);

  renderer->AddActor(meshEdgeActor);

  qvtkWidget->GetRenderWindow()->Render();

  mapper->Delete();
  edges->Delete();
}


// Draw feature edges:
//----------------------------------------------------------------------
void VtkPost::drawFeatureEdgesSlot()
{
  renderer->RemoveActor(featureEdgeActor);
  if(!drawFeatureEdgesAct->isChecked()) return;

  bool useSurfaceGrid = preferences->ui.surfaceRButton->isChecked();
  int featureAngle = preferences->ui.angleSpin->value();
  int lineWidth = preferences->ui.lineWidthSpin->value();
  
  vtkUnstructuredGrid *grid = NULL;

  if(useSurfaceGrid) {
    grid = surfaceGrid;
  } else {
    grid = volumeGrid;
  }

  if(!grid) return;
  if(grid->GetNumberOfCells() < 1) return;

  // Convert from vtkUnstructuredGrid to vtkPolyData:
  vtkGeometryFilter *filter = vtkGeometryFilter::New();
  filter->SetInput(grid);
  filter->GetOutput()->ReleaseDataFlagOn();

  vtkFeatureEdges *edges = vtkFeatureEdges::New();
  edges->SetInputConnection(filter->GetOutputPort());
  edges->SetFeatureAngle(featureAngle);
  edges->BoundaryEdgesOn();
  edges->ManifoldEdgesOn();
  edges->NonManifoldEdgesOn();

  vtkPolyDataMapper *mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(edges->GetOutputPort());
  mapper->ScalarVisibilityOff();
  mapper->SetResolveCoincidentTopologyToPolygonOffset();
  // mapper->ImmediateModeRenderingOn();

  featureEdgeActor->GetProperty()->SetLineWidth(lineWidth);
  featureEdgeActor->GetProperty()->SetColor(0, 0, 0);
  featureEdgeActor->SetMapper(mapper);

  renderer->AddActor(featureEdgeActor);
  qvtkWidget->GetRenderWindow()->Render();
  
  filter->Delete();
  edges->Delete();
  mapper->Delete();
}



// Draw stream lines:
//----------------------------------------------------------------------
void VtkPost::showStreamLineDialogSlot()
{
  qvtkWidget->GetRenderWindow()->Render();
  
  if(drawStreamLineAct->isChecked()) {
    streamLine->show();
  } else {
    streamLine->close();
    drawVectorSlot();
  }
}

void VtkPost::hideStreamLineSlot()
{
  drawStreamLineAct->setChecked(false);
  drawStreamLineSlot();
}

void VtkPost::drawStreamLineSlot()
{
  renderer->RemoveActor(streamLineActor);
  if(!drawStreamLineAct->isChecked()) return;

  QString vectorName = streamLine->ui.vectorCombo->currentText();

  if(vectorName.isEmpty()) return;

  int i, j, index = -1;
  for(i = 0; i < scalarFields; i++) {
    ScalarField *sf = &scalarField[i];
    QString name = sf->name;
    if((j = name.indexOf("_x")) >= 0) {
      if(vectorName == name.mid(0, j)) {
	index = i;
	break;
      }
    }
  }

  if(index < 0) return;

  // UI data:
  //----------

  // Controls:
  double propagationTime = streamLine->ui.propagationTime->text().toDouble();
  double stepLength = streamLine->ui.stepLength->text().toDouble();
  double integStepLength = streamLine->ui.integStepLength->text().toDouble();
  int threads = streamLine->ui.threads->value();
  bool useSurfaceGrid = streamLine->ui.useSurfaceGrid->isChecked();
  bool lineSource = streamLine->ui.lineSource->isChecked();
  bool sphereSource = streamLine->ui.sphereSource->isChecked();
  bool pickSource = streamLine->ui.pickSource->isChecked();
  bool forward = streamLine->ui.forward->isChecked();
  bool backward = streamLine->ui.backward->isChecked();

  if(!(forward || backward)) {
    qvtkWidget->GetRenderWindow()->Render();
    return;
  }

  // Color:
  int colorIndex = streamLine->ui.colorCombo->currentIndex();
  QString colorName = streamLine->ui.colorCombo->currentText();
  double minVal = streamLine->ui.minVal->text().toDouble();
  double maxVal = streamLine->ui.maxVal->text().toDouble();

  // Appearance:
  bool drawRibbon = streamLine->ui.drawRibbon->isChecked();
  int ribbonWidth = streamLine->ui.ribbonWidth->value();
  int lineWidth = streamLine->ui.lineWidth->text().toInt();

  // Line source:
  double startX = streamLine->ui.startX->text().toDouble();
  double startY = streamLine->ui.startY->text().toDouble();
  double startZ = streamLine->ui.startZ->text().toDouble();
  double endX = streamLine->ui.endX->text().toDouble();
  double endY = streamLine->ui.endY->text().toDouble();
  double endZ = streamLine->ui.endZ->text().toDouble();
  int lines = streamLine->ui.lines->value();
  bool drawRake = streamLine->ui.rake->isChecked();
  int rakeWidth = streamLine->ui.rakeWidth->value();

  int step = timeStep->ui.timeStep->value();
  if(step > timeStep->maxSteps) step = timeStep->maxSteps;
  int offset = epMesh->epNodes * (step - 1);

  // Sphere source:
  double centerX = streamLine->ui.centerX->text().toDouble();
  double centerY = streamLine->ui.centerY->text().toDouble();
  double centerZ = streamLine->ui.centerZ->text().toDouble();
  double radius = streamLine->ui.radius->text().toDouble();
  int points = streamLine->ui.points->value();

  // Pick source:
  double pickX = currentPickPosition[0];
  double pickY = currentPickPosition[1];
  double pickZ = currentPickPosition[2];
  
  // Choose the grid:
  //------------------
  vtkUnstructuredGrid *grid = NULL;
  if(useSurfaceGrid)
    grid = surfaceGrid;
  else
    grid = volumeGrid;

  if(!grid) return;
  if(grid->GetNumberOfCells() < 1) return;

  // Vector data:
  //-------------
  grid->GetPointData()->RemoveArray("VectorData");
  vtkFloatArray *vectorData = vtkFloatArray::New();
  ScalarField *sf_x = &scalarField[index + 0];
  ScalarField *sf_y = &scalarField[index + 1];
  ScalarField *sf_z = &scalarField[index + 2];
  vectorData->SetNumberOfComponents(3);
  vectorData->SetNumberOfTuples(epMesh->epNodes);
  vectorData->SetName("VectorData");
  for(int i = 0; i < epMesh->epNodes; i++) {
    double val_x  = sf_x->value[i + offset];
    double val_y  = sf_y->value[i + offset];
    double val_z  = sf_z->value[i + offset];
    vectorData->SetComponent(i,0,val_x); 
    vectorData->SetComponent(i,1,val_y); 
    vectorData->SetComponent(i,2,val_z); 
  }
  grid->GetPointData()->AddArray(vectorData);

  // Color data:
  //-------------
  grid->GetPointData()->RemoveArray("StreamLineColor");
  ScalarField *sf = &scalarField[colorIndex];
  vtkFloatArray *vectorColor = vtkFloatArray::New();
  vectorColor->SetNumberOfComponents(1);
  vectorColor->SetNumberOfTuples(epMesh->epNodes);
  vectorColor->SetName("StreamLineColor");
  for(int i = 0; i < epMesh->epNodes; i++) 
    vectorColor->SetComponent(i, 0, sf->value[i + offset]); 
  grid->GetPointData()->AddArray(vectorColor);

  // Generate stream lines:
  //-----------------------
  grid->GetPointData()->SetActiveVectors("VectorData");
  grid->GetPointData()->SetActiveScalars("StreamLineColor");

  vtkPointSource *point = vtkPointSource::New();
  vtkLineSource *line = vtkLineSource::New();
  if(lineSource) {
    line->SetPoint1(startX, startY, startZ);
    line->SetPoint2(endX, endY, endZ);
    line->SetResolution(lines);
  } else {
    if(sphereSource) {
      point->SetCenter(centerX, centerY, centerZ);
      point->SetRadius(radius);
      point->SetNumberOfPoints(points);
      point->SetDistributionToUniform();
    } else {
      point->SetCenter(pickX, pickY, pickZ);
      point->SetRadius(0.0);
      point->SetNumberOfPoints(1);
    }
  }

  vtkStreamLine *streamer = vtkStreamLine::New();
  vtkRungeKutta4 *integrator = vtkRungeKutta4::New();
  streamer->SetInput(grid);
  if(lineSource) {
    streamer->SetSource(line->GetOutput());
  } else {
    streamer->SetSource(point->GetOutput());
  }
  streamer->SetIntegrator(integrator);
  streamer->SetMaximumPropagationTime(propagationTime);
  streamer->SetIntegrationStepLength(integStepLength);
  if(forward && backward) {
    streamer->SetIntegrationDirectionToIntegrateBothDirections();
  } else if(forward) {
    streamer->SetIntegrationDirectionToForward();    
  } else {
    streamer->SetIntegrationDirectionToBackward();
  }
  streamer->SetStepLength(stepLength);
  streamer->SetNumberOfThreads(threads);
  
  vtkRibbonFilter *ribbon = vtkRibbonFilter::New();
  if(drawRibbon) {
    double length = grid->GetLength();
    ribbon->SetInputConnection(streamer->GetOutputPort());
    ribbon->SetWidth(ribbonWidth * length / 1000.0);
    ribbon->SetWidthFactor(5);
  }

  vtkPolyDataMapper *mapper = vtkPolyDataMapper::New();
  mapper->ScalarVisibilityOn();
  mapper->SetScalarRange(minVal, maxVal);

  if(drawRibbon) {
    mapper->SetInputConnection(ribbon->GetOutputPort());
  } else {
    mapper->SetInputConnection(streamer->GetOutputPort());
  }

  mapper->SetColorModeToMapScalars();
  mapper->SetLookupTable(currentLut);

  streamLineActor->SetMapper(mapper);

  if(!drawRibbon)
    streamLineActor->GetProperty()->SetLineWidth(lineWidth);

  renderer->AddActor(streamLineActor);

  // Redraw colorbar:
  //------------------
  currentStreamLineName = colorName;
  drawColorBarSlot();

  qvtkWidget->GetRenderWindow()->Render();

  line->Delete();
  point->Delete();
  vectorData->Delete();
  vectorColor->Delete();
  integrator->Delete();
  streamer->Delete();
  ribbon->Delete();
  mapper->Delete();

  // Draw rake
  //-----------
  if(drawRake) {
    // todo
  }

}



// Draw vectors:
//----------------------------------------------------------------------
void VtkPost::showVectorDialogSlot()
{
  qvtkWidget->GetRenderWindow()->Render();

  if(drawVectorAct->isChecked()) {
    vector->show();
  } else {
    vector->close();
    drawVectorSlot();
  }
}

void VtkPost::hideVectorSlot()
{
  drawVectorAct->setChecked(false);
  drawVectorSlot();
}

void VtkPost::drawVectorSlot()
{
  renderer->RemoveActor(vectorActor);
  if(!drawVectorAct->isChecked()) return;

  QString vectorName = vector->ui.vectorCombo->currentText();

  if(vectorName.isEmpty()) return;

  int i, j, index = -1;
  for(i = 0; i < scalarFields; i++) {
    ScalarField *sf = &scalarField[i];
    QString name = sf->name;
    if((j = name.indexOf("_x")) >= 0) {
      if(vectorName == name.mid(0, j)) {
	index = i;
	break;
      }
    }
  }

  if(index < 0) return;

  // UI data:
  //----------
  int colorIndex = vector->ui.colorCombo->currentIndex();
  QString colorName = vector->ui.colorCombo->currentText();
  double minVal = vector->ui.minVal->text().toDouble();
  double maxVal = vector->ui.maxVal->text().toDouble();
  int quality = vector->ui.qualitySpin->value();
  int scaleMultiplier = vector->ui.scaleSpin->value();
  bool scaleByMagnitude = vector->ui.scaleByMagnitude->isChecked();

  int step = timeStep->ui.timeStep->value();
  if(step > timeStep->maxSteps) step = timeStep->maxSteps;
  int offset = epMesh->epNodes * (step - 1);

  // Vector data:
  //-------------
  volumeGrid->GetPointData()->RemoveArray("VectorData");
  vtkFloatArray *vectorData = vtkFloatArray::New();
  ScalarField *sf_x = &scalarField[index + 0];
  ScalarField *sf_y = &scalarField[index + 1];
  ScalarField *sf_z = &scalarField[index + 2];
  vectorData->SetNumberOfComponents(3);
  vectorData->SetNumberOfTuples(epMesh->epNodes);
  vectorData->SetName("VectorData");
  double scaleFactor = 0.0;
  for(int i = 0; i < epMesh->epNodes; i++) {
    double val_x  = sf_x->value[i + offset];
    double val_y  = sf_y->value[i + offset];
    double val_z  = sf_z->value[i + offset];
    double absval = sqrt(val_x*val_x + val_y*val_y + val_z*val_z);
    if(absval > scaleFactor) scaleFactor = absval;
    vectorData->SetComponent(i, 0, val_x); 
    vectorData->SetComponent(i, 1, val_y); 
    vectorData->SetComponent(i, 2, val_z); 
  }
  volumeGrid->GetPointData()->AddArray(vectorData);

  // Size of volume grid:
  //---------------------
  double length = volumeGrid->GetLength();
  if(scaleByMagnitude)
    scaleFactor = scaleFactor * 100.0 / length;

  // Color data:
  //-------------
  volumeGrid->GetPointData()->RemoveArray("VectorColor");
  ScalarField *sf = &scalarField[colorIndex];
  vtkFloatArray *vectorColor = vtkFloatArray::New();
  vectorColor->SetNumberOfComponents(1);
  vectorColor->SetNumberOfTuples(epMesh->epNodes);
  vectorColor->SetName("VectorColor");
  for(int i = 0; i < epMesh->epNodes; i++) 
    vectorColor->SetComponent(i, 0, sf->value[i + offset]); 
  volumeGrid->GetPointData()->AddArray(vectorColor);

  // Glyphs:
  //---------
  volumeGrid->GetPointData()->SetActiveVectors("VectorData"); 
  vtkGlyph3D *glyph = vtkGlyph3D::New();
  vtkArrowSource *arrow = vtkArrowSource::New();
  arrow->SetTipResolution(quality);
  arrow->SetShaftResolution(quality);
  glyph->SetInput(volumeGrid);
  glyph->SetSourceConnection(arrow->GetOutputPort());
  glyph->SetVectorModeToUseVector();

  if(scaleByMagnitude) {
    glyph->SetScaleFactor(scaleMultiplier / scaleFactor);
    glyph->SetScaleModeToScaleByVector();
  } else {
    glyph->SetScaleFactor(scaleMultiplier * length  / 100.0);
    glyph->ScalingOn();
  }
  glyph->SetColorModeToColorByScale();
  
  vtkPolyDataMapper *mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(glyph->GetOutputPort());
  mapper->SetScalarModeToUsePointFieldData();
  mapper->ScalarVisibilityOn();
  mapper->SetScalarRange(minVal, maxVal);
  mapper->SelectColorArray("VectorColor");
  mapper->SetLookupTable(currentLut);
  // mapper->ImmediateModeRenderingOn();

  vectorActor->SetMapper(mapper);
  renderer->AddActor(vectorActor);

  // Update color bar && field name:
  //---------------------------------
  currentVectorName = colorName;
  drawColorBarSlot();

  qvtkWidget->GetRenderWindow()->Render();

  mapper->Delete();
  arrow->Delete();
  glyph->Delete();
  vectorData->Delete();
  vectorColor->Delete();
}



// Draw surfaces:
//----------------------------------------------------------------------
void VtkPost::showSurfaceDialogSlot()
{
  qvtkWidget->GetRenderWindow()->Render();

  if(drawSurfaceAct->isChecked()) {
    surface->show();
  } else {
    surface->close();
    drawSurfaceSlot();
  }
}

void VtkPost::hideSurfaceSlot()
{
  drawSurfaceAct->setChecked(false);
  drawSurfaceSlot();
}

void VtkPost::drawSurfaceSlot()
{
  renderer->RemoveActor(surfaceActor);
  if(!drawSurfaceAct->isChecked()) return;

  // Data from UI:
  //--------------
  int surfaceIndex = surface->ui.surfaceCombo->currentIndex();
  QString surfaceName = surface->ui.surfaceCombo->currentText();
  double minVal = surface->ui.minEdit->text().toDouble();
  double maxVal = surface->ui.maxEdit->text().toDouble();
  bool useNormals = surface->ui.useNormals->isChecked();
  int featureAngle = surface->ui.featureAngle->value();
  double opacity = surface->ui.opacitySpin->value() / 100.0;
  bool useClip = surface->ui.clipPlane->isChecked();

  int step = timeStep->ui.timeStep->value();
  if(step > timeStep->maxSteps) step = timeStep->maxSteps;
  int offset = epMesh->epNodes * (step - 1);

  // Scalars:
  //---------
  surfaceGrid->GetPointData()->RemoveArray("Surface");
  vtkFloatArray *scalars = vtkFloatArray::New();
  ScalarField *sf = &scalarField[surfaceIndex];
  scalars->SetNumberOfComponents(1);
  scalars->SetNumberOfTuples(epMesh->epNodes);
  scalars->SetName("Surface");
  for(int i = 0; i < epMesh->epNodes; i++)
    scalars->SetComponent(i, 0, sf->value[i + offset]);  
  surfaceGrid->GetPointData()->AddArray(scalars);

  // Convert from vtkUnstructuredGrid to vtkPolyData:
  //-------------------------------------------------
  vtkGeometryFilter *filter = vtkGeometryFilter::New();

  filter->SetInput(surfaceGrid);
  filter->GetOutput()->ReleaseDataFlagOn();

  // Apply the clip plane:
  //-----------------------
  vtkClipPolyData *clipper = vtkClipPolyData::New();

  if(useClip) {
    setupClipPlane();
    clipper->SetInputConnection(filter->GetOutputPort());
    clipper->SetClipFunction(clipPlane);
    clipper->GenerateClipScalarsOn();
    clipper->GenerateClippedOutputOn();
  }

  // Normals:
  //---------
  vtkPolyDataNormals *normals = vtkPolyDataNormals::New();
  
  if(useNormals) {
    if(useClip) {
      normals->SetInputConnection(clipper->GetOutputPort());
    } else {
      normals->SetInputConnection(filter->GetOutputPort());
    }
    normals->SetFeatureAngle(featureAngle);
  }

  // Mapper:
  //--------
  vtkDataSetMapper *mapper = vtkDataSetMapper::New();

  if(useNormals) {
    mapper->SetInputConnection(normals->GetOutputPort());
  } else {
    if(useClip) {
      mapper->SetInputConnection(clipper->GetOutputPort());
    } else {
      mapper->SetInput(surfaceGrid);
    }
  }

  mapper->SetScalarModeToUsePointFieldData();
  mapper->SelectColorArray("Surface");
  mapper->ScalarVisibilityOn();
  mapper->SetScalarRange(minVal, maxVal);
  mapper->SetResolveCoincidentTopologyToPolygonOffset();
  mapper->SetLookupTable(currentLut);
  // mapper->ImmediateModeRenderingOn();

  // Actor & renderer:
  //------------------
  surfaceActor->SetMapper(mapper);
  surfaceActor->GetProperty()->SetOpacity(opacity);
  renderer->AddActor(surfaceActor);

  // Update color bar && field name:
  //---------------------------------
  currentSurfaceName = sf->name;
  drawColorBarSlot();

  qvtkWidget->GetRenderWindow()->Render();

  // Clean up:
  //-----------
  clipper->Delete();
  normals->Delete();
  filter->Delete();
  scalars->Delete();
  mapper->Delete();
}



// Draw isosurfaces (3D):
//----------------------------------------------------------------------
void VtkPost::showIsoSurfaceDialogSlot()
{
  qvtkWidget->GetRenderWindow()->Render();

  if(drawIsoSurfaceAct->isChecked()) {
    isoSurface->show();
  } else {
    isoSurface->close();
    drawIsoSurfaceSlot();
  }
}

void VtkPost::hideIsoSurfaceSlot()
{
  drawIsoSurfaceAct->setChecked(false);
  drawIsoSurfaceSlot();
}

void VtkPost::drawIsoSurfaceSlot()
{
  renderer->RemoveActor(isoSurfaceActor);
  if(!drawIsoSurfaceAct->isChecked()) return;

  // Data from UI:
  //--------------
  int contourIndex = isoSurface->ui.contoursCombo->currentIndex();
  QString contourName = isoSurface->ui.contoursCombo->currentText();
  int contours = isoSurface->ui.contoursSpin->value() + 1;
  double contourMinVal = isoSurface->ui.contoursMinEdit->text().toDouble();
  double contourMaxVal = isoSurface->ui.contoursMaxEdit->text().toDouble();
  bool useNormals = isoSurface->ui.normalsCheck->isChecked();
  int colorIndex = isoSurface->ui.colorCombo->currentIndex();
  QString colorName = isoSurface->ui.colorCombo->currentText();
  double colorMinVal = isoSurface->ui.colorMinEdit->text().toDouble();
  double colorMaxVal = isoSurface->ui.colorMaxEdit->text().toDouble();
  int featureAngle = isoSurface->ui.featureAngle->value();
  double opacity = isoSurface->ui.opacitySpin->value() / 100.0;
  bool useClip = isoSurface->ui.clipPlane->isChecked();

  int step = timeStep->ui.timeStep->value();
  if(step > timeStep->maxSteps) step = timeStep->maxSteps;
  int offset = epMesh->epNodes * (step - 1);

  if(contourName == "Null") return;

  // Scalars:
  //----------
  volumeGrid->GetPointData()->RemoveArray("IsoSurface");
  vtkFloatArray *contourArray = vtkFloatArray::New();
  ScalarField *sf = &scalarField[contourIndex];
  contourArray->SetNumberOfComponents(1);
  contourArray->SetNumberOfTuples(epMesh->epNodes);
  contourArray->SetName("IsoSurface");
  for(int i = 0; i < epMesh->epNodes; i++)
    contourArray->SetComponent(i, 0, sf->value[i + offset]);
  volumeGrid->GetPointData()->AddArray(contourArray);

  volumeGrid->GetPointData()->RemoveArray("IsoSurfaceColor");
  vtkFloatArray *colorArray = vtkFloatArray::New();
  sf = &scalarField[colorIndex];
  colorArray->SetName("IsoSurfaceColor");
  colorArray->SetNumberOfComponents(1);
  colorArray->SetNumberOfTuples(epMesh->epNodes);
  for(int i = 0; i < epMesh->epNodes; i++)
    colorArray->SetComponent(i, 0, sf->value[i + offset]);
  volumeGrid->GetPointData()->AddArray(colorArray);

  // Isosurfaces:
  //--------------
  vtkContourFilter *iso = vtkContourFilter::New();
  volumeGrid->GetPointData()->SetActiveScalars("IsoSurface");
  iso->SetInput(volumeGrid);
  iso->ComputeScalarsOn();
  iso->GenerateValues(contours, contourMinVal, contourMaxVal);

  // Apply the clip plane:
  //-----------------------
  vtkClipPolyData *clipper = vtkClipPolyData::New();

  if(useClip) {
    setupClipPlane();
    clipper->SetInputConnection(iso->GetOutputPort());
    clipper->SetClipFunction(clipPlane);
    clipper->GenerateClipScalarsOn();
    clipper->GenerateClippedOutputOn();
  }

  // Normals:
  //---------
  vtkPolyDataNormals *normals = vtkPolyDataNormals::New();

  if(useNormals) {
    if(useClip) {
      normals->SetInputConnection(clipper->GetOutputPort());
    } else {
      normals->SetInputConnection(iso->GetOutputPort());
    }
    normals->SetFeatureAngle(featureAngle);
  }

  // Mapper:
  //--------
  vtkDataSetMapper *mapper = vtkDataSetMapper::New();
  
  if(useNormals) {
    mapper->SetInputConnection(normals->GetOutputPort());
  } else {
    if(useClip) {
      mapper->SetInputConnection(clipper->GetOutputPort());      
    } else {
      mapper->SetInputConnection(iso->GetOutputPort());
    }
  }

  mapper->ScalarVisibilityOn();
  mapper->SelectColorArray("IsoSurfaceColor");
  mapper->SetScalarModeToUsePointFieldData();
  mapper->SetScalarRange(colorMinVal, colorMaxVal);
  mapper->SetLookupTable(currentLut);
  // mapper->ImmediateModeRenderingOn();

  // Actor && renderer:
  //-------------------
  isoSurfaceActor->SetMapper(mapper);
  isoSurfaceActor->GetProperty()->SetOpacity(opacity);
  renderer->AddActor(isoSurfaceActor);

  // Redraw colorbar:
  //------------------
  currentIsoSurfaceName = colorName;
  drawColorBarSlot();

  qvtkWidget->GetRenderWindow()->Render();

  // Clean up:
  //----------
  clipper->Delete();
  contourArray->Delete();
  colorArray->Delete();
  iso->Delete();
  normals->Delete();
  mapper->Delete();
}



// Draw iso contours (2D):
//----------------------------------------------------------------------
void VtkPost::showIsoContourDialogSlot()
{
  qvtkWidget->GetRenderWindow()->Render();

  if(drawIsoContourAct->isChecked()) {
    isoContour->show();
  } else {
    isoContour->close();
    drawIsoContourSlot();
  }
}

void VtkPost::hideIsoContourSlot()
{
  drawIsoContourAct->setChecked(false);
  drawIsoContourSlot();
}

void VtkPost::drawIsoContourSlot()
{
  renderer->RemoveActor(isoContourActor);
  if(!drawIsoContourAct->isChecked()) return;

  // Data from UI:
  //--------------
  int contourIndex = isoContour->ui.contoursCombo->currentIndex();
  QString contourName = isoContour->ui.contoursCombo->currentText();
  int contours = isoContour->ui.contoursSpin->value() + 1;
  int lineWidth = isoContour->ui.lineWidthSpin->value();
  double contourMinVal = isoContour->ui.contoursMinEdit->text().toDouble();
  double contourMaxVal = isoContour->ui.contoursMaxEdit->text().toDouble();
  int colorIndex = isoContour->ui.colorCombo->currentIndex();
  QString colorName = isoContour->ui.colorCombo->currentText();
  double colorMinVal = isoContour->ui.colorMinEdit->text().toDouble();
  double colorMaxVal = isoContour->ui.colorMaxEdit->text().toDouble();

  int step = timeStep->ui.timeStep->value();
  if(step > timeStep->maxSteps) step = timeStep->maxSteps;
  int offset = epMesh->epNodes * (step - 1);

  if(contourName == "Null") return;

  // Scalars:
  //----------
  surfaceGrid->GetPointData()->RemoveArray("IsoContour");
  vtkFloatArray *contourArray = vtkFloatArray::New();
  ScalarField *sf = &scalarField[contourIndex];
  contourArray->SetNumberOfComponents(1);
  contourArray->SetNumberOfTuples(epMesh->epNodes);
  contourArray->SetName("IsoContour");
  for(int i = 0; i < epMesh->epNodes; i++)
    contourArray->SetComponent(i, 0, sf->value[i + offset]);
  surfaceGrid->GetPointData()->AddArray(contourArray);

  surfaceGrid->GetPointData()->RemoveArray("IsoContourColor");
  vtkFloatArray *colorArray = vtkFloatArray::New();
  sf = &scalarField[colorIndex];
  colorArray->SetName("IsoContourColor");
  colorArray->SetNumberOfComponents(1);
  colorArray->SetNumberOfTuples(epMesh->epNodes);
  for(int i = 0; i < epMesh->epNodes; i++)
    colorArray->SetComponent(i, 0, sf->value[i + offset]);
  surfaceGrid->GetPointData()->AddArray(colorArray);

  // Isocontours:
  //--------------
  vtkContourFilter *iso = vtkContourFilter::New();
  surfaceGrid->GetPointData()->SetActiveScalars("IsoContour");
  iso->SetInput(surfaceGrid);
  iso->ComputeScalarsOn();
  iso->GenerateValues(contours, contourMinVal, contourMaxVal);

  // Mapper:
  //--------
  vtkDataSetMapper *mapper = vtkDataSetMapper::New();
  mapper->SetInputConnection(iso->GetOutputPort());
  mapper->ScalarVisibilityOn();
  mapper->SelectColorArray("IsoContourColor");
  mapper->SetScalarModeToUsePointFieldData();
  mapper->SetScalarRange(colorMinVal, colorMaxVal);
  mapper->SetLookupTable(currentLut);
  // mapper->ImmediateModeRenderingOn();

  // Actor & renderer:
  //-------------------
  isoContourActor->SetMapper(mapper);
  isoContourActor->GetProperty()->SetLineWidth(lineWidth);
  renderer->AddActor(isoContourActor);

  // Redraw colorbar:
  //------------------
  currentIsoContourName = colorName;
  drawColorBarSlot();
  
  qvtkWidget->GetRenderWindow()->Render();

  // Clean up:
  //----------
  contourArray->Delete();
  colorArray->Delete();
  iso->Delete();
  mapper->Delete();
}


// Set up the clip plane:
//----------------------------------------------------------------------
void VtkPost::setupClipPlane()
{ 
  double px = preferences->ui.clipPointX->text().toDouble();
  double py = preferences->ui.clipPointY->text().toDouble();
  double pz = preferences->ui.clipPointZ->text().toDouble();
  double nx = preferences->ui.clipNormalX->text().toDouble();
  double ny = preferences->ui.clipNormalY->text().toDouble();
  double nz = preferences->ui.clipNormalZ->text().toDouble();

  clipPlane->SetOrigin(px, py, pz);
  clipPlane->SetNormal(nx, ny, nz);
}

// Time step control:
//----------------------------------------------------------------------
void VtkPost::showTimeStepDialogSlot()
{
  timeStep->show();
}

void VtkPost::timeStepChangedSlot()
{
  redrawSlot();
}

// Fit to windows:
//----------------------------------------------------------------------
void VtkPost::fitToWindowSlot()
{
  renderer->ResetCamera();  
}
