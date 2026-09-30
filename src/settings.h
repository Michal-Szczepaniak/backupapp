#ifndef SETTINGS_H
#define SETTINGS_H

#include <QMap>
#include <QObject>
#include <QVariant>

class Settings : public QObject
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList getProfiles();

signals:

private:
    QMap<QString, QString> _settingsMap;
};

#endif // SETTINGS_H
