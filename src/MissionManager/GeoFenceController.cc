/****************************************************************************
 *
 *   (c) 2009-2016 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


/// @file
///     @author Don Gagne <don@thegagnes.com>

#include "GeoFenceController.h"
#include "Vehicle.h"
#include "FirmwarePlugin.h"
#include "MAVLinkProtocol.h"
#include "QGCApplication.h"
#include "ParameterManager.h"
#include "JsonHelper.h"
#include "QGCQGeoCoordinate.h"
#include "AppSettings.h"
#include "PlanMasterController.h"

#ifndef __mobile__
#include "MainWindow.h"
#include "QGCQFileDialog.h"
#endif

#include <QJsonDocument>
#include <QJsonArray>

QGC_LOGGING_CATEGORY(GeoFenceControllerLog, "GeoFenceControllerLog")

const char* GeoFenceController::_jsonFileTypeValue =    "GeoFence";
const char* GeoFenceController::_jsonBreachReturnKey =  "breachReturn";

GeoFenceController::GeoFenceController(PlanMasterController* masterController, QObject* parent)
    : PlanElementController(masterController, parent)
    , _geoFenceManager(_managerVehicle->geoFenceManager())
    , _dirty(false)
    , _itemsRequested(false)
{
    connect(&_inclusionPolygons, &QmlObjectListModel::countChanged, this, &GeoFenceController::_updateContainsItems);
    connect(&_exclusionPolygons, &QmlObjectListModel::countChanged, this, &GeoFenceController::_updateContainsItems);

    managerVehicleChanged(_managerVehicle);
}

GeoFenceController::~GeoFenceController()
{

}

void GeoFenceController::start(bool editMode)
{
    qCDebug(GeoFenceControllerLog) << "start editMode" << editMode;

    PlanElementController::start(editMode);
    _init();
}

void GeoFenceController::_init(void)
{

}

void GeoFenceController::setBreachReturnPoint(const QGeoCoordinate& breachReturnPoint)
{
    if (_breachReturnPoint != breachReturnPoint) {
        _breachReturnPoint = breachReturnPoint;
        setDirty(true);
        emit breachReturnPointChanged(breachReturnPoint);
    }
}

void GeoFenceController::_signalAll(void)
{
    emit breachReturnSupportedChanged(breachReturnSupported());
    emit breachReturnPointChanged(breachReturnPoint());
    emit circleEnabledChanged(circleEnabled());
    emit circleRadiusFactChanged(circleRadiusFact());
    emit polygonEnabledChanged(polygonEnabled());
    emit polygonSupportedChanged(polygonSupported());
    emit dirtyChanged(dirty());
}

void GeoFenceController::managerVehicleChanged(Vehicle* managerVehicle)
{
    if (_managerVehicle) {
        _geoFenceManager->disconnect(this);
        _managerVehicle = NULL;
        _geoFenceManager = NULL;
    }

    _managerVehicle = managerVehicle;
    if (!_managerVehicle) {
        qWarning() << "GeoFenceController::managerVehicleChanged managerVehicle=NULL";
        return;
    }

    _geoFenceManager = _managerVehicle->geoFenceManager();
    connect(_geoFenceManager, &GeoFenceManager::breachReturnSupportedChanged,   this, &GeoFenceController::breachReturnSupportedChanged);
    connect(_geoFenceManager, &GeoFenceManager::circleEnabledChanged,           this, &GeoFenceController::circleEnabledChanged);
    connect(_geoFenceManager, &GeoFenceManager::circleRadiusFactChanged,        this, &GeoFenceController::circleRadiusFactChanged);
    connect(_geoFenceManager, &GeoFenceManager::polygonEnabledChanged,          this, &GeoFenceController::polygonEnabledChanged);
    connect(_geoFenceManager, &GeoFenceManager::polygonSupportedChanged,        this, &GeoFenceController::polygonSupportedChanged);
    connect(_geoFenceManager, &GeoFenceManager::loadComplete,                   this, &GeoFenceController::_managerLoadComplete);
    connect(_geoFenceManager, &GeoFenceManager::sendComplete,                   this, &GeoFenceController::_managerSendComplete);
    connect(_geoFenceManager, &GeoFenceManager::removeAllComplete,              this, &GeoFenceController::_managerRemoveAllComplete);
    connect(_geoFenceManager, &GeoFenceManager::inProgressChanged,              this, &GeoFenceController::syncInProgressChanged);

    _signalAll();
}

bool GeoFenceController::load(const QJsonObject& json, QString& errorString)
{
    Q_UNUSED(json);
    Q_UNUSED(errorString);

#if 0
    QString errorStr;
    QString errorMessage = tr("GeoFence: %1");

    if (json.contains(_jsonBreachReturnKey) &&
            !JsonHelper::loadGeoCoordinate(json[_jsonBreachReturnKey], false /* altitudeRequired */, _breachReturnPoint, errorStr)) {
        errorString = errorMessage.arg(errorStr);
        return false;
    }

    if (!_mapPolygon.loadFromJson(json, true, errorStr)) {
        errorString = errorMessage.arg(errorStr);
        return false;
    }
    _mapPolygon.setDirty(false);
    setDirty(false);

    _signalAll();
