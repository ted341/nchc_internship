#include <Inventor/SbColorRGBA.h>
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

#include <VolumeViz/nodes/SoVolumeData.h>
#include <VolumeViz/nodes/SoVolumeRender.h>
#include <VolumeViz/nodes/SoVolumeRendering.h>
#include <VolumeViz/nodes/SoVolumeClippingGroup.h>
#include <VolumeViz/nodes/SoVolumeRenderingQuality.h>
#include <VolumeViz/nodes/SoVolumeRenderingPhysicalQuality.h>
#include <VolumeViz/readers/SoVRRasterStackReader.h>

#include <LDM/nodes/SoDataRange.h>
#include <LDM/nodes/SoTransferFunction.h>
#include <LDM/manips/SoROIManip.h>
#include <DialogViz/SoDialogVizAll.h>
#include <gl/GL.h>
#include "utils.h"
#define CASE_NUM 0

// root of scene graph.
SoSeparator *Root;

// pointer to volume data of current test case
SoVolumeData *volData;

// root of CSGTree(CSGShapeRoot)
SoVolumeClippingGroup *pCSGSRt;
SoSwitch *sClipShape;
SoROIManip *mROI;

// pointer to current CSG operation
SoDialogRadioButtons* dCSGRadio;

class ClearButtonAuditor : public SoDialogPushButtonAuditor
{
	virtual void dialogPushButton(SoDialogPushButton*)
	{
		if (sClipShape->whichChild.getValue() == SO_SWITCH_ALL)
		{
			pCSGSRt->removeAllChildren();
			sClipShape->whichChild = SO_SWITCH_NONE;
		}
	}
};

class RadioButtonAuditor : public SoDialogChoiceAuditor
{
	virtual void dialogChoice(SoDialogChoice *cpt)
	{
		if (cpt->selectedItem.getValue() == 0)
			sClipShape->whichChild = SO_SWITCH_NONE;
		else if (find<SoCSGShape>(pCSGSRt, SbString(), true))
			sClipShape->whichChild = SO_SWITCH_ALL;
	}
};

class CheckBoxAuditor : public SoDialogCheckBoxAuditor
{
	virtual void dialogCheckBox(SoDialogCheckBox *cpt)
	{
		if (sClipShape->whichChild.getValue() == SO_SWITCH_NONE)
		{
			if (cpt->state.getValue() == TRUE)
				mROI->flags = SoROI::SUB_VOLUME;
			else mROI->flags = SoROI::EXCLUSION_BOX;
		}
	}
};

void lineFinalizedCallback(SoPolyLineScreenDrawer::EventArg& eventArg);

