/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "DataPropertiesPanel.h"
#include "ui_DataPropertiesPanel.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "SetTiltAnglesOperator.h"
#include "SetTiltAnglesReaction.h"
#include "Utilities.h"

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqPropertiesPanel.h>
#include <pqProxyWidget.h>
#include <pqView.h>
#include <vtkDataSetAttributes.h>
#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <vtkAlgorithm.h>
#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkFieldData.h>
#include <vtkSMSourceProxy.h>

#include <QDebug>
#include <QDoubleValidator>
#include <QGroupBox>
#include <QMainWindow>

namespace tomviz {

void setupInformationWidget(pqProxyInformationWidget* infoWidget) {
  // Remove the spacer from the information widgets. These add too much space
  // below the information widgets.
  QLayout* layout = infoWidget->layout();
  for (int i = 0; i < layout->count(); ++i) {
    QLayoutItem *layoutItem = layout->itemAt(i);
    if (layoutItem->spacerItem()) {
        layout->removeItem(layoutItem);
        delete layoutItem;
        --i;
    }
  }

  // Hide some information groups
  const char* groupBoxNames[] =
    { "properties", "groupDataTime", "groupBox", "groupBox_3" };
  for (size_t i = 0; i < sizeof(groupBoxNames)/sizeof(char*); ++i) {
    QGroupBox* gb =
      infoWidget->findChild<QGroupBox *>(groupBoxNames[i]);
    if (gb) {
      gb->setVisible(false);
    }
  }
}

DataPropertiesPanel::DataPropertiesPanel(QWidget* parentObject)
  : QWidget(parentObject), m_ui(new Ui::DataPropertiesPanel)
{
  m_ui->setupUi(this);
  m_ui->xLengthBox->setValidator(new QDoubleValidator(m_ui->xLengthBox));
  m_ui->yLengthBox->setValidator(new QDoubleValidator(m_ui->yLengthBox));
  m_ui->zLengthBox->setValidator(new QDoubleValidator(m_ui->zLengthBox));

  // Clean up the information widgets
  setupInformationWidget(m_ui->OriginalInformationWidget);
  setupInformationWidget(m_ui->TransformedInformationWidget);

  // Disconnect certain signals and slots from the information widget
  disconnect(&pqActiveObjects::instance(), SIGNAL(portChanged(pqOutputPort*)),
    m_ui->OriginalInformationWidget, SLOT(setOutputPort(pqOutputPort*)));
  disconnect(&pqActiveObjects::instance(), SIGNAL(portChanged(pqOutputPort*)),
    m_ui->TransformedInformationWidget, SLOT(setOutputPort(pqOutputPort*)));

  QVBoxLayout* l = m_ui->verticalLayout;

  l->setSpacing(pqPropertiesPanel::suggestedVerticalSpacing());

  // add separator labels.
  QWidget* separator = nullptr;
  separator =
    pqProxyWidget::newGroupLabelWidget("Original Dimensions & Range", this);
  l->insertWidget(l->indexOf(m_ui->OriginalInformationWidget), separator);

  separator =
    pqProxyWidget::newGroupLabelWidget("Transformed Dimensions & Range", this);
  l->insertWidget(l->indexOf(m_ui->TransformedInformationWidget), separator);

  separator = pqProxyWidget::newGroupLabelWidget("Units and Size", this);
  l->insertWidget(l->indexOf(m_ui->LengthWidget), separator);

  m_tiltAnglesSeparator =
    pqProxyWidget::newGroupLabelWidget("Tilt Angles", this);
  l->insertWidget(l->indexOf(m_ui->SetTiltAnglesButton), m_tiltAnglesSeparator);

  clear();

  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(setDataSource(DataSource*)));
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateAxesGridLabels()));
  connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
          SLOT(updateAxesGridLabels()));
  connect(m_ui->SetTiltAnglesButton, SIGNAL(clicked()), SLOT(setTiltAngles()));
  connect(m_ui->unitBox, SIGNAL(editingFinished()), SLOT(updateUnits()));
  connect(m_ui->xLengthBox, SIGNAL(editingFinished()), SLOT(updateXLength()));
  connect(m_ui->yLengthBox, SIGNAL(editingFinished()), SLOT(updateYLength()));
  connect(m_ui->zLengthBox, SIGNAL(editingFinished()), SLOT(updateZLength()));
}

