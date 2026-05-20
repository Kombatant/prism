#pragma once

#include <QJsonObject>
#include <QString>

class GlslEffectInstaller
{
public:
    enum class Category {
        OpenClose,
        MinimizeRestore,
        Maximize,
        FullScreen,
    };

    struct CategorySpec {
        Category category;
        QString id;
        QString displayName;
        QString metadataCategory;
        QString exclusiveGroup;
        QString firstShaderLabel;
        QString secondShaderLabel;
    };

    struct InstallRequest {
        QString effectName;
        Category category = Category::OpenClose;
        QString firstShaderPath;
        QString secondShaderPath;
        QString kdlPath;
        bool reuseSingleShaderForBoth = false;
    };

    struct InstallResult {
        bool ok = false;
        QString packageId;
        QString packagePath;
        QString message;
    };

    struct InstalledEffect {
        QString packageId;
        QString name;
        QString category;
        QString sourceKind;
        QString packagePath;
        bool enabled = false;
        int sizeKiB = 0;
    };

    static QList<CategorySpec> categories();
    static CategorySpec categorySpec(Category category);

    InstallResult installEffect(const InstallRequest &request) const;
    QList<InstalledEffect> installedEffects() const;
    bool removeInstalledEffect(const QString &packageId, QString *errorMessage = nullptr) const;

private:
    struct KdlImport {
        bool ok = false;
        QString openShaderSource;
        QString closeShaderSource;
        int durationMs = 250;
        QString error;
    };

    QString sanitizeToken(const QString &value) const;
    QString readTextFile(const QString &path) const;
    bool writeTextFile(const QString &path, const QString &contents) const;
    bool copyFileReplacing(const QString &sourcePath, const QString &targetPath) const;
    KdlImport parseKdlFile(const QString &path) const;
    QString convertShaderSource(const QString &sourceCode, const QString &entryPoint, bool coreVariant) const;
    QString packageRoot() const;
    QString buildPackageId(const InstallRequest &request) const;
    QString buildMetadata(const InstallRequest &request, const QString &packageId) const;
    QString buildManifest(const InstallRequest &request, const QString &packageId, int defaultDurationMs, const QString &sourceKind) const;
    QString buildMainScript(Category category, int defaultDurationMs) const;
    QString buildConfigXml(int defaultDurationMs) const;
    QString buildConfigUi() const;
    QJsonObject readJsonObjectFile(const QString &path) const;
    bool reloadKWin(QString *errorMessage) const;
};
