/* remoteselector.cpp
* Bluemoon - a Bluetooth manager designed for LXQt desktop environment
* https://github.com/sklins/BlueMoon
*
* Copyright: 2016, Bluemoon Team
* Authors:
* Sofiia Sandomirskaya <ssandomirskaya@gmail.com>
* Elina Gilmanova <linkegilmanova@gmail.com>
* Denis Pavlov <dspavlov_1@edu.hse.ru>
* Linh Chi Tran <lc.tinatran@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published
* by the Free Software Foundation; either version 2.1 of the License,
* or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General
* Public License along with this library; if not, write to the
* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
* Boston, MA 02110-1301 USA
*/

#include "remoteselector.h"

#include "progress.h"

RemoteSelector::RemoteSelector(QWidget* parent)
    : QWidget(parent)
    , pairingErrorFlag_(false)
{
    ui.reset(new Ui::RemoteSelector);
    ui->setupUi(this);

    //Stuff with local trusted deviceList
    trustedDevicelist =  trusteddevicelist();
    //TODO:check path
    //if (!QDir("/etc/bluemoon_bluetooth").exists())
    //    QDir().mkdir("/home/linke/Desktop/bluemoon_blth");

    QString dir=QDir::currentPath();
    int lastIndex=dir.lastIndexOf('/');
    dir.resize(lastIndex+1);

    trustedDevicelist.setFileName(QString("list.dat"));
    //TODO: choose file direcory
    //trustedDevicelist.setFileDirectory("/etc/bluemoon_bluetooth/");
    trustedDevicelist.setFileDirectory(dir);
    trustedDevicelist.readTrustedDeviceList();

    localDevice_.reset(new QBluetoothLocalDevice);
    QBluetoothAddress adapterAddress = localDevice_->address();
    discoveryAgent_.reset(new QBluetoothServiceDiscoveryAgent(adapterAddress));

    connect(discoveryAgent_.data(), &QBluetoothServiceDiscoveryAgent::serviceDiscovered, this, &RemoteSelector::serviceDiscovered);
    connect(discoveryAgent_.data(), &QBluetoothServiceDiscoveryAgent::finished, this, &RemoteSelector::discoveryFinished);
    connect(discoveryAgent_.data(), &QBluetoothServiceDiscoveryAgent::canceled, this, &RemoteSelector::discoveryFinished);

    ui->remoteDevices->setColumnWidth(3, 75);
    ui->remoteDevices->setColumnWidth(4, 100);

    connect(localDevice_.data(), &QBluetoothLocalDevice::pairingDisplayPinCode, this, &RemoteSelector::displayPin);
    connect(localDevice_.data(), &QBluetoothLocalDevice::pairingDisplayConfirmation, this, &RemoteSelector::displayConfirmation);
    connect(localDevice_.data(), &QBluetoothLocalDevice::pairingFinished, this, &RemoteSelector::pairingFinished);
    connect(localDevice_.data(), &QBluetoothLocalDevice::error, this, &RemoteSelector::pairingError);

    ui->busyWidget->setMovie(new QMovie(":/icons/busy.gif"));
    //ui->busyWidget->movie()->start();

    ui->pairingBusy->setMovie(new QMovie(":/icons/pairing.gif"));
    ui->pairingBusy->hide();

    ui->remoteDevices->clearContents();
    ui->remoteDevices->setRowCount(0);

    showtrustedDeviceList(trustedDevicelist);
    connect(ui->checkBox_BtOnOff, &QCheckBox::clicked, this, &RemoteSelector::powerOnOff);
    connect(ui->checkBox_BtVisible,&QCheckBox::clicked, this, &RemoteSelector::visibilityOnOff);

    connect(ui->sendFilesButton, &QPushButton::clicked, this, &RemoteSelector::sendFileButton_clicked);

    //local name
    QString labelText = "Local device name: ";
    labelText .append("<b>");
    labelText .append(localDevice_.data()->name());
    labelText .append("</b>");
    ui->label->setText(labelText);

    createActions();
    createTrayIcon();
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
}