DataPropertiesPanel::~DataPropertiesPanel()
{
}

void DataPropertiesPanel::paintEvent(QPaintEvent* e)
{
  updateData();
  QWidget::paintEvent(e);
}

void DataPropertiesPanel::setDataSource(DataSource* dsource)
{
  if (m_currentDataSource) {
    disconnect(m_currentDataSource);
  }

  // Clean up the information widgets and any pqPipelineSources we created the
  // last time this method was called.
  pqOutputPort* port = m_ui->OriginalInformationWidget->getOutputPort();
  if (port) {
    pqPipelineSource* source = port->getSource();
    if (source) {
      source->deleteLater();
    }
  }
  m_ui->OriginalInformationWidget->setOutputPort(nullptr);
  port = m_ui->TransformedInformationWidget->getOutputPort();
  if (port) {
    pqPipelineSource* source = port->getSource();
    if (source) {
      source->deleteLater();
    }
  }

  m_currentDataSource = dsource;
  if (dsource) {
    connect(dsource, SIGNAL(dataChanged()), SLOT(scheduleUpdate()),
            Qt::UniqueConnection);

    // Create a temporary pipeline source that we use to get a pqOutputPort
    // suitable to set in ParaView's information widget.
    pqPipelineSource* source =
      new pqPipelineSource("", dsource->originalDataSource(),
        pqActiveObjects::instance().activeServer());
    m_ui->OriginalInformationWidget->setOutputPort(source->getOutputPort(0));

    source =
      new pqPipelineSource("", dsource->producer(),
        pqActiveObjects::instance().activeServer());
    m_ui->TransformedInformationWidget->setOutputPort(source->getOutputPort(0));
  }
  scheduleUpdate();
}


void DataPropertiesPanel::updateData()
{
  if (!m_updateNeeded) {
    return;
  }

  disconnect(m_ui->TiltAnglesTable, SIGNAL(cellChanged(int, int)), this,
             SLOT(onTiltAnglesModified(int, int)));
  clear();

  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }

  int extent[6];
  double spacing[3];
  dsource->getExtent(extent);
  dsource->getSpacing(spacing);
  m_ui->xLengthBox->setText(
    QString("%1").arg(spacing[0] * (extent[1] - extent[0] + 1)));
  m_ui->yLengthBox->setText(
    QString("%1").arg(spacing[1] * (extent[3] - extent[2] + 1)));
  m_ui->zLengthBox->setText(
    QString("%1").arg(spacing[2] * (extent[5] - extent[4] + 1)));
  m_ui->unitBox->setText(m_currentDataSource->getUnits(0));

  // display tilt series data
  if (dsource->type() == DataSource::TiltSeries) {
    m_tiltAnglesSeparator->show();
    m_ui->SetTiltAnglesButton->show();
    m_ui->TiltAnglesTable->show();
    QVector<double> tiltAngles = dsource->getTiltAngles();
    m_ui->TiltAnglesTable->setRowCount(tiltAngles.size());
    m_ui->TiltAnglesTable->setColumnCount(1);
    for (int i = 0; i < tiltAngles.size(); ++i) {
      QTableWidgetItem* item = new QTableWidgetItem();
      item->setData(Qt::DisplayRole, QString::number(tiltAngles[i]));
      m_ui->TiltAnglesTable->setItem(i, 0, item);
    }
  } else {
    m_tiltAnglesSeparator->hide();
    m_ui->SetTiltAnglesButton->hide();
    m_ui->TiltAnglesTable->hide();
  }
  connect(m_ui->TiltAnglesTable, SIGNAL(cellChanged(int, int)),
          SLOT(onTiltAnglesModified(int, int)));

  m_updateNeeded = false;
}

