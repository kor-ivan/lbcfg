#ifndef FIRMWAREPACKAGE_H
#define FIRMWAREPACKAGE_H

#include <QString>

class FirmwarePackage
{
public:
    struct Result {
        QString path;
        QString error;
        bool temporary = false;

        bool isOk() const { return error.isEmpty() && !path.isEmpty(); }
    };

    static Result prepare(const QString &sourcePath);
    static void cleanup(const Result &result);

private:
    static Result decompressXz(const QString &sourcePath);
};

#endif // FIRMWAREPACKAGE_H