void RemoteSelector::startDiscovery(const QBluetoothUuid& uuid) {
    ui->stopButton->setDisabled(false);
    ui->sendFilesButton->setDisabled(true);
    if (discoveryAgent_->isActive())
        discoveryAgent_->stop();

    discoveryAgent_->setUuidFilter(uuid);
    discoveryAgent_->start();

    if (!discoveryAgent_->isActive() ||
            discoveryAgent_->error() != QBluetoothServiceDiscoveryAgent::NoError)
    {
        ui->status->setText(tr("Cannot find remote services."));
    } else {
        ui->status->setText(tr("Scanning..."));
        ui->busyWidget->show();
        ui->busyWidget->movie()->start();
    }
}

QBluetoothServiceInfo RemoteSelector::service() const {
    return service_;
}

void RemoteSelector::serviceDiscovered(const QBluetoothServiceInfo& serviceInfo) {
#if DEBUG_OUTPUT
    qDebug() << "Discovered service on"
             << serviceInfo.device().name() << serviceInfo.device().address().toString();
    qDebug() << "\tService name:" << serviceInfo.serviceName();
    qDebug() << "\tDescription:"
             << serviceInfo.attribute(QBluetoothServiceInfo::ServiceDescription).toString();
    qDebug() << "\tProvider:"
             << serviceInfo.attribute(QBluetoothServiceInfo::ServiceProvider).toString();
    qDebug() << "\tL2CAP protocol service multiplexer:"
             << serviceInfo.protocolServiceMultiplexer();
    qDebug() << "\tRFCOMM server channel:" << serviceInfo.serverChannel();
#endif

    QString remoteName;
    if (serviceInfo.device().name().isEmpty())
        remoteName = serviceInfo.device().address().toString();
    else
        remoteName = serviceInfo.device().name();

    QMutableMapIterator<int, QBluetoothServiceInfo> i(discoveredServices_);
    while (i.hasNext()){
        i.next();
        if (serviceInfo.device().address() == i.value().device().address()){
            i.setValue(serviceInfo);
            return;
        }
    }

    QString tooltip = QString("address: %1\nname: %2\nis trusted: %3\nis valid: %4\nservice: %5").arg(
                serviceInfo.device().address().toString(),
                serviceInfo.device().name(),
                trustedDevicelist.isTrusted(serviceInfo.device().address().toString()) ? "yes" : "no",
                serviceInfo.device().isValid() ? "yes" : "no",
                serviceInfo.serviceDescription());
    if (!trustedDevicelist.isTrusted(serviceInfo.device().address().toString()))
    {
        int row = ui->remoteDevices->rowCount();
        ui->remoteDevices->insertRow(row);
        QTableWidgetItem *item = new QTableWidgetItem(serviceInfo.device().address().toString());
        item->setToolTip(tooltip);
        ui->remoteDevices->setItem(row, 0, item);
        item = new QTableWidgetItem(serviceInfo.device().name());
        ui->remoteDevices->setItem(row, 1, item);
        item = new QTableWidgetItem(serviceInfo.serviceName());

        ui->remoteDevices->setItem(row, 2, item);

        QBluetoothLocalDevice::Pairing p;

        p = localDevice_->pairingStatus(serviceInfo.device().address());

        ui->remoteDevices->blockSignals(true);

        item = new QTableWidgetItem();
        if ((p&QBluetoothLocalDevice::Paired) || (p&QBluetoothLocalDevice::AuthorizedPaired))
            item->setCheckState(Qt::Checked);
        else
            item->setCheckState(Qt::Unchecked);
        ui->remoteDevices->setItem(row, 3, item);

        item = new QTableWidgetItem();
        if (p&QBluetoothLocalDevice::AuthorizedPaired)
            item->setCheckState(Qt::Checked);
        else
            item->setCheckState(Qt::Unchecked);
        ui->remoteDevices->setItem(row, 4, item);

        item = new QTableWidgetItem();
        if (trustedDevicelist.isTrusted(serviceInfo.device().address().toString()))
            item->setCheckState(Qt::Checked);
        else
            item->setCheckState(Qt::Unchecked);
        ui->remoteDevices->setItem(row, 5, item);

        ui->remoteDevices->blockSignals(false);


        discoveredServices_.insert(row, serviceInfo);
    }
}

