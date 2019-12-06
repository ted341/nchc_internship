#include <Inventor/SbColorRGBA.h>
#include <Inventor/cuda/SoCuda.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoCSGShape.h>
#include <Inventor/nodes/SoGradientBackground.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/drawers/SoLassoScreenDrawer.h>
#include <Inventor/drawers/SoEllipseScreenDrawer.h>
#include <Inventor/drawers/SoPolygonScreenDrawer.h>
#include <Inventor/drawers/SoRectangleScreenDrawer.h>
#include <Inventor/misc/SbExtrusionGenerator.h>

#include <VolumeViz/nodes/SoVolumeGroup.h>
#include <VolumeViz/nodes/SoMultiDataSeparator.h>
#include <VolumeViz/nodes/SoVolumeShader.h>
#include <VolumeViz/nodes/SoVolumeData.h>
#include <VolumeViz/nodes/SoVolumeRender.h>
#include <VolumeViz/nodes/SoVolumeRendering.h>
#include <VolumeViz/nodes/SoVolumeClippingGroup.h>
#include <VolumeViz/nodes/SoVolumeRenderingQuality.h>
#include <VolumeViz/nodes/SoVolumeRenderingPhysicalQuality.h>
#include <VolumeViz/readers/SoVRRasterStackReader.h>

#include <LDM/nodes/SoDataRange.h>
#include <LDM/nodes/SoTransferFunction.h>
#include <LDM/nodes/SoDataCompositor.h>
#include <LDM/manips/SoROIManip.h>
#include <DialogViz/SoDialogVizAll.h>
#include <gl/GL.h>
#include "utils.h"
#define CASE_NUM 0
#define USE_CUSTOM_COMPOSITOR 0

#if USE_CUSTOM_COMPOSITOR 
#include "CPUDataCompositor.h"
#endif

SoSeparator *ptRoot;				// pointer to root of scene graph.
SoVolumeData *volData;				// pointer to volume data of current test case
SoVolumeClippingGroup *ptCSGSRt;	// pointer to root of CSGTree(CSGShapeRoot)
SoDialogRadioButtons *CSGOp;		// pointer to current CSG operation
SoSwitch *ptClipShape;				// pointer to switch for clipping
SoROIManip *ptROI;					// pointer to roi box

class ClearButtonAuditor : public SoDialogPushButtonAuditor
{
	virtual void dialogPushButton(SoDialogPushButton*)
	{
		if (ptClipShape->whichChild.getValue() == SO_SWITCH_ALL)
		{
			ptCSGSRt->removeAllChildren();
			ptClipShape->whichChild = SO_SWITCH_NONE;
		}
	}
};

class RadioButtonAuditor : public SoDialogChoiceAuditor
{
	virtual void dialogChoice(SoDialogChoice *cpt)
	{
		if (cpt->selectedItem.getValue() == 0)
			ptClipShape->whichChild = SO_SWITCH_NONE;
		else if (find<SoCSGShape>(ptCSGSRt, SbString(), true))
			ptClipShape->whichChild = SO_SWITCH_ALL;
	}
};
 
class CheckBoxAuditor : public SoDialogCheckBoxAuditor
{
	virtual void dialogCheckBox(SoDialogCheckBox *cpt)
	{
		if (ptClipShape->whichChild.getValue() == SO_SWITCH_NONE)
		{
			if (cpt->state.getValue() == TRUE)
				ptROI->flags = SoROI::SUB_VOLUME;
			else ptROI->flags = SoROI::EXCLUSION_BOX;
		}
	}
};

void lineFinalizedCallback(SoPolyLineScreenDrawer::EventArg& eventArg);

