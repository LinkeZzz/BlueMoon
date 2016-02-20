#pragma once

#include "ui_remoteselector.h"

#include <QtCore>
#include <QtBluetooth>
#include <QtWidgets>

#include "pindialog.h"
#include "trusteddevicelist.h"

class RemoteSelector
    : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteSelector(QWidget* = 0);

    void startDiscovery(const QBluetoothUuid& uuid);
    QBluetoothServiceInfo service() const;

    /*
     * Devicelist settings
     */
    void showtrustedDeviceList(trusteddevicelist tdl);
    void addToTrustList(trusteddevicelist tdl,const QBluetoothServiceInfo& serviceInfo);
    void deleteFromTrustList(trusteddevicelist tdl,const QBluetoothServiceInfo& serviceInfo);

private:
    QScopedPointer<Ui::RemoteSelector> ui;
    QScopedPointer<QBluetoothLocalDevice> localDevice_;
    QScopedPointer<QBluetoothServiceDiscoveryAgent> discoveryAgent_;

    QBluetoothServiceInfo service_;
    QMap<int, QBluetoothServiceInfo> discoveredServices_;
    QScopedPointer<PinDialog> pinDialog_;
    bool pairingErrorFlag_;

    QString addressToName(const QBluetoothAddress& address);

    trusteddevicelist trustedDevicelist;

public Q_SLOTS:
    void startDiscovery();

private slots:
    void serviceDiscovered(const QBluetoothServiceInfo &serviceInfo);
    void discoveryFinished();
    void on_refreshPB_clicked();
    void on_stopButton_clicked();
    void sendFileButton_clicked();

    void pairingFinished(const QBluetoothAddress &address,QBluetoothLocalDevice::Pairing pairing);
    void pairingError(QBluetoothLocalDevice::Error error);
    void displayPin(const QBluetoothAddress &address, QString pin);
    void displayConfirmation(const QBluetoothAddress &address, QString pin);
    void displayConfReject();
    void displayConfAccepted();

    void on_remoteDevices_cellClicked(int row, int column);
    void on_remoteDevices_itemChanged(QTableWidgetItem* item);


};