void RemoteSelector::discoveryFinished() {
    ui->status->setText(tr("Select the device to send to."));
    ui->sendFilesButton->setEnabled(true);
    ui->stopButton->setDisabled(true);
    ui->busyWidget->movie()->stop();
    ui->busyWidget->hide();
}

void RemoteSelector::startDiscovery() {
    startDiscovery(QBluetoothUuid(QBluetoothUuid::ObexObjectPush));
}

void RemoteSelector::on_refreshPB_clicked() {
    startDiscovery();
    ui->stopButton->setDisabled(false);
}

void RemoteSelector::on_stopButton_clicked() {
    discoveryAgent_->stop();
}

QString RemoteSelector::addressToName(const QBluetoothAddress &address) {
    QMapIterator<int, QBluetoothServiceInfo> i(discoveredServices_);
    while (i.hasNext()){
        i.next();
        if (i.value().device().address() == address)
            return i.value().device().name();
    }
    return address.toString();
}

void RemoteSelector::displayPin(const QBluetoothAddress &address, QString pin) {
    pinDialog_.reset(new PinDialog(this, QString("Enter pairing pin on: %1").arg(addressToName(address)), pin));
    pinDialog_->show();
}

void RemoteSelector::displayConfirmation(const QBluetoothAddress &address, QString pin) {
    Q_UNUSED(address);

    pinDialog_.reset(new PinDialog(this, QString("Confirm this pin is the same"), pin));
    connect(pinDialog_.data(), &PinDialog::accepted, this, &RemoteSelector::displayConfAccepted);
    connect(pinDialog_.data(), &PinDialog::rejected, this, &RemoteSelector::displayConfReject);
    pinDialog_->initializeComponent();
    pinDialog_->show();
}

void RemoteSelector::displayConfAccepted() {
    localDevice_->pairingConfirmation(true);
}

void RemoteSelector::displayConfReject() {
    localDevice_->pairingConfirmation(false);
}

void RemoteSelector::pairingFinished(const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing status) {
    QBluetoothServiceInfo service;
    int row = 0;

    ui->pairingBusy->hide();
    ui->pairingBusy->movie()->stop();

    ui->remoteDevices->blockSignals(true);

    for (int i = 0; i < discoveredServices_.count(); i++) {
        if (discoveredServices_.value(i).device().address() == address) {
            service = discoveredServices_.value(i);
            row = i;
            break;
        }
    }

    pinDialog_.reset();

    QMessageBox msgBox;
    if (pairingErrorFlag_) {
        msgBox.setText("Pairing failed with " + address.toString());
    } else if (status == QBluetoothLocalDevice::Paired
               || status == QBluetoothLocalDevice::AuthorizedPaired) {
        msgBox.setText("Paired successfully with " + address.toString());
    } else {
        msgBox.setText("Pairing released with " + address.toString());
    }

    if (service.isValid()) {
        if (status == QBluetoothLocalDevice::AuthorizedPaired) {
            ui->remoteDevices->item(row, 3)->setCheckState(Qt::Checked);
            ui->remoteDevices->item(row, 4)->setCheckState(Qt::Checked);
        }
        else if (status == QBluetoothLocalDevice::Paired) {
            ui->remoteDevices->item(row, 3)->setCheckState(Qt::Checked);
            ui->remoteDevices->item(row, 4)->setCheckState(Qt::Unchecked);
        }
        else {
            ui->remoteDevices->item(row, 3)->setCheckState(Qt::Unchecked);
            ui->remoteDevices->item(row, 4)->setCheckState(Qt::Unchecked);
        }
    }

    pairingErrorFlag_ = false;
    msgBox.exec();

    ui->remoteDevices->blockSignals(false);
}

