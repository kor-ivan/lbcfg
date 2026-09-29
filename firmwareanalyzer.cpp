#include "firmwareanalyzer.h"
#include "firmwarepackage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

const QByteArray EspAppDescMagic = QByteArray::fromHex("3254CDAB");
const QRegularExpression LogicBoxVersionPattern(
    QStringLiteral(R"((20\d{12}-[0-9a-fA-F]{7,40}))"));

constexpr qsizetype FirmwareReadLimit = 1024 * 1024;
constexpr qsizetype VersionOffset = 16;
constexpr qsizetype VersionSize = 32;
constexpr qsizetype ProjectNameOffset = 48;
constexpr qsizetype ProjectNameSize = 32;

} // namespace

firmwareAnalyzer::firmwareAnalyzer(QObject *parent)
    : QObject(parent)
{
}

firmwareAnalyzer::firmwareAnalyzer(QObject *parent, const QString &path)
    : QObject(parent),
      m_path(path)
{
    // update();
}

QMap<QString, firmwareAnalyzer::fwinfo> firmwareAnalyzer::getFirmwareMap() const
{
    return m_firmwareMap;
}

QList<firmwareAnalyzer::fwinfo> firmwareAnalyzer::getRejectedFirmware() const
{
    return m_rejectedFirmware;
}

void firmwareAnalyzer::setPath(const QString &path)
{
    m_path = path;
}

QString firmwareAnalyzer::path() const
{
    return m_path;
}

QByteArray firmwareAnalyzer::getCheckSum(const QString &moduleName) const
{
    const auto it = m_firmwareMap.constFind(moduleName);
    if (it == m_firmwareMap.constEnd())
        return {};

    return it.value().checksum;
}

QString firmwareAnalyzer::normalizeVersion(const QString &version)
{
    const QString trimmed = version.trimmed();
    const QRegularExpressionMatch match = LogicBoxVersionPattern.match(trimmed);

    if (match.hasMatch())
        return match.captured(1).toLower();

    return trimmed;
}

bool firmwareAnalyzer::versionsMatch(const QString &left, const QString &right)
{
    const QString normalizedLeft = normalizeVersion(left);
    const QString normalizedRight = normalizeVersion(right);

    if (normalizedLeft.isEmpty() || normalizedRight.isEmpty())
        return false;

    if (normalizedLeft.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) == 0
        || normalizedRight.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) == 0) {
        return false;
    }

    return normalizedLeft.compare(normalizedRight, Qt::CaseInsensitive) == 0;
}

firmwareAnalyzer::Error firmwareAnalyzer::error() const
{
    return m_error;
}

QString firmwareAnalyzer::errorString() const
{
    return m_errorString;
}

void firmwareAnalyzer::update()
{
    m_firmwareMap.clear();
    m_rejectedFirmware.clear();
    m_duplicateModules.clear();
    m_modules.clear();
    m_projectModules.clear();
    m_error = ok;
    m_errorString.clear();

    QFileInfo pathInfo(m_path);

    if (!pathInfo.exists()) {
        m_error = pathNotFound;
        m_errorString = QStringLiteral("Директория firmware не найдена: %1").arg(m_path);
        emit updated();
        return;
    }

    if (!pathInfo.isDir()) {
        m_error = pathIsNotDirectory;
        m_errorString = QStringLiteral("Путь firmware не является директорией: %1").arg(m_path);
        emit updated();
        return;
    }

    if (!loadModulesSchema()) {
        m_error = schemaErr;
        emit updated();
        return;
    }

    QDirIterator it(m_path, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString sourcePath = it.next();
        // Only <schema module name>.bin.xz belongs to the repository catalog.
        // Unpacked BINs and unrelated archives must not create duplicates.
        if (moduleFromFileName(sourcePath).isEmpty()) {
            continue;
        }

        analyzeFile(sourcePath);
    }

    emit updated(getGitHeadHash(m_path));
}