int main(int argc, char *argv[])
{
	//------------
	// initialize
	//------------

	Widget myWindow = SoXt::init(argv[0]);
	if (!myWindow) return 0;
	SoCuda::init();
	SoVolumeRendering::init();
	SoDialogViz::init();
	SoCSGShape::setRescueOperation(SoCSGShape::LEFT_ONLY);
	SoPreferences::setValue("OIV_BUFFER_REGION_ENABLE", "0"); // for line drawer
	
	//-------------
	// declaration
	//-------------

	SoSeparator *Root = new SoSeparator;
	SoSeparator* VolumeViz = new SoSeparator;
	SoMaterial* volMatrl = new SoMaterial;
	SoDataRange* dataRng = new SoDataRange;
	SoMultiDataSeparator *mulDataSep = new SoMultiDataSeparator;
	SoVolumeGroup *gVol = new SoVolumeGroup;
	SoDataCompositor *volComp = new SoDataCompositor;
	SoVolumeShader *vshade = new SoVolumeShader;
	SoVolumeClippingGroup *CSGSRt = new SoVolumeClippingGroup;
	SoVolumeRenderingQuality *volQual = new SoVolumeRenderingQuality;
	SoVolumeRenderingPhysicalQuality *phyQual = new SoVolumeRenderingPhysicalQuality;
	SoVolumeRender *volRend = new SoVolumeRender;
	SoGradientBackground *Background = new SoGradientBackground();
	SoSwitch *sClip = new SoSwitch;
	SoSwitch *sClipShape = new SoSwitch;
	SoROIManip *mROI = new SoROIManip;

	SoSeparator* pDrawer = new SoSeparator;
	SoSwitch* lineDrawerSwitch = new SoSwitch;
	SoLassoScreenDrawer* lasso = new SoLassoScreenDrawer;
	SoPolygonScreenDrawer* polygon = new SoPolygonScreenDrawer;
	SoEllipseScreenDrawer* ellipseCorner = new SoEllipseScreenDrawer;
	SoRectangleScreenDrawer* rectangleCorner = new SoRectangleScreenDrawer;

	//--------------
	// scene graph
	//--------------
	
	sClipShape->addChild(CSGSRt);
	ptClipShape = sClipShape; // pointing

	sClip->addChild(mROI);
	sClip->addChild(pDrawer);

	pDrawer->addChild(lineDrawerSwitch);
	lineDrawerSwitch->addChild(lasso);
	lineDrawerSwitch->addChild(polygon);
	lineDrawerSwitch->addChild(ellipseCorner);
	lineDrawerSwitch->addChild(rectangleCorner);

#if USE_CUSTOM_COMPOSITOR
	CPUDataCompositor *composer = new CPUDataCompositor;
	//composer->useCudaIfAvailable.setValue(FALSE);
	// Enable this to experiment generation of RGBA dataset on the fly. 
	composer->rgbaMode.setValue(TRUE);
	mulDataSep->addChild(composer);
#else
	mulDataSep->addChild(volComp);
#endif
	
	VolumeViz->addChild(mulDataSep);
	Root->addChild(Background);
	Root->addChild(VolumeViz);
	ptRoot = Root; // pointing

	//----------------------
	// process volume data
	//----------------------

	int len = 50;
	char *fname;
	float map[8][3] = { { 1.0, 0.0, 0.0 }, { 1.0, 0.5, 0.0 }, { 1.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 },
						{ 0.0, 1.0, 1.0 }, { 0.0, 0.5, 1.0 }, { 0.0, 0.0, 1.0 }, { 0.5, 0.5, 0.5 } };
	for (int pic = 0; pic < 6; pic++)
	{
		if (pic == 3) continue; // blank picture
		auto &m = map[pic];
		SoSeparator *volSep = new SoSeparator;
		SoVRRasterStackReader *SR = new SoVRRasterStackReader;
		SbStringList *list = new SbStringList;
		SoVolumeData *sample = new SoVolumeData;
		SoTransferFunction *transFunc = new SoTransferFunction;

		// read data
		for (int i = 1; i < 289; i++)
		{
			fname = new char[len];
			sprintf_s(fname, len, "D:/hcchang0701/multiVol_tif/1_Mask%d_%.4d.tif", pic+1, i);
			list->append(fname);
		}
		SR->setFilenameList(*list);
		sample->setReader(*SR);
		sample->dataSetId = pic+1;

		// transfer function
		int pvals = 256, thr = 100;
		SbColorRGBA* colors = new SbColorRGBA[pvals];
		for (int i = 0; i < pvals; ++i)
		{
			float itns = i / (float)pvals;
			colors[i].setValue(itns*m[0], itns*m[1], itns*m[2], (i > thr));// ? (0.5*(i - thr) / (255 - thr)) : 0);
		}
		transFunc->colorMap.setValues(0, 4*pvals, (float*)colors);
		transFunc->predefColorMap = SoTransferFunction::NONE;
		/*
		volSep->addChild(volQual);
		volSep->addChild(volMatrl);
		volSep->addChild(transFunc);
		volSep->addChild(sample);
		volSep->addChild(volRend);
		gVol->addChild(volSep);
		*/
		mulDataSep->addChild(sample);
		mulDataSep->addChild(transFunc);
		// volData = sample;
	}
	mulDataSep->addChild(volQual);
	mulDataSep->addChild(volMatrl);
	mulDataSep->addChild(volRend);

	// data compositor
	volComp->preDefCompositor = SoDataCompositor::ADD;
	volComp->rgbaMode = TRUE;

	// material
	volMatrl->diffuseColor.setValue(1, 1, 1);
	volMatrl->specularColor.setValue(1, 1, 1);
	volMatrl->shininess.setValue(0.5);

	// rendering quality
	volQual->preIntegrated = TRUE;
	volQual->ambientOcclusion = TRUE;
	volQual->edgeColoring = TRUE;
	volQual->lighting = TRUE;
	volQual->gradientThreshold = 0.1;
	volQual->surfaceScalarExponent = 2;
	volQual->lightingModel = SoVolumeRenderingQuality::OIV6;

	// volume rendering
	volRend->samplingAlignment = SoVolumeRender::BOUNDARY_ALIGNED;

	// background
	Background->color0.setValue(0, 1, 0);
	Background->color1.setValue(0, 0, 1);

	// region of interest
	// mROI->box.setValue(SbVec3i32(0, 0, 0), volData->data.getSize() - SbVec3i32(1, 1, 1));
	ptROI = mROI;

	// csg shape root
	CSGSRt->ref();
	CSGSRt->numPasses = 64;
	CSGSRt->setNotEnoughPassCallback(notEnoughPassCB, NULL);
	ptCSGSRt = CSGSRt;
	std::cout << CSGSRt->getMaxNumPasses() << std::endl;

	//---------------------
	// create line drawers
	//---------------------

	// line drawer
	pDrawer->fastEditing = SoSeparator::CLEAR_ZBUFFER;
	pDrawer->boundingBoxIgnoring = TRUE;
	lineDrawerSwitch->whichChild = 0;

	// lasso
	lasso->color.setValue(1, 0, 0);
	lasso->onFinish.add(lineFinalizedCallback);
	lasso->isClosed = TRUE;

	// polygon
	polygon->color.setValue(1, 0, 0);
	polygon->onFinish.add(lineFinalizedCallback);

	// ellipse corner
	ellipseCorner->color.setValue(1, 0, 0);
	ellipseCorner->method = SoEllipseScreenDrawer::CORNER_CORNER;
	ellipseCorner->onFinish.add(lineFinalizedCallback);

	// Rectangle corner
	rectangleCorner->color.setValue(1, 1, 1);
	rectangleCorner->method = SoRectangleScreenDrawer::CORNER_CORNER;
	rectangleCorner->onFinish.add(lineFinalizedCallback);

	//---------------------------------
	// dialog (apart from scene graph)
	//---------------------------------

	// dialog
	SoTopLevelDialog* myTop = new SoTopLevelDialog;
	SoRowDialog* leftRD = new SoRowDialog;
	SoRowDialog* rightRD = new SoRowDialog;
	SoColumnDialog* colDialog = new SoColumnDialog;
	SoDialogCustom* myCustom = new SoDialogCustom;

	// boxes & buttons
	SoDialogRadioButtons *clipMode = new SoDialogRadioButtons;
	SoDialogRadioButtons *dCSGRadio = new SoDialogRadioButtons;
	SoDialogComboBox* modeRadio = new SoDialogComboBox;
	SoDialogCheckBox *clipIO = new SoDialogCheckBox;
	SoDialogPushButton* clearButton = new SoDialogPushButton;

	// auditor
	RadioButtonAuditor *rbAuditor = new RadioButtonAuditor;
	CheckBoxAuditor *cbAuditor = new CheckBoxAuditor;
	ClearButtonAuditor* clearButtonAuditor = new ClearButtonAuditor();
	
	// customization of dialog
	myCustom->width = 800;
	myCustom->height = 600;

	// radio button for clipping mode
	clipMode->label = "Clipping Mode";
	clipMode->addItem("ROI");
	clipMode->addItem("Line Drawers");
	clipMode->addAuditor(rbAuditor);
	sClip->whichChild.connectFrom(&clipMode->selectedItem);

	// combo box to select lineDrawer
	modeRadio->addItem("Lasso");
	modeRadio->addItem("Polygon");
	modeRadio->addItem("Ellipse");
	modeRadio->addItem("Rectangle");
	modeRadio->enable.connectFrom(&clipMode->selectedItem);
	lineDrawerSwitch->whichChild.connectFrom(&modeRadio->selectedItem);

	// combo box to select csgOperation
	dCSGRadio->label = "CSG Operation";
	dCSGRadio->addItem("Intersection");
	dCSGRadio->addItem("Union");
	CSGOp = dCSGRadio;
	
	// checkBox to clip inside/outside of csg group 
	clipIO->onString = "Draw inside";
	clipIO->offString = "Draw outside";
	clipIO->addAuditor(cbAuditor);
	CSGSRt->clipOutside.connectFrom(&clipIO->state);

	// clear button
	clearButton->buttonLabel = "Clear";
	clearButton->addAuditor(clearButtonAuditor);

	// left row
	leftRD->addChild(clipMode);
	leftRD->addChild(modeRadio);

	// right row
	rightRD->addChild(clipIO);
	rightRD->addChild(dCSGRadio);
	rightRD->addChild(clearButton);

	// column dialog
	colDialog->addChild(leftRD);
	colDialog->addChild(rightRD);
	colDialog->height = 6;
	colDialog->fixedHeight = TRUE;

	// top level
	myTop->label = "Brain Visualization";
	myTop->addChild(myCustom);
	myTop->addChild(colDialog);
	myTop->buildDialog(myWindow, TRUE);
	myTop->show();

	//-----------------------------------
	// display (apart from scene graph)
	//-----------------------------------

	SoXtExaminerViewer *myViewer = new SoXtExaminerViewer(myCustom->getWidget());
	myViewer->viewAll();
	myViewer->setSceneGraph(Root);
	myViewer->show();

	SoXt::show(myWindow);
	SoXt::mainLoop();

	//--------------
	// termination
	//--------------

	CSGSRt->unref();
	SoVolumeRendering::finish();
	SoDialogViz::finish();
	SoCuda::finish();
	SoXt::finish();

	return 0;
}