void RemoteSelector::pairingError(QBluetoothLocalDevice::Error error) {
    if (error != QBluetoothLocalDevice::PairingError)
        return;

    pairingErrorFlag_ = true;
    pairingFinished(service_.device().address(), QBluetoothLocalDevice::Unpaired);
}

//
void RemoteSelector::on_remoteDevices_cellClicked(int row, int column) {
    Q_UNUSED(column);
    service_ = discoveredServices_.value(row);
    QBluetoothServiceInfo service;
    QTableWidgetItem *item;

    QAbstractItemModel* model = ui->remoteDevices->model();
    QModelIndex idx = model->index(row, 0);
    QString macAddress = model->data(idx).toString();

    /*if (column==5)
    {
        if(trustedDevicelist.isTrusted(macAddress))
        {
            item=new QTableWidgetItem();
            item->setCheckState(Qt::Unchecked);
            ui->remoteDevices->setItem(row,5, item);

            trustedDevicelist.deleteFromTrustList(macAddress);
            trustedDevicelist.writeToTrustedDeviceList();
        }
        else
        {
            item=new QTableWidgetItem();
            item->setCheckState(Qt::Checked);
            ui->remoteDevices->setItem(row,5, item);

            QString address=  ui->remoteDevices->item(row, 0)->text();
            for (int i = 0; i < discoveredServices_.count(); i++)
            {
                if (discoveredServices_.value(i).device().address().toString() == address) {
                    service = discoveredServices_.value(i);
                }
            }
            trustedDevicelist.addToTrustList(service);
            trustedDevicelist.writeToTrustedDeviceList();
        }//

    }*/
}

//
void RemoteSelector::on_remoteDevices_itemChanged(QTableWidgetItem* item) {
    int row = item->row();
    int column = item->column();
    service_ = discoveredServices_.value(row);

    if (column < 3 | column == 5)
        return;

    if (item->checkState() == Qt::Unchecked && column == 3){
        localDevice_->requestPairing(service_.device().address(), QBluetoothLocalDevice::Unpaired);
        return; // don't continue and start movie
    }
    else if ((item->checkState() == Qt::Checked && column == 3) ||
            (item->checkState() == Qt::Unchecked && column == 4)){
        localDevice_->requestPairing(service_.device().address(), QBluetoothLocalDevice::Paired);
        ui->remoteDevices->blockSignals(true);
        ui->remoteDevices->item(row, column)->setCheckState(Qt::PartiallyChecked);
        ui->remoteDevices->blockSignals(false);
    }
    else if (item->checkState() == Qt::Checked && column == 4){
        localDevice_->requestPairing(service_.device().address(), QBluetoothLocalDevice::AuthorizedPaired);
        ui->remoteDevices->blockSignals(true);
        ui->remoteDevices->item(row, column)->setCheckState(Qt::PartiallyChecked);
        ui->remoteDevices->blockSignals(false);
    }
    ui->pairingBusy->show();
    ui->pairingBusy->movie()->start();
        
}

void RemoteSelector::sendFileButton_clicked() {
    Progress* p = new Progress(0, service_);
    p->show();
}

// Show saved trusted device list in the table widget
void RemoteSelector::showtrustedDeviceList(trusteddevicelist tdl)
{

    QVector<QVector<QString> >trustedList=tdl.getTrustedDevices();
    QTableWidgetItem *item;
    for (int i = 0; i < trustedList.size(); ++i)
    {
        int row = ui->remoteDevices->rowCount();
        ui->remoteDevices->insertRow(row);
        item=new QTableWidgetItem(trustedList[i][2]);
        ui->remoteDevices->setItem(i,0, item);

        item=new QTableWidgetItem(trustedList[i][1]);
        ui->remoteDevices->setItem(i,1, item);

        item=new QTableWidgetItem(trustedList[i][0]);
        ui->remoteDevices->setItem(i,2, item);

        item=new QTableWidgetItem();
        item->setCheckState(Qt::Checked);
        ui->remoteDevices->setItem(i,5,item);
    }
}