void firmwareAnalyzer::analyzeFile(const QString &sourcePath)
{
    fwinfo info;
    info.sourcePath = QFileInfo(sourcePath).absoluteFilePath();
    info.compressed = sourcePath.endsWith(QStringLiteral(".bin.xz"), Qt::CaseInsensitive);

    bool checksumOk = false;
    QString checksumError;
    info.checksum = calculateSha256(sourcePath, &checksumOk, &checksumError);

    if (!checksumOk) {
        info.err = checksumErr;
        info.errStr = checksumError;
        m_rejectedFirmware.append(info);
        return;
    }

    const FirmwarePackage::Result prepared = FirmwarePackage::prepare(sourcePath);

    if (!prepared.isOk()) {
        info.err = info.compressed ? xzErr : fileOpenErr;
        info.errStr = prepared.error;
        m_rejectedFirmware.append(info);
        return;
    }

    QString projectName;
    QString version;
    bool firmwareValid = false;
    Error metadataError = ok;
    QString metadataErrorString;

    const bool metadataOk = readFirmwareInfo(prepared.path,
                                             &projectName,
                                             &version,
                                             &firmwareValid,
                                             &metadataError,
                                             &metadataErrorString);

    info.projectName = projectName;
    info.version = version.isEmpty() ? QStringLiteral("unknown") : version;
    info.isValid = firmwareValid;

    // Analyzer needs only metadata. A temporary BIN produced from XZ must not
    // remain on disk merely because the user never starts an OTA operation.
    if (!FirmwarePackage::remove(prepared)) {
        info.err = removeTempErr;
        info.errStr = QStringLiteral("Не удалось удалить временный BIN: %1")
                          .arg(prepared.path);
        m_rejectedFirmware.append(info);
        return;
    }

    if (!metadataOk) {
        info.err = metadataError;
        info.errStr = metadataErrorString;
        m_rejectedFirmware.append(info);
        return;
    }

    QString moduleName;

    if (!projectName.isEmpty()) {
        // Primary path for ESP32 firmware: internal project_name is authoritative.
        moduleName = moduleFromProjectName(projectName);

        // If project_name exists but is unknown, do not guess from the filename.
        if (moduleName.isEmpty()) {
            info.err = unknownProjectName;
            info.errStr = QStringLiteral("Неизвестный project_name прошивки: %1")
                              .arg(projectName);
            m_rejectedFirmware.append(info);
            return;
        }
    } else {
        // Fallback for valid images without project_name. This is required for
        // STM32 images, where ESP-IDF app metadata does not exist.
        moduleName = moduleFromFileName(sourcePath);

        if (moduleName.isEmpty()) {
            info.err = projectNameMissing;
            info.errStr = QStringLiteral(
                "В прошивке отсутствует project_name, а имя файла не совпадает "
                "с известным типом модуля");
            m_rejectedFirmware.append(info);
            return;
        }
    }

    if (moduleName != moduleFromFileName(sourcePath)) {
        info.err = moduleNameMismatch;
        info.errStr = QStringLiteral("Имя файла не соответствует project_name %1: %2")
                          .arg(projectName, sourcePath);
        m_rejectedFirmware.append(info);
        return;
    }

    // QMap cannot represent two different firmware files for one module without
    // silently replacing one of them. Treat that as an error instead.
    if (m_firmwareMap.contains(moduleName) || m_duplicateModules.contains(moduleName)) {
        if (m_firmwareMap.contains(moduleName)) {
            fwinfo existing = m_firmwareMap.take(moduleName);
            existing.err = duplicateModule;
            existing.errStr = QStringLiteral("Найдено несколько прошивок для %1")
                                  .arg(moduleName);
            m_rejectedFirmware.append(existing);
        }
        m_duplicateModules.insert(moduleName);

        info.err = duplicateModule;
        info.errStr = QStringLiteral("Дублирующая прошивка для %1: %2")
                          .arg(moduleName, sourcePath);
        m_rejectedFirmware.append(info);
        return;
    }

    if (info.version == QStringLiteral("unknown")) {
        info.err = versionMissing;
        info.errStr = QStringLiteral("Версия прошивки не найдена");
    } else {
        info.err = ok;
        info.errStr.clear();
    }

    m_firmwareMap.insert(moduleName, info);
}