void DataPropertiesPanel::onTiltAnglesModified(int row, int column)
{
  DataSource* dsource = m_currentDataSource;
  // The table shouldn't be shown if this is not true, so this slot shouldn't be
  // called
  Q_ASSERT(dsource->type() == DataSource::TiltSeries);
  QTableWidgetItem* item = m_ui->TiltAnglesTable->item(row, column);
  auto ok = false;
  auto value = item->data(Qt::DisplayRole).toDouble(&ok);
  if (ok) {
    auto needToAdd = false;
    SetTiltAnglesOperator* op = nullptr;
    if (dsource->operators().size() > 0) {
      op = qobject_cast<SetTiltAnglesOperator*>(dsource->operators().last());
    }
    if (!op) {
      op = new SetTiltAnglesOperator;
      op->setParent(dsource);
      needToAdd = true;
    }
    auto tiltAngles = op->tiltAngles();
    tiltAngles[row] = value;
    op->setTiltAngles(tiltAngles);
    if (needToAdd) {
      dsource->addOperator(op);
    }
  } else {
    std::cerr << "Invalid tilt angle." << std::endl;
  }
}

void DataPropertiesPanel::setTiltAngles()
{
  DataSource* dsource = m_currentDataSource;
  auto mainWindow = qobject_cast<QMainWindow*>(window());
  SetTiltAnglesReaction::showSetTiltAnglesUI(mainWindow, dsource);
}

void DataPropertiesPanel::scheduleUpdate()
{
  m_updateNeeded = true;
  if (isVisible()) {
    updateData();
  }
}

void DataPropertiesPanel::updateUnits()
{
  if (m_currentDataSource) {
    const QString& text = m_ui->unitBox->text();
    m_currentDataSource->setUnits(text);
    updateAxesGridLabels();
  }
}

void DataPropertiesPanel::updateXLength()
{
  const QString& text = m_ui->xLengthBox->text();
  auto ok = false;
  double newLength = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse X Length string";
    return;
  }
  updateSpacing(0, newLength);
  updateData();
  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }
  emit dsource->dataPropertiesChanged();
}

void DataPropertiesPanel::updateYLength()
{
  const QString& text = m_ui->yLengthBox->text();
  auto ok = false;
  double newLength = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse Y Length string";
    return;
  }
  updateSpacing(1, newLength);
  updateData();
  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }
  emit dsource->dataPropertiesChanged();
}

void DataPropertiesPanel::updateZLength()
{
  const QString& text = m_ui->zLengthBox->text();
  auto ok = false;
  double newLength = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse Z Length string";
    return;
  }
  updateSpacing(2, newLength);
  updateData();
  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }
  emit dsource->dataPropertiesChanged();
}

void DataPropertiesPanel::updateAxesGridLabels()
{
  vtkSMViewProxy* view = ActiveObjects::instance().activeView();
  if (!view) {
    return;
  }
  auto axesGrid = vtkSMPropertyHelper(view, "AxesGrid", true).GetAsProxy();
  DataSource* ds = ActiveObjects::instance().activeDataSource();
  if (!axesGrid || !ds) {
    return;
  }
  QString xTitle = QString("X (%1)").arg(ds->getUnits(0));
  QString yTitle = QString("Y (%1)").arg(ds->getUnits(1));
  QString zTitle = QString("Z (%1)").arg(ds->getUnits(2));
  vtkSMPropertyHelper(axesGrid, "XTitle").Set(xTitle.toUtf8().data());
  vtkSMPropertyHelper(axesGrid, "YTitle").Set(yTitle.toUtf8().data());
  vtkSMPropertyHelper(axesGrid, "ZTitle").Set(zTitle.toUtf8().data());
  axesGrid->UpdateVTKObjects();

  pqView* qtView =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (qtView) {
    qtView->render();
  }
}

void DataPropertiesPanel::clear()
{
  if (m_colorMapWidget) {
    m_ui->verticalLayout->removeWidget(m_colorMapWidget);
    delete m_colorMapWidget;
  }
  m_tiltAnglesSeparator->hide();
  m_ui->SetTiltAnglesButton->hide();
  m_ui->TiltAnglesTable->clear();
  m_ui->TiltAnglesTable->setRowCount(0);
  m_ui->TiltAnglesTable->hide();
}

void DataPropertiesPanel::updateSpacing(int axis, double newLength)
{
  if (!m_currentDataSource) {
    return;
  }
  int extent[6];
  double spacing[3];
  m_currentDataSource->getExtent(extent);
  m_currentDataSource->getSpacing(spacing);
  spacing[axis] = newLength / (extent[2 * axis + 1] - extent[2 * axis] + 1);
  m_currentDataSource->setSpacing(spacing);
}
}