// Tray Icon creating.
// Overriding the virtual function
void RemoteSelector::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if(event->type() == QEvent::WindowStateChange)
        if(isMinimized())
            this->hide();
}

void RemoteSelector::createActions()
{
    minimizeAction = new QAction(tr("Minimize"), this);
    connect(minimizeAction, SIGNAL(triggered()), this, SLOT(hide()));

    maximizeAction = new QAction(tr("Maximize"), this);
    connect(maximizeAction, SIGNAL(triggered()), this, SLOT(showMaximized()));

    bluetoothOnAction = new QAction(tr("Bluetooth ON"), this);
    connect(bluetoothOnAction, SIGNAL(triggered()), this, SLOT(bluetoothOn()));

    bluetoothOffAction = new QAction(tr("Bluetooth OFF"), this);
    connect(bluetoothOffAction, SIGNAL(triggered()), this, SLOT(bluetoothOff()));

    quitAction = new QAction(tr("Quit"), this);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
}

void RemoteSelector::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(bluetoothOnAction);
    trayIconMenu->addAction(bluetoothOffAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(minimizeAction);
    trayIconMenu->addAction(maximizeAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setIcon(QIcon("../icons/scalable_bluemoon.svg"));
}
void RemoteSelector::changeHostMode()
{
    if (RemoteSelector::turnOnOff & RemoteSelector::visibility)
        localDevice_.data()->setHostMode(QBluetoothLocalDevice::HostDiscoverable);
    if (RemoteSelector::turnOnOff & !RemoteSelector::visibility)
        localDevice_.data()->setHostMode(QBluetoothLocalDevice::HostConnectable);

    if (!RemoteSelector::turnOnOff & RemoteSelector::visibility)
        localDevice_.data()->setHostMode(QBluetoothLocalDevice::HostDiscoverableLimitedInquiry);
    if (!RemoteSelector::turnOnOff & !RemoteSelector::visibility)
        localDevice_.data()->setHostMode(QBluetoothLocalDevice::HostPoweredOff);

}

void RemoteSelector::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::MiddleClick:
        showNormal();
    default:
        ;
    }
}

void RemoteSelector::bluetoothOn()
{
    this->showNormal();
    localDevice_.data()->setHostMode(QBluetoothLocalDevice::HostDiscoverable);
    ui->checkBox_BtOnOff->setChecked(true);
    ui->checkBox_BtVisible->setEnabled(true);

}

void RemoteSelector::bluetoothOff()
{
    localDevice_.data()->setHostMode(QBluetoothLocalDevice::HostPoweredOff);
    ui->checkBox_BtOnOff->setChecked(false);
    ui->checkBox_BtVisible->setChecked(false);
    ui->checkBox_BtVisible->setEnabled(false);
}
void RemoteSelector::visibilityOnOff()
{
    if (ui->checkBox_BtVisible->isChecked() )
        RemoteSelector::visibility=true;
    else
        RemoteSelector::visibility=false;
    changeHostMode();
}
void RemoteSelector::powerOnOff()
{
    if (ui->checkBox_BtOnOff->isChecked() )
    {
        RemoteSelector::turnOnOff=true;
        ui->checkBox_BtVisible->setEnabled(true);
        ui->refreshPB->setEnabled(true);
    }
    else
    {
        RemoteSelector::turnOnOff=false;
        ui->checkBox_BtVisible->setEnabled(false);
        ui->refreshPB->setEnabled(false);
    }
    changeHostMode();
}