QByteArray firmwareAnalyzer::calculateSha256(const QString &filePath,
                                             bool *okResult,
                                             QString *errorString)
{
    if (okResult)
        *okResult = false;
    if (errorString)
        errorString->clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorString) {
            *errorString = QStringLiteral("Не удалось открыть файл для SHA-256: %1")
                               .arg(file.errorString());
        }
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    constexpr qint64 BufferSize = 1024 * 1024;

    while (!file.atEnd()) {
        const QByteArray chunk = file.read(BufferSize);

        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            if (errorString) {
                *errorString = QStringLiteral("Ошибка чтения файла при SHA-256: %1")
                                   .arg(file.errorString());
            }
            return {};
        }

        hash.addData(chunk);
    }

    if (okResult)
        *okResult = true;

    return hash.result();
}

bool firmwareAnalyzer::readFirmwareInfo(const QString &binPath,
                                        QString *projectName,
                                        QString *version,
                                        bool *isValid,
                                        Error *readError,
                                        QString *errorString)
{
    if (projectName)
        projectName->clear();
    if (version)
        version->clear();
    if (isValid)
        *isValid = false;
    if (readError)
        *readError = ok;
    if (errorString)
        errorString->clear();

    const FirmwarePackage::ImageType imageType = FirmwarePackage::detectImageType(binPath);

    if (imageType == FirmwarePackage::ImageType::Unknown) {
        if (readError)
            *readError = invalidFirmwareImage;
        if (errorString) {
            *errorString = QStringLiteral(
                "Файл не распознан как поддерживаемый ESP32/STM32 firmware image");
        }
        return false;
    }

    if (isValid)
        *isValid = true;

    QFile file(binPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (isValid)
            *isValid = false;
        if (readError)
            *readError = fileOpenErr;
        if (errorString) {
            *errorString = QStringLiteral("Не удалось открыть BIN: %1")
                               .arg(file.errorString());
        }
        return false;
    }

    const QByteArray data = file.read(FirmwareReadLimit);

    if (data.isEmpty() && file.error() != QFile::NoError) {
        if (isValid)
            *isValid = false;
        if (readError)
            *readError = fileOpenErr;
        if (errorString) {
            *errorString = QStringLiteral("Ошибка чтения BIN: %1")
                               .arg(file.errorString());
        }
        return false;
    }

    if (imageType == FirmwarePackage::ImageType::Stm32) {
        // STM32 raw images have no ESP-IDF esp_app_desc_t/project_name.
        // Try to recover the build version if the firmware embeds the usual
        // LogicBox timestamp-hash string; otherwise "unknown" is acceptable.
        if (version)
            *version = findEmbeddedVersion(data);
        return true;
    }

    const qsizetype appDescOffset = data.indexOf(EspAppDescMagic);
    if (appDescOffset < 0) {
        if (readError)
            *readError = appDescriptorNotFound;
        if (errorString)
            *errorString = QStringLiteral("esp_app_desc_t не найден");
        return false;
    }

    const QString detectedVersion = readFixedString(data,
                                                    appDescOffset + VersionOffset,
                                                    VersionSize);
    const QString detectedProjectName = readFixedString(data,
                                                        appDescOffset + ProjectNameOffset,
                                                        ProjectNameSize);

    if (version)
        *version = detectedVersion;
    if (projectName)
        *projectName = detectedProjectName;

    // An empty project_name is not a damaged image. Caller may use the strict
    // filename fallback for known modules.
    return true;
}

QString firmwareAnalyzer::readFixedString(const QByteArray &data,
                                          qsizetype offset,
                                          qsizetype size)
{
    if (offset < 0 || offset + size > data.size())
        return {};

    QByteArray raw = data.mid(offset, size);
    const qsizetype zeroPosition = raw.indexOf('\0');
    if (zeroPosition >= 0)
        raw.truncate(zeroPosition);

    return QString::fromLatin1(raw).trimmed();
}

QString firmwareAnalyzer::findEmbeddedVersion(const QByteArray &data)
{
    // Current LogicBox versions use YYYYMMDDhhmmss-githash, for example
    // 20260806143356-7e0c291. This is only a fallback for non-ESP firmware.
    const QRegularExpressionMatch match =
        LogicBoxVersionPattern.match(QString::fromLatin1(data));

    return match.hasMatch() ? match.captured(1) : QString();
}