void lineFinalizedCallback(SoPolyLineScreenDrawer::EventArg& eventArg)
{
	SoPolyLineScreenDrawer* lineDrawer = eventArg.getSource();
	SoHandleEventAction* action = eventArg.getAction();

	// If less than 1 point, shape cannot be generated.
	if (lineDrawer->point.getNum() < 1)
	{
		lineDrawer->clear();
		return;
	}

	// retrieve points of line in cam space
	std::vector<SbVec2f> lineInCam(lineDrawer->point.getNum());
	for (unsigned int i = 0; i < lineInCam.size(); ++i)
		lineInCam[i].setValue(lineDrawer->point[i][0], lineDrawer->point[i][1]);

	//------------------------------------------------------------------------------
	// create a new extruded shape :

	// retrieve path of extruded shape root
	SoSearchAction searchAction;
	searchAction.setNode(ptCSGSRt);
	searchAction.apply(ptRoot);
	SoPath* pathToExtrudedShapeRoot = searchAction.getPath();

	// retrieve bbox of volumeData node.
	SbBox3f bbox = getBbox(volData);

	// create an extruded shape from specified line. Line is extruded along view 
	// direction between bounding box enclosing planes.
	SoShape* extrudedShape = SbExtrusionGenerator::createFrom2DPoints(lineInCam,
		pathToExtrudedShapeRoot,
		SoViewVolumeElement::get(action->getState()),
		bbox);
	if (extrudedShape == NULL)
	{
		lineDrawer->clear();
		return;
	}

	// Clip extrudedShape with volumeData bbox.
	SoSeparator* bboxSep = createCube(bbox);
	SoCSGShape* extrudedShapeClipped = new SoCSGShape;
	extrudedShapeClipped->leftOperand = extrudedShape;
	extrudedShapeClipped->rightOperand = bboxSep;
	extrudedShapeClipped->csgOperation = SoCSGShape::INTERSECTION;


	// if it's the first time a CSGShape is added to CSGSRt,
	// Initialize CSGTree with the box representing volumeData.
	if (find<SoCSGShape>(ptCSGSRt, SbString(), true) == NULL)
	{
		SoCSGShape* initialBox = new SoCSGShape;
		initialBox->rightOperand = bboxSep;
		initialBox->csgOperation = SoCSGShape::RIGHT_ONLY;
		ptCSGSRt->addChild(initialBox);
		ptClipShape->whichChild = SO_SWITCH_ALL;
	}

	// Add this clippedShape to current CSGTree
	switch (CSGOp->selectedItem.getValue())
	{
	case 0:
		addShape(ptCSGSRt, extrudedShapeClipped, SoCSGShape::INTERSECTION);
		break;
	case 1:
		addShape(ptCSGSRt, extrudedShapeClipped, SoCSGShape::ADD);
		break;
	}

	lineDrawer->clear();
	action->setHandled();
}