#endif

    return true;
}

void  GeoFenceController::save(QJsonObject& json)
{
    Q_UNUSED(json);
#if 0
    json[JsonHelper::jsonVersionKey] = 1;

    if (_breachReturnPoint.isValid()) {
        QJsonValue jsonBreachReturn;
        JsonHelper::saveGeoCoordinate(_breachReturnPoint, false /* writeAltitude */, jsonBreachReturn);
        json[_jsonBreachReturnKey] = jsonBreachReturn;
    }

    _mapPolygon.saveToJson(json);
#endif
}

void GeoFenceController::removeAll(void)
{    
    setBreachReturnPoint(QGeoCoordinate());
    _inclusionPolygons.clear();
    _exclusionPolygons.clear();
}

void GeoFenceController::removeAllFromVehicle(void)
{
    if (_masterController->offline()) {
        qCWarning(GeoFenceControllerLog) << "GeoFenceController::removeAllFromVehicle called while offline";
    } else if (syncInProgress()) {
        qCWarning(GeoFenceControllerLog) << "GeoFenceController::removeAllFromVehicle called while syncInProgress";
    } else {
        _geoFenceManager->removeAll();
    }
}

void GeoFenceController::loadFromVehicle(void)
{
    if (_masterController->offline()) {
        qCWarning(GeoFenceControllerLog) << "GeoFenceController::loadFromVehicle called while offline";
    } else if (syncInProgress()) {
        qCWarning(GeoFenceControllerLog) << "GeoFenceController::loadFromVehicle called while syncInProgress";
    } else {
        _itemsRequested = true;
        _geoFenceManager->loadFromVehicle();
    }
}

void GeoFenceController::sendToVehicle(void)
{
    if (_masterController->offline()) {
        qCWarning(GeoFenceControllerLog) << "GeoFenceController::sendToVehicle called while offline";
    } else if (syncInProgress()) {
        qCWarning(GeoFenceControllerLog) << "GeoFenceController::sendToVehicle called while syncInProgress";
    } else {
        qCDebug(GeoFenceControllerLog) << "GeoFenceController::sendToVehicle";
        _geoFenceManager->sendToVehicle(_breachReturnPoint, _inclusionPolygons, _exclusionPolygons);
        setDirty(false);
    }
}

bool GeoFenceController::syncInProgress(void) const
{
    return _geoFenceManager->inProgress();
}

bool GeoFenceController::dirty(void) const
{
    return _dirty;
}


void GeoFenceController::setDirty(bool dirty)
{
    if (dirty != _dirty) {
        _dirty = dirty;
        if (!dirty) {
            for (int i=0; i<_inclusionPolygons.count(); i++) {
                QGCMapPolygon* polygon = _inclusionPolygons.value<QGCMapPolygon*>(i);
                polygon->setDirty(false);
            }
            for (int i=0; i<_exclusionPolygons.count(); i++) {
                QGCMapPolygon* polygon = _exclusionPolygons.value<QGCMapPolygon*>(i);
                polygon->setDirty(false);
            }
        }
        emit dirtyChanged(dirty);
    }
}

void GeoFenceController::_polygonDirtyChanged(bool dirty)
{
    if (dirty) {
        setDirty(true);
    }
}

bool GeoFenceController::breachReturnSupported(void) const
{
    return _geoFenceManager->breachReturnSupported();
}

bool GeoFenceController::circleEnabled(void) const
{
    return _geoFenceManager->circleEnabled();
}

Fact* GeoFenceController::circleRadiusFact(void) const
{
    return _geoFenceManager->circleRadiusFact();
}

bool GeoFenceController::polygonSupported(void) const
{
    return _geoFenceManager->polygonSupported();
}

bool GeoFenceController::polygonEnabled(void) const
{
    return _geoFenceManager->polygonEnabled();
}

QVariantList GeoFenceController::params(void) const
{
    return _geoFenceManager->params();
}

QStringList GeoFenceController::paramLabels(void) const
{
    return _geoFenceManager->paramLabels();
}

void GeoFenceController::_setDirty(void)
{
    setDirty(true);
}