bool firmwareAnalyzer::loadModulesSchema()
{
    QFile file(QStringLiteral(":/config/resources/modules_schema.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        m_errorString = QStringLiteral("Не удалось открыть схему модулей: %1")
                            .arg(file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_errorString = QStringLiteral("Некорректная JSON-схема модулей: %1")
                            .arg(parseError.errorString());
        return false;
    }

    const QJsonArray modules = document.object().value(QStringLiteral("slot"))
                                  .toObject().value(QStringLiteral("modules")).toArray();
    if (modules.isEmpty()) {
        m_errorString = QStringLiteral("В схеме отсутствует непустой массив slot.modules");
        return false;
    }

    for (const QJsonValue &value : modules) {
        const QJsonObject module = value.toObject();
        const QString name = module.value(QStringLiteral("name")).toString().trimmed();
        const QString key = name.toLower();
        if (name.isEmpty() || m_modules.contains(key)) {
            m_errorString = QStringLiteral("Пустое или повторяющееся имя модуля в схеме: %1").arg(name);
            return false;
        }
        m_modules.insert(key, name);

        // Canonical names work automatically; only legacy ESP project names
        // need explicit aliases in the schema (not YAML module identifiers).
        const QJsonValue aliases = module.value(QStringLiteral("firmware_project_names"));
        if (!aliases.isUndefined() && !aliases.isArray()) {
            m_errorString = QStringLiteral("firmware_project_names должен быть массивом: %1").arg(name);
            return false;
        }
        QJsonArray projects = aliases.toArray();
        projects.append(name);
        for (const QJsonValue &project : projects) {
            const QString projectKey = project.toString().trimmed().toLower();
            if (projectKey.isEmpty()
                || (m_projectModules.contains(projectKey) && m_projectModules.value(projectKey) != name)) {
                m_errorString = QStringLiteral("Пустое или неоднозначное имя проекта в схеме: %1").arg(projectKey);
                return false;
            }
            m_projectModules.insert(projectKey, name);
        }
    }
    return true;
}

QString firmwareAnalyzer::moduleFromProjectName(const QString &projectName) const
{
    return m_projectModules.value(projectName.trimmed().toLower());
}

QString firmwareAnalyzer::moduleFromFileName(const QString &filePath) const
{
    QString fileName = QFileInfo(filePath).fileName();

    if (!fileName.endsWith(QStringLiteral(".bin.xz"), Qt::CaseInsensitive))
        return {};
    fileName.chop(7);
    return m_modules.value(fileName.toLower());
}

QString firmwareAnalyzer::getGitHeadHash(const QString &repositoryPath)
{
    QDir dir(repositoryPath);
    QString gitPath;

    // Поднимаемся вверх по дереву каталогов в поиске папки .git
    while (dir.exists() && !dir.isRoot()) {
        if (dir.exists(QStringLiteral(".git"))) {
            gitPath = dir.filePath(QStringLiteral(".git"));
            break;
        }
        if (!dir.cdUp()) {
            break;
        }
    }

    if (gitPath.isEmpty())
        return QString(); // Репозиторий Git не найден

    // Если .git — это файл (например, в git-субмодулях), парсим путь из него
    QFileInfo gitInfo(gitPath);
    if (gitInfo.isFile()) {
        QFile gitFile(gitPath);
        if (gitFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromLatin1(gitFile.readAll()).trimmed();
            if (content.startsWith(QStringLiteral("gitdir: "))) {
                QString relPath = content.mid(8);
                QDir baseDir(gitInfo.absolutePath());
                gitPath = baseDir.absoluteFilePath(relPath);
            }
        }
    }

    // 1. Читаем файл .git/HEAD
    QFile headFile(gitPath + QStringLiteral("/HEAD"));
    if (!headFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QString headContent = QString::fromLatin1(headFile.readAll()).trimmed();
    headFile.close();

    // HEAD может содержать ссылку на ветку ("ref: refs/heads/master") или прямой SHA-1 (Detached HEAD)
    if (headContent.startsWith(QStringLiteral("ref: "))) {
        QString refPath = headContent.mid(5); // Получаем "refs/heads/master"

        // 2. Читаем файл, на который указывает ссылка (например, .git/refs/heads/master)
        QFile refFile(gitPath + QStringLiteral("/") + refPath);
        if (!refFile.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();

        QString hash = QString::fromLatin1(refFile.readAll()).trimmed();
        return hash.left(7); // Возвращаем первые 7 символов
    }

    // Если это Detached HEAD, там уже лежит чистый хэш
    if (headContent.length() >= 40) {
        return headContent.left(7);
    }

    return QString();
}