int main(int argc, char *argv[])
{
	// initialize

	Widget myWindow = SoXt::init(argv[0]);
	if (!myWindow) return 0;
	SoVolumeRendering::init();
	SoDialogViz::init();
	SoCSGShape::setRescueOperation(SoCSGShape::LEFT_ONLY);
	SoPreferences::setValue("OIV_BUFFER_REGION_ENABLE", "0");
	Root = new SoSeparator;

	//--------------------------------------------------------
	// process volume data

	int num = 50;
	char *fname;
	SoVRRasterStackReader *SR = new SoVRRasterStackReader;
	SbStringList *list = new SbStringList;

	switch (CASE_NUM)
	{
	case 0:
		for (int i = 1; i < 170; i++)
		{
			fname = new char[num];
			sprintf_s(fname, num, "D:/hcchang0701/brain/brain/_Mask3_%.3d.tif", i);
			list->append(fname);
		}
		break;
	case 1:
		for (int i = 1; i < 132; i++)
		{
			fname = new char[num];
			sprintf_s(fname, num, "D:/hcchang0701/brain/tif/_Mask1_%.3d.tif", i);
			list->append(fname);
		}
		break;
	case 2:
		for (int i = 1; i < 289; i++)
		{
			fname = new char[num];
			sprintf_s(fname, num, "D:/hcchang0701/body2/body_Mask1_%.3d.tif", i);
			list->append(fname);
		}
		break;
	default: 
		break;
	}
	
	SR->setFilenameList(*list);
	//SR1->setDirectory("D:/hcchang0701/brain/brain/");
	volData = new SoVolumeData;
	volData->setReader(*SR);

	//double minval, maxval;
	//volData->getMinMax(minval, maxval);

	SoMaterial* volMatrl = new SoMaterial;
	volMatrl->diffuseColor.setValue(1, 0.4, 0.4);
	volMatrl->specularColor.setValue(1, 1, 1);
	volMatrl->shininess.setValue(0.8);

	int numColors = 256, threshold;
	SoTransferFunction* transFunc = new SoTransferFunction;
	SbColorRGBA* colors = new SbColorRGBA[numColors];
	for (int i = 0; i < numColors; ++i) 
	{
		float intensity = i / (float)numColors;
		switch (CASE_NUM)
		{
		case 0: threshold = 100; break;
		case 1: threshold = 128; break;
		default: threshold = 90; break;
		}
		colors[i].setValue(intensity * 1, intensity*0.4, intensity*0.4, i > threshold);
	}
	transFunc->colorMap.setValues(0, 4*numColors, (float*)colors);
	transFunc->predefColorMap = SoTransferFunction::NONE;
	//transFunc->maxValue = maxval;
	
	SoDataRange* dataRng = new SoDataRange;

	SoVolumeRenderingQuality* VolQual = new SoVolumeRenderingQuality;
	VolQual->preIntegrated = TRUE;
	VolQual->ambientOcclusion = TRUE;
	VolQual->edgeColoring = TRUE;
	VolQual->lighting = TRUE;
	VolQual->gradientThreshold = 0.1;
	VolQual->surfaceScalarExponent = 2;
	VolQual->lightingModel = SoVolumeRenderingQuality::OIV6;

	SoVolumeRenderingPhysicalQuality *phyQual = new SoVolumeRenderingPhysicalQuality;

	SoVolumeRender *volRend = new SoVolumeRender;
	volRend->samplingAlignment = SoVolumeRender::BOUNDARY_ALIGNED;

	SoGradientBackground *gradBG = new SoGradientBackground();
	gradBG->color0.setValue(0, 1, 0);
	gradBG->color1.setValue(0, 0, 1);

	mROI = new SoROIManip;
	mROI->box.setValue(SbVec3i32(0, 0, 0), volData->data.getSize() - SbVec3i32(1, 1, 1));

	SoSwitch *sClip = new SoSwitch;
	sClip->addChild(mROI);

	//------------------------------------------------------------------------------
	// Add line drawers

	pCSGSRt = new SoVolumeClippingGroup;
	pCSGSRt->numPasses = 64;
	pCSGSRt->setNotEnoughPassCallback(notEnoughPassCB, NULL);
	pCSGSRt->ref();
	std::cout << pCSGSRt->getMaxNumPasses() << std::endl;

	SoSeparator* pDrawer = new SoSeparator;
	pDrawer->fastEditing = SoSeparator::CLEAR_ZBUFFER;
	pDrawer->boundingBoxIgnoring = TRUE;

	SoSwitch* lineDrawerSwitch = new SoSwitch;
	lineDrawerSwitch->whichChild = 0;
	pDrawer->addChild(lineDrawerSwitch);
	sClip->addChild(pDrawer);

	// lasso
	SoLassoScreenDrawer* lasso = new SoLassoScreenDrawer();
	lasso->color.setValue(1, 0, 0);
	lasso->onFinish.add(lineFinalizedCallback);
	lasso->isClosed = TRUE;
	lineDrawerSwitch->addChild(lasso);

	// polygon
	SoPolygonScreenDrawer* polygon = new SoPolygonScreenDrawer;
	polygon->color.setValue(1, 0, 0);
	polygon->onFinish.add(lineFinalizedCallback);
	lineDrawerSwitch->addChild(polygon);

	// ellipse corner
	SoEllipseScreenDrawer* ellipseCorner = new SoEllipseScreenDrawer;
	ellipseCorner->color.setValue(1, 0, 0);
	ellipseCorner->method = SoEllipseScreenDrawer::CORNER_CORNER;
	ellipseCorner->onFinish.add(lineFinalizedCallback);
	lineDrawerSwitch->addChild(ellipseCorner);

	// Rectangle corner
	SoRectangleScreenDrawer* rectangleCorner = new SoRectangleScreenDrawer;
	rectangleCorner->color.setValue(1, 1, 1);
	rectangleCorner->method = SoRectangleScreenDrawer::CORNER_CORNER;
	rectangleCorner->onFinish.add(lineFinalizedCallback);
	lineDrawerSwitch->addChild(rectangleCorner);
	
	//---------------------------------------------------------------------
	// Build dialog

	SoDialogCustom* myCustom = new SoDialogCustom;
	myCustom->width = 800;
	myCustom->height = 600;

	SoDialogRadioButtons *clipMode = new SoDialogRadioButtons;
	RadioButtonAuditor *rbAuditor = new RadioButtonAuditor;
	clipMode->label = "Clipping Mode";
	clipMode->addItem("ROI");
	clipMode->addItem("Line Drawers");
	clipMode->addAuditor(rbAuditor);
	sClip->whichChild.connectFrom(&clipMode->selectedItem);

	// combo box to select lineDrawer
	SoDialogComboBox* modeRadio = new SoDialogComboBox;
	modeRadio->addItem("Lasso");
	modeRadio->addItem("Polygon");
	modeRadio->addItem("Ellipse");
	modeRadio->addItem("Rectangle");
	modeRadio->enable.connectFrom(&clipMode->selectedItem);
	lineDrawerSwitch->whichChild.connectFrom(&modeRadio->selectedItem);

	// combo box to select csgOperation
	dCSGRadio = new SoDialogRadioButtons;
	dCSGRadio->label = "CSG Operation";
	dCSGRadio->addItem("Intersection");
	dCSGRadio->addItem("Union");

	// CheckBox to clip inside/outside of csg group 
	SoDialogCheckBox *clipIO = new SoDialogCheckBox;
	CheckBoxAuditor *cbAuditor = new CheckBoxAuditor;
	clipIO->onString = "Draw inside";
	clipIO->offString = "Draw outside";
	clipIO->addAuditor(cbAuditor);
	pCSGSRt->clipOutside.connectFrom(&clipIO->state);

	// Clear button
	SoDialogPushButton* clearButton = new SoDialogPushButton;
	ClearButtonAuditor* clearButtonAuditor = new ClearButtonAuditor();
	clearButton->buttonLabel = "Clear";
	clearButton->addAuditor(clearButtonAuditor);

	SoRowDialog* leftRD = new SoRowDialog;
	leftRD->addChild(clipMode);
	leftRD->addChild(modeRadio);

	SoRowDialog* rightRD = new SoRowDialog;
	rightRD->addChild(clipIO);
	rightRD->addChild(dCSGRadio);
	rightRD->addChild(clearButton);

	SoColumnDialog* colDialog = new SoColumnDialog;
	colDialog->addChild(leftRD);
	colDialog->addChild(rightRD);
	colDialog->height = 6;
	colDialog->fixedHeight = TRUE;

	SoTopLevelDialog* myTop = new SoTopLevelDialog;
	myTop->label = "Brain Visualization";
	myTop->addChild(myCustom);
	myTop->addChild(colDialog);

	//--------------------------------------------------------------------------
	// construct scene graph

	sClipShape = new SoSwitch;
	sClipShape->addChild(pCSGSRt);

	SoSeparator* pVolume = new SoSeparator;
	pVolume->addChild(volData);
	pVolume->addChild(volMatrl);
	pVolume->addChild(transFunc);
	pVolume->addChild(dataRng);
	//pVolume->addChild(VolQual);
	pVolume->addChild(sClip);
	pVolume->addChild(sClipShape);
	pVolume->addChild(phyQual);
	pVolume->addChild(volRend);

	Root->addChild(gradBG);
	Root->addChild(pVolume);
	
	//----------------------------------------------------------------
	// display

	myTop->buildDialog(myWindow, TRUE);
	myTop->show();
	
	SoXtExaminerViewer *myViewer = new SoXtExaminerViewer(myCustom->getWidget());
	myViewer->viewAll();
	myViewer->setSceneGraph(Root);
	myViewer->show();

	SoXt::show(myWindow);
	SoXt::mainLoop();

	//-----------------------------------------------------------------
	// termination

	pCSGSRt->unref();
	delete clearButtonAuditor;
	delete myViewer;
	SoVolumeRendering::finish();
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
	searchAction.setNode(pCSGSRt);
	searchAction.apply(Root);
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


	// if it's the first time a CSGShape is added to pCSGSRt,
	// Initialize CSGTree with the box representing volumeData.
	if (find<SoCSGShape>(pCSGSRt, SbString(), true) == NULL)
	{
		SoCSGShape* initialBox = new SoCSGShape;
		initialBox->rightOperand = bboxSep;
		initialBox->csgOperation = SoCSGShape::RIGHT_ONLY;
		pCSGSRt->addChild(initialBox);
		sClipShape->whichChild = SO_SWITCH_ALL;
	}

	// Add this clippedShape to current CSGTree
	switch (dCSGRadio->selectedItem.getValue())
	{
	case 0:
		addShape(pCSGSRt, extrudedShapeClipped, SoCSGShape::INTERSECTION);
		break;
	case 1:
		addShape(pCSGSRt, extrudedShapeClipped, SoCSGShape::ADD);
		break;
	}

	lineDrawer->clear();
	action->setHandled();
}