void GeoFenceController::_setPolygonsFromManager(const QList<QList<QGeoCoordinate>>& inclusionPolygons, const QList<QList<QGeoCoordinate>>& exclusionPolygons)
{
    _inclusionPolygons.clearAndDeleteContents();
    _exclusionPolygons.clearAndDeleteContents();

    for (int i=0; i<inclusionPolygons.count(); i++) {
        QGCMapPolygon*                  mapPolygon = new QGCMapPolygon(this);
        const QList<QGeoCoordinate>&    polygon = inclusionPolygons[i];

        for (int j=0; j<polygon.count(); j++) {
            mapPolygon->appendVertex(polygon[j]);
        }
        _inclusionPolygons.append(mapPolygon);
    }

    for (int i=0; i<exclusionPolygons.count(); i++) {
        QGCMapPolygon*                  mapPolygon = new QGCMapPolygon(this);
        const QList<QGeoCoordinate>&    polygon = exclusionPolygons[i];

        for (int j=0; j<polygon.count(); j++) {
            mapPolygon->appendVertex(polygon[j]);
        }
        _exclusionPolygons.append(mapPolygon);
    }

    setDirty(false);
}

void GeoFenceController::_setReturnPointFromManager(QGeoCoordinate breachReturnPoint)
{
    _breachReturnPoint = breachReturnPoint;
    emit breachReturnPointChanged(_breachReturnPoint);
}

void GeoFenceController::_managerLoadComplete(void)
{
    // Fly view always reloads on _loadComplete
    // Plan view only reloads on _loadComplete if specifically requested
    if (!_editMode || _itemsRequested) {
        _setReturnPointFromManager(_geoFenceManager->breachReturnPoint());
        _setPolygonsFromManager(_geoFenceManager->inclusionPolygons(), _geoFenceManager->exclusionPolygons());
        setDirty(false);
        _signalAll();
        emit loadComplete();
    }
    _itemsRequested = false;
}

void GeoFenceController::_managerSendComplete(bool error)
{
    // Fly view always reloads on manager sendComplete
    if (!error && !_editMode) {
        showPlanFromManagerVehicle();
    }
}

void GeoFenceController::_managerRemoveAllComplete(bool error)
{
    if (!error) {
        // Remove all from vehicle so we always update
        showPlanFromManagerVehicle();
    }
}

bool GeoFenceController::containsItems(void) const
{
    return _inclusionPolygons.count() > 0 || _exclusionPolygons.count() > 0;
}

void GeoFenceController::_updateContainsItems(void)
{
    emit containsItemsChanged(containsItems());
}

bool GeoFenceController::showPlanFromManagerVehicle(void)
{
    qCDebug(GeoFenceControllerLog) << "showPlanFromManagerVehicle" << _editMode;
    if (_masterController->offline()) {
        qCWarning(GeoFenceControllerLog) << "GeoFenceController::showPlanFromManagerVehicle called while offline";
        return true;    // stops further propagation of showPlanFromManagerVehicle due to error
    } else {
        _itemsRequested = true;
        if (!_managerVehicle->initialPlanRequestComplete()) {
            // The vehicle hasn't completed initial load, we can just wait for loadComplete to be signalled automatically
            qCDebug(GeoFenceControllerLog) << "showPlanFromManagerVehicle: !initialPlanRequestComplete, wait for signal";
            return true;
        } else if (syncInProgress()) {
            // If the sync is already in progress, _loadComplete will be called automatically when it is done. So no need to do anything.
            qCDebug(GeoFenceControllerLog) << "showPlanFromManagerVehicle: syncInProgress wait for signal";
            return true;
        } else {
            // Fake a _loadComplete with the current items
            qCDebug(GeoFenceControllerLog) << "showPlanFromManagerVehicle: sync complete simulate signal";
            _itemsRequested = true;
            _managerLoadComplete();
            return false;
        }
    }
}

void GeoFenceController::addInclusion(QGeoCoordinate topLeft, QGeoCoordinate bottomRight)
{
    QGCMapPolygon* polygon = new QGCMapPolygon(this);

    polygon->appendVertex(QGeoCoordinate(topLeft.latitude(), topLeft.longitude()));
    polygon->appendVertex(QGeoCoordinate(topLeft.latitude(), bottomRight.longitude()));
    polygon->appendVertex(QGeoCoordinate(bottomRight.latitude(), bottomRight.longitude()));
    polygon->appendVertex(QGeoCoordinate(bottomRight.latitude(), topLeft.longitude()));

    _inclusionPolygons.append(polygon);
}

void GeoFenceController::addExclusion(QGeoCoordinate topLeft, QGeoCoordinate bottomRight)
{
    QGCMapPolygon* polygon = new QGCMapPolygon(this);

    polygon->appendVertex(QGeoCoordinate(topLeft.latitude(), topLeft.longitude()));
    polygon->appendVertex(QGeoCoordinate(topLeft.latitude(), bottomRight.longitude()));
    polygon->appendVertex(QGeoCoordinate(bottomRight.latitude(), bottomRight.longitude()));
    polygon->appendVertex(QGeoCoordinate(bottomRight.latitude(), topLeft.longitude()));

    _exclusionPolygons.append(polygon);
}
