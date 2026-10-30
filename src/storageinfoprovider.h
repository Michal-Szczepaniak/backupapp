#ifndef STORAGEINFOPROVIDER_H
#define STORAGEINFOPROVIDER_H

#include <QObject>

class StorageInfoProvider : public QObject
{
    Q_OBJECT
public:
    explicit StorageInfoProvider(QObject *parent = nullptr);

    Q_INVOKABLE qint64 freeBytes(const QString &path) const;
};

#endif // STORAGEINFOPROVIDER_H
