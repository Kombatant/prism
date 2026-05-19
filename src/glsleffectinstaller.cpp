#include "glsleffectinstaller.h"

#include <algorithm>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QRegularExpression>
#include <QStandardPaths>

namespace
{
QString shaderEntryPoint(bool firstShader)
{
    return firstShader ? QStringLiteral("open_color") : QStringLiteral("close_color");
}

QString openCloseScript()
{
    return QStringLiteral(R"JS(
"use strict";

function print(message) {
    console.error("prism: " + message);
}

var importedOpenClose = {
    duration: animationTime(250),
    preserveBlur: false,
    openShaderId: 0,
    closeShaderId: 0,
    hashString: function (value) {
        var hash = 2166136261;
        for (var index = 0; index < value.length; ++index) {
            hash = ((hash << 5) - hash) + value.charCodeAt(index);
            hash |= 0;
        }
        return ((hash >>> 0) % 10000) / 10000.0;
    },
    loadConfig: function () {
        importedOpenClose.duration = animationTime(effect.readConfig("Duration", %1));
        importedOpenClose.preserveBlur = effect.readConfig("PreserveBlur", false);
        print("config duration=" + importedOpenClose.duration + " preserveBlur=" + importedOpenClose.preserveBlur);
    },
    shouldAnimateWindow: function (window) {
        if (window.windowClass === "plasmashell plasmashell" ||
            window.windowClass === "plasmashell org.kde.plasmashell") {
            return window.hasDecoration;
        }

        if (!window.hasDecoration && window.onAllDesktops) {
            return false;
        }

        if (window.popupWindow || window.lockScreen || window.outline) {
            return false;
        }

        if (!window.managed) {
            return false;
        }

        return window.normalWindow || window.dialog || window.hasDecoration;
    },
    setupForcedRoles: function (window) {
        if (importedOpenClose.preserveBlur) {
            window.setData(Effect.WindowForceBackgroundContrastRole, true);
            window.setData(Effect.WindowForceBlurRole, true);
        } else {
            window.setData(Effect.WindowForceBackgroundContrastRole, false);
            window.setData(Effect.WindowForceBlurRole, false);
        }
    },
    cleanupForcedRoles: function (window) {
        if (!window) {
            return;
        }
        window.setData(Effect.WindowForceBackgroundContrastRole, null);
        window.setData(Effect.WindowForceBlurRole, null);
    },
    setCommonUniforms: function (shaderId, seedValue, forOpening, isFullscreen) {
        if (!shaderId || typeof effect.setUniform !== "function") {
            return;
        }

        try {
            effect.setUniform(shaderId, "uForOpening", forOpening ? 1.0 : 0.0);
        } catch (error) {}

        try {
            effect.setUniform(shaderId, "uIsFullscreen", isFullscreen ? 1.0 : 0.0);
        } catch (error) {}

        try {
            effect.setUniform(shaderId, "uDuration", importedOpenClose.duration / 1000.0);
        } catch (error) {}

        try {
            effect.setUniform(shaderId, "uSeed", importedOpenClose.hashString(seedValue));
        } catch (error) {
            print("failed to set uSeed: " + error);
        }
    },
    loadShaders: function () {
        importedOpenClose.openShaderId = 0;
        importedOpenClose.closeShaderId = 0;

        if (typeof effect.addFragmentShader !== "function" || typeof Effect.MapTexture === "undefined") {
            print("shader API unavailable");
            return;
        }

        try {
            importedOpenClose.openShaderId = effect.addFragmentShader(Effect.MapTexture, "open.frag");
            importedOpenClose.closeShaderId = effect.addFragmentShader(Effect.MapTexture, "close.frag");
            print("loaded shaders open=" + importedOpenClose.openShaderId + " close=" + importedOpenClose.closeShaderId);
        } catch (error) {
            print("shader load failed: " + error);
            importedOpenClose.openShaderId = 0;
            importedOpenClose.closeShaderId = 0;
        }
    },
    shaderUniformAnimation: function (shaderId) {
        if (!shaderId) {
            return [];
        }

        return [{
            type: Effect.ShaderUniform,
            fragmentShader: shaderId,
            uniform: "uProgress",
            from: 0.0,
            to: 1.0
        }];
    },
    slotWindowAdded: function (window) {
        if (effects.hasActiveFullScreenEffect || !importedOpenClose.shouldAnimateWindow(window)) {
            return;
        }
        if (!window.visible || effect.isGrabbed(window, Effect.WindowAddedGrabRole)) {
            return;
        }

        importedOpenClose.setupForcedRoles(window);
        importedOpenClose.setCommonUniforms(importedOpenClose.openShaderId, "open", true, window.fullScreen);
        print("windowAdded shader=" + importedOpenClose.openShaderId + " caption=" + window.caption);

        var animations = importedOpenClose.shaderUniformAnimation(importedOpenClose.openShaderId);
        if (animations.length === 0) {
            print("windowAdded produced no shader animation");
            importedOpenClose.cleanupForcedRoles(window);
            return;
        }

        window.importedOpenCloseOpenAnimation = animate({
            window: window,
            curve: QEasingCurve.Linear,
            duration: importedOpenClose.duration,
            animations: animations
        });
    },
    slotWindowClosed: function (window) {
        if (effects.hasActiveFullScreenEffect || !importedOpenClose.shouldAnimateWindow(window)) {
            return;
        }
        if (!window.visible || window.skipsCloseAnimation || effect.isGrabbed(window, Effect.WindowClosedGrabRole)) {
            return;
        }

        if (window.importedOpenCloseOpenAnimation) {
            cancel(window.importedOpenCloseOpenAnimation);
            delete window.importedOpenCloseOpenAnimation;
        }

        importedOpenClose.setupForcedRoles(window);
        importedOpenClose.setCommonUniforms(importedOpenClose.closeShaderId, "close", false, window.fullScreen);
        print("windowClosed shader=" + importedOpenClose.closeShaderId + " caption=" + window.caption);

        var animations = importedOpenClose.shaderUniformAnimation(importedOpenClose.closeShaderId);
        if (animations.length === 0) {
            print("windowClosed produced no shader animation");
            importedOpenClose.cleanupForcedRoles(window);
            return;
        }

        window.importedOpenCloseCloseAnimation = animate({
            window: window,
            curve: QEasingCurve.Linear,
            duration: importedOpenClose.duration,
            animations: animations
        });
    },
    slotWindowDataChanged: function (window, role) {
        if (role === Effect.WindowAddedGrabRole && window.importedOpenCloseOpenAnimation && effect.isGrabbed(window, role)) {
            cancel(window.importedOpenCloseOpenAnimation);
            delete window.importedOpenCloseOpenAnimation;
            importedOpenClose.cleanupForcedRoles(window);
        } else if (role === Effect.WindowClosedGrabRole && window.importedOpenCloseCloseAnimation && effect.isGrabbed(window, role)) {
            cancel(window.importedOpenCloseCloseAnimation);
            delete window.importedOpenCloseCloseAnimation;
            importedOpenClose.cleanupForcedRoles(window);
        }
    },
    init: function () {
        effect.configChanged.connect(importedOpenClose.loadConfig);
        effect.animationEnded.connect(importedOpenClose.cleanupForcedRoles);
        effects.windowAdded.connect(importedOpenClose.slotWindowAdded);
        effects.windowClosed.connect(importedOpenClose.slotWindowClosed);
        effects.windowDataChanged.connect(importedOpenClose.slotWindowDataChanged);
        print("initializing open/close effect");
        importedOpenClose.loadConfig();
        importedOpenClose.loadShaders();
    }
};

importedOpenClose.init();
)JS");
}

QString minimizeRestoreScript()
{
    return QStringLiteral(R"JS(
"use strict";

function print(message) {
    console.error("prism-minimize: " + message);
}

var importedMinimizeRestore = {
    duration: animationTime(250),
    preserveBlur: false,
    restoreShaderId: 0,
    minimizeShaderId: 0,
    hashString: function (value) {
        var hash = 2166136261;
        for (var index = 0; index < value.length; ++index) {
            hash = ((hash << 5) - hash) + value.charCodeAt(index);
            hash |= 0;
        }
        return ((hash >>> 0) % 10000) / 10000.0;
    },
    loadConfig: function () {
        importedMinimizeRestore.duration = animationTime(effect.readConfig("Duration", %1));
        importedMinimizeRestore.preserveBlur = effect.readConfig("PreserveBlur", false);
        print("config duration=" + importedMinimizeRestore.duration + " preserveBlur=" + importedMinimizeRestore.preserveBlur);
    },
    shouldAnimateWindow: function (window) {
        if (window.windowClass === "plasmashell plasmashell" ||
            window.windowClass === "plasmashell org.kde.plasmashell") {
            return window.hasDecoration;
        }

        if (!window.hasDecoration && window.onAllDesktops) {
            return false;
        }

        if (window.popupWindow || window.lockScreen || window.outline) {
            return false;
        }

        if (!window.managed) {
            return false;
        }

        return window.normalWindow || window.dialog || window.hasDecoration;
    },
    setupForcedRoles: function (window) {
        if (importedMinimizeRestore.preserveBlur) {
            window.setData(Effect.WindowForceBackgroundContrastRole, true);
            window.setData(Effect.WindowForceBlurRole, true);
        } else {
            window.setData(Effect.WindowForceBackgroundContrastRole, false);
            window.setData(Effect.WindowForceBlurRole, false);
        }
    },
    cleanupForcedRoles: function (window) {
        if (!window) {
            return;
        }
        window.setData(Effect.WindowForceBackgroundContrastRole, null);
        window.setData(Effect.WindowForceBlurRole, null);
    },
    setCommonUniforms: function (shaderId, seedValue, forOpening, isFullscreen) {
        if (!shaderId || typeof effect.setUniform !== "function") {
            return;
        }

        try {
            effect.setUniform(shaderId, "uForOpening", forOpening ? 1.0 : 0.0);
        } catch (error) {}

        try {
            effect.setUniform(shaderId, "uIsFullscreen", isFullscreen ? 1.0 : 0.0);
        } catch (error) {}

        try {
            effect.setUniform(shaderId, "uDuration", importedMinimizeRestore.duration / 1000.0);
        } catch (error) {}

        try {
            effect.setUniform(shaderId, "uSeed", importedMinimizeRestore.hashString(seedValue));
        } catch (error) {
            print("failed to set uSeed: " + error);
        }
    },
    loadShaders: function () {
        importedMinimizeRestore.restoreShaderId = 0;
        importedMinimizeRestore.minimizeShaderId = 0;

        if (typeof effect.addFragmentShader !== "function" || typeof Effect.MapTexture === "undefined") {
            print("shader API unavailable");
            return;
        }

        try {
            importedMinimizeRestore.restoreShaderId = effect.addFragmentShader(Effect.MapTexture, "open.frag");
            importedMinimizeRestore.minimizeShaderId = effect.addFragmentShader(Effect.MapTexture, "close.frag");
            print("loaded shaders restore=" + importedMinimizeRestore.restoreShaderId + " minimize=" + importedMinimizeRestore.minimizeShaderId);
        } catch (error) {
            print("shader load failed: " + error);
            importedMinimizeRestore.restoreShaderId = 0;
            importedMinimizeRestore.minimizeShaderId = 0;
        }
    },
    shaderUniformAnimation: function (shaderId) {
        if (!shaderId) {
            return [];
        }

        return [{
            type: Effect.ShaderUniform,
            fragmentShader: shaderId,
            uniform: "uProgress",
            from: 0.0,
            to: 1.0
        }];
    },
    stopAnimation: function (window, key) {
        if (!window[key]) {
            return;
        }

        cancel(window[key]);
        delete window[key];
    },
    slotWindowMinimized: function (window) {
        if (effects.hasActiveFullScreenEffect || !importedMinimizeRestore.shouldAnimateWindow(window)) {
            return;
        }

        importedMinimizeRestore.stopAnimation(window, "importedRestoreAnimation");
        importedMinimizeRestore.setupForcedRoles(window);
        importedMinimizeRestore.setCommonUniforms(importedMinimizeRestore.minimizeShaderId, "minimize", false, window.fullScreen);
        print("windowMinimized shader=" + importedMinimizeRestore.minimizeShaderId + " caption=" + window.caption);

        var animations = importedMinimizeRestore.shaderUniformAnimation(importedMinimizeRestore.minimizeShaderId);
        if (animations.length === 0) {
            print("windowMinimized produced no shader animation");
            importedMinimizeRestore.cleanupForcedRoles(window);
            return;
        }

        window.importedMinimizeAnimation = animate({
            window: window,
            curve: QEasingCurve.Linear,
            duration: importedMinimizeRestore.duration,
            keepAlive: false,
            animations: animations
        });
    },
    slotWindowUnminimized: function (window) {
        if (effects.hasActiveFullScreenEffect || !importedMinimizeRestore.shouldAnimateWindow(window)) {
            return;
        }

        importedMinimizeRestore.stopAnimation(window, "importedMinimizeAnimation");
        importedMinimizeRestore.setupForcedRoles(window);
        importedMinimizeRestore.setCommonUniforms(importedMinimizeRestore.restoreShaderId, "restore", true, window.fullScreen);
        print("windowUnminimized shader=" + importedMinimizeRestore.restoreShaderId + " caption=" + window.caption);

        var animations = importedMinimizeRestore.shaderUniformAnimation(importedMinimizeRestore.restoreShaderId);
        if (animations.length === 0) {
            print("windowUnminimized produced no shader animation");
            importedMinimizeRestore.cleanupForcedRoles(window);
            return;
        }

        window.importedRestoreAnimation = animate({
            window: window,
            curve: QEasingCurve.Linear,
            duration: importedMinimizeRestore.duration,
            keepAlive: false,
            animations: animations
        });
    },
    slotWindowAdded: function (window) {
        if (!importedMinimizeRestore.shouldAnimateWindow(window) || window.importedMinimizeRestoreTracked) {
            return;
        }

        window.importedMinimizeRestoreTracked = true;
        window.minimizedChanged.connect(() => {
            if (window.minimized) {
                importedMinimizeRestore.slotWindowMinimized(window);
            } else {
                importedMinimizeRestore.slotWindowUnminimized(window);
            }
        });
    },
    init: function () {
        effect.configChanged.connect(importedMinimizeRestore.loadConfig);
        effect.animationEnded.connect(importedMinimizeRestore.cleanupForcedRoles);
        effects.windowAdded.connect(importedMinimizeRestore.slotWindowAdded);
        for (const window of effects.stackingOrder) {
            importedMinimizeRestore.slotWindowAdded(window);
        }
        print("initializing minimize/restore effect");
        importedMinimizeRestore.loadConfig();
        importedMinimizeRestore.loadShaders();
    }
};

importedMinimizeRestore.init();
)JS");
}
}

QList<GlslEffectInstaller::CategorySpec> GlslEffectInstaller::categories()
{
    return {
        {
            .category = Category::OpenClose,
            .id = QStringLiteral("open-close"),
            .displayName = QStringLiteral("Window Open/Close Animation"),
            .metadataCategory = QStringLiteral("Window Open/Close Animation"),
            .exclusiveGroup = QStringLiteral("toplevel-open-close-animation"),
            .firstShaderLabel = QStringLiteral("Open Shader"),
            .secondShaderLabel = QStringLiteral("Close Shader"),
        },
        {
            .category = Category::MinimizeRestore,
            .id = QStringLiteral("minimize-restore"),
            .displayName = QStringLiteral("Minimize/Restore Animation"),
            .metadataCategory = QStringLiteral("Minimize/Restore Animation"),
            .exclusiveGroup = QStringLiteral("minimize"),
            .firstShaderLabel = QStringLiteral("Restore Shader"),
            .secondShaderLabel = QStringLiteral("Minimize Shader"),
        },
    };
}

GlslEffectInstaller::CategorySpec GlslEffectInstaller::categorySpec(Category category)
{
    for (const CategorySpec &spec : categories()) {
        if (spec.category == category) {
            return spec;
        }
    }

    return categories().constFirst();
}

GlslEffectInstaller::InstallResult GlslEffectInstaller::installEffect(const InstallRequest &request) const
{
    InstallResult result;

    if (request.effectName.trimmed().isEmpty()) {
        result.message = QStringLiteral("Effect name is required.");
        return result;
    }
    QString firstShaderSource;
    QString secondShaderSource;
    QString sourceKind = QStringLiteral("glsl");
    int defaultDurationMs = 250;

    const QString kdlPath = !request.kdlPath.trimmed().isEmpty()
        ? request.kdlPath.trimmed()
        : (request.firstShaderPath.endsWith(QStringLiteral(".kdl"), Qt::CaseInsensitive) ? request.firstShaderPath.trimmed() : QString());

    if (!kdlPath.isEmpty()) {
        const KdlImport kdlImport = parseKdlFile(kdlPath);
        if (!kdlImport.ok) {
            result.message = kdlImport.error;
            return result;
        }
        firstShaderSource = kdlImport.openShaderSource;
        secondShaderSource = kdlImport.closeShaderSource;
        defaultDurationMs = kdlImport.durationMs;
        sourceKind = QStringLiteral("kdl");
    } else {
        if (request.firstShaderPath.trimmed().isEmpty()) {
            result.message = QStringLiteral("Provide a .kdl file or at least the first shader file.");
            return result;
        }

        firstShaderSource = readTextFile(request.firstShaderPath);
        if (firstShaderSource.isEmpty()) {
            result.message = QStringLiteral("Failed to read the first shader file.");
            return result;
        }

        if (request.reuseSingleShaderForBoth && request.secondShaderPath.trimmed().isEmpty()) {
            secondShaderSource = firstShaderSource;
        } else {
            if (request.secondShaderPath.trimmed().isEmpty()) {
                result.message = QStringLiteral("Provide the second shader file or enable reuse of the first shader for both effects.");
                return result;
            }
            secondShaderSource = readTextFile(request.secondShaderPath);
        }

        if (secondShaderSource.isEmpty()) {
            result.message = QStringLiteral("Failed to read the second shader file.");
            return result;
        }
    }

    const QString convertedFirst = convertShaderSource(firstShaderSource, shaderEntryPoint(true), false);
    const QString convertedFirstCore = convertShaderSource(firstShaderSource, shaderEntryPoint(true), true);
    const QString convertedSecond = convertShaderSource(secondShaderSource, shaderEntryPoint(false), false);
    const QString convertedSecondCore = convertShaderSource(secondShaderSource, shaderEntryPoint(false), true);
    if (convertedFirst.isEmpty() || convertedFirstCore.isEmpty() || convertedSecond.isEmpty() || convertedSecondCore.isEmpty()) {
        result.message = QStringLiteral("One or both shaders are missing the expected entry points (open_color / close_color).");
        return result;
    }

    const QString packageId = buildPackageId(request);
    const QString targetDirPath = packageRoot() + QLatin1Char('/') + packageId;
    QDir targetDir(targetDirPath);
    if (targetDir.exists() && !targetDir.removeRecursively()) {
        result.message = QStringLiteral("Failed to replace existing package at %1.").arg(targetDirPath);
        return result;
    }

    const QString codeDir = targetDirPath + QStringLiteral("/contents/code");
    const QString configDir = targetDirPath + QStringLiteral("/contents/config");
    const QString shaderDir = targetDirPath + QStringLiteral("/contents/shaders");
    const QString uiDir = targetDirPath + QStringLiteral("/contents/ui");
    const QString rawDir = targetDirPath + QStringLiteral("/contents/raw");
    if (!QDir().mkpath(codeDir) || !QDir().mkpath(configDir) || !QDir().mkpath(shaderDir) || !QDir().mkpath(uiDir) || !QDir().mkpath(rawDir)) {
        result.message = QStringLiteral("Failed to create package directories.");
        return result;
    }

    const bool rawInputsWritten = !kdlPath.isEmpty()
        ? writeTextFile(rawDir + QStringLiteral("/source.kdl"), readTextFile(kdlPath))
        : (copyFileReplacing(request.firstShaderPath, rawDir + QStringLiteral("/first.raw.glsl")) &&
           (request.reuseSingleShaderForBoth && request.secondShaderPath.trimmed().isEmpty()
                ? copyFileReplacing(request.firstShaderPath, rawDir + QStringLiteral("/second.raw.glsl"))
                : copyFileReplacing(request.secondShaderPath, rawDir + QStringLiteral("/second.raw.glsl"))));

    if (!rawInputsWritten ||
        !writeTextFile(targetDirPath + QStringLiteral("/metadata.json"), buildMetadata(request, packageId)) ||
        !writeTextFile(targetDirPath + QStringLiteral("/import.json"), buildManifest(request, packageId, defaultDurationMs, sourceKind)) ||
        !writeTextFile(codeDir + QStringLiteral("/main.js"), buildMainScript(request.category, defaultDurationMs)) ||
        !writeTextFile(configDir + QStringLiteral("/main.xml"), buildConfigXml(defaultDurationMs)) ||
        !writeTextFile(uiDir + QStringLiteral("/config.ui"), buildConfigUi()) ||
        !writeTextFile(shaderDir + QStringLiteral("/open.frag"), convertedFirst) ||
        !writeTextFile(shaderDir + QStringLiteral("/open_core.frag"), convertedFirstCore) ||
        !writeTextFile(shaderDir + QStringLiteral("/close.frag"), convertedSecond) ||
        !writeTextFile(shaderDir + QStringLiteral("/close_core.frag"), convertedSecondCore)) {
        result.message = QStringLiteral("Failed to write generated KWin effect files.");
        return result;
    }

    QString reloadError;
    if (!reloadKWin(&reloadError)) {
        result.ok = true;
        result.packageId = packageId;
        result.packagePath = targetDirPath;
        result.message = QStringLiteral("Installed, but KWin reload failed: %1").arg(reloadError);
        return result;
    }

    result.ok = true;
    result.packageId = packageId;
    result.packagePath = targetDirPath;
    result.message = QStringLiteral("Installed %1. It should now appear in System Settings -> Desktop Effects / Animations.").arg(request.effectName.trimmed());
    return result;
}

QList<GlslEffectInstaller::InstalledEffect> GlslEffectInstaller::installedEffects() const
{
    QList<InstalledEffect> effects;

    QDir root(packageRoot());
    const QStringList packageDirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &packageDir : packageDirs) {
        if (!packageDir.startsWith(QStringLiteral("kwin6_effect_glsl_"))) {
            continue;
        }

        const QString packagePath = root.filePath(packageDir);
        const QJsonObject metadataObject = readJsonObjectFile(packagePath + QStringLiteral("/metadata.json"));
        const QJsonObject importObject = readJsonObjectFile(packagePath + QStringLiteral("/import.json"));
        const QJsonObject pluginObject = metadataObject.value(QStringLiteral("KPlugin")).toObject();

        InstalledEffect effect;
        effect.packageId = pluginObject.value(QStringLiteral("Id")).toString(packageDir);
        effect.name = pluginObject.value(QStringLiteral("Name")).toString(packageDir);
        effect.category = pluginObject.value(QStringLiteral("Category")).toString();
        effect.sourceKind = importObject.value(QStringLiteral("sourceKind")).toString(QStringLiteral("glsl"));
        effect.packagePath = packagePath;

        QDir dir(packagePath);
        const QFileInfoList fileInfos = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
        qint64 totalBytes = 0;
        QList<QFileInfo> pending = fileInfos;
        while (!pending.isEmpty()) {
            const QFileInfo info = pending.takeLast();
            if (info.isDir()) {
                const QFileInfoList nested = QDir(info.filePath()).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
                for (const QFileInfo &nestedInfo : nested) {
                    pending.append(nestedInfo);
                }
            } else {
                totalBytes += info.size();
            }
        }
        effect.sizeKiB = qMax(1, static_cast<int>((totalBytes + 1023) / 1024));

        QSettings kwinSettings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kwinrc"), QSettings::IniFormat);
        kwinSettings.beginGroup(QStringLiteral("Plugins"));
        effect.enabled = kwinSettings.value(effect.packageId + QStringLiteral("Enabled"), false).toBool();
        kwinSettings.endGroup();

        effects.append(effect);
    }

    std::sort(effects.begin(), effects.end(), [](const InstalledEffect &left, const InstalledEffect &right) {
        return left.name.toLower() < right.name.toLower();
    });
    return effects;
}

bool GlslEffectInstaller::removeInstalledEffect(const QString &packageId, QString *errorMessage) const
{
    if (packageId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Missing effect package id.");
        }
        return false;
    }

    const QString packagePath = packageRoot() + QLatin1Char('/') + packageId;
    QDir targetDir(packagePath);
    if (!targetDir.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Effect package was not found on disk.");
        }
        return false;
    }

    QSettings kwinSettings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kwinrc"), QSettings::IniFormat);
    kwinSettings.beginGroup(QStringLiteral("Plugins"));
    const bool enabled = kwinSettings.value(packageId + QStringLiteral("Enabled"), false).toBool();
    kwinSettings.endGroup();
    if (enabled) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Disable the effect in KWin before removing it.");
        }
        return false;
    }

    if (!targetDir.removeRecursively()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to remove %1.").arg(packagePath);
        }
        return false;
    }

    kwinSettings.beginGroup(QStringLiteral("Plugins"));
    kwinSettings.remove(packageId + QStringLiteral("Enabled"));
    kwinSettings.endGroup();
    kwinSettings.remove(QStringLiteral("Effect-") + packageId);
    kwinSettings.sync();

    QString reloadError;
    if (!reloadKWin(&reloadError) && errorMessage) {
        *errorMessage = QStringLiteral("Effect removed, but KWin reload failed: %1").arg(reloadError);
    }

    return true;
}

GlslEffectInstaller::KdlImport GlslEffectInstaller::parseKdlFile(const QString &path) const
{
    const QString source = readTextFile(path);
    if (source.isEmpty()) {
        return {.ok = false, .error = QStringLiteral("Failed to read KDL file.")};
    }

    const QRegularExpression openShaderRegex(
        QStringLiteral(R"REGEX(window-open\s*\{[\s\S]*?custom-shader\s+r"([\s\S]*?)")REGEX"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression closeShaderRegex(
        QStringLiteral(R"REGEX(window-close\s*\{[\s\S]*?custom-shader\s+r"([\s\S]*?)")REGEX"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression durationRegex(
        QStringLiteral(R"(window-open\s*\{[\s\S]*?duration-ms\s+(\d+))"),
        QRegularExpression::DotMatchesEverythingOption);

    const QRegularExpressionMatch openMatch = openShaderRegex.match(source);
    const QRegularExpressionMatch closeMatch = closeShaderRegex.match(source);
    if (!openMatch.hasMatch() || !closeMatch.hasMatch()) {
        return {.ok = false, .error = QStringLiteral("KDL import currently requires window-open and window-close custom-shader blocks.")};
    }

    int durationMs = 250;
    const QRegularExpressionMatch durationMatch = durationRegex.match(source);
    if (durationMatch.hasMatch()) {
        bool ok = false;
        const int parsedDuration = durationMatch.captured(1).toInt(&ok);
        if (ok && parsedDuration > 0) {
            durationMs = parsedDuration;
        }
    }

    return {
        .ok = true,
        .openShaderSource = openMatch.captured(1).trimmed(),
        .closeShaderSource = closeMatch.captured(1).trimmed(),
        .durationMs = durationMs,
    };
}

QString GlslEffectInstaller::sanitizeToken(const QString &value) const
{
    QString sanitized = value.toLower();
    sanitized.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    sanitized.remove(QRegularExpression(QStringLiteral("(^-+|-+$)")));
    if (sanitized.isEmpty()) {
        sanitized = QStringLiteral("imported");
    }
    return sanitized;
}

QString GlslEffectInstaller::readTextFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool GlslEffectInstaller::writeTextFile(const QString &path, const QString &contents) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(contents.toUtf8());
    return true;
}

bool GlslEffectInstaller::copyFileReplacing(const QString &sourcePath, const QString &targetPath) const
{
    QFile::remove(targetPath);
    return QFile::copy(sourcePath, targetPath);
}

QString GlslEffectInstaller::convertShaderSource(const QString &sourceCode, const QString &entryPoint, bool coreVariant) const
{
    if (sourceCode.trimmed().isEmpty() || !sourceCode.contains(entryPoint)) {
        return {};
    }

    const QString preamble = coreVariant
        ? QStringLiteral(
              "#version 140\n"
              "#define KWIN\n"
              "#define PLASMA6\n"
              "\n"
              "uniform bool uForOpening;\n"
              "uniform bool uIsFullscreen;\n"
              "uniform float uProgress;\n"
              "uniform float uDuration;\n"
              "uniform float uSeed;\n"
              "\n"
              "#if defined(PLASMA6)\n"
              "#include \"colormanagement.glsl\"\n"
              "#endif\n"
              "\n"
              "uniform sampler2D sampler;\n"
              "uniform int textureWidth;\n"
              "uniform int textureHeight;\n"
              "in vec2 texcoord0;\n"
              "out vec4 fragColor;\n"
              "\n"
              "vec2 uSize = vec2(textureWidth, textureHeight);\n"
              "vec2 iTexCoord = vec2(texcoord0.x, 1.0 - texcoord0.y);\n"
              "\n"
              "vec4 niriTexture2D(sampler2D tex, vec2 uv)\n"
              "{\n"
              "    vec4 color = texture(tex, vec2(uv.x, 1.0 - uv.y));\n"
              "    if (color.a > 0.0) {\n"
              "        color.rgb /= color.a;\n"
              "    }\n"
              "    return color;\n"
              "}\n"
              "\n"
              "vec4 getInputColor(vec2 coords)\n"
              "{\n"
              "    vec4 color = niriTexture2D(sampler, coords);\n"
              "    if (color.a > 0.0) {\n"
              "        color.rgb /= color.a;\n"
              "    }\n"
              "    return color;\n"
              "}\n"
              "\n"
              "void setOutputColor(vec4 outColor)\n"
              "{\n"
              "    if (outColor.a > 1000.0 && uForOpening && uIsFullscreen) {\n"
              "        outColor = vec4(0.0);\n"
              "    }\n"
              "    fragColor = vec4(outColor.rgb * outColor.a, outColor.a);\n"
              "#if defined(PLASMA6)\n"
              "    fragColor = sourceEncodingToNitsInDestinationColorspace(fragColor);\n"
              "    fragColor = nitsToDestinationEncoding(fragColor);\n"
              "#endif\n"
              "}\n"
              "\n"
              "#define niri_clamped_progress clamp(uProgress, 0.0, 1.0)\n"
              "#define niri_random_seed uSeed\n"
              "#define niri_tex sampler\n"
              "const mat3 niri_geo_to_tex = mat3(\n"
              "    vec3(1.0, 0.0, 0.0),\n"
              "    vec3(0.0, -1.0, 0.0),\n"
              "    vec3(0.0, 1.0, 1.0)\n"
              ");\n"
              "\n")
        : QStringLiteral(
              "#define KWIN_LEGACY\n"
              "#define PLASMA6\n"
              "\n"
              "uniform bool uForOpening;\n"
              "uniform bool uIsFullscreen;\n"
              "uniform float uProgress;\n"
              "uniform float uDuration;\n"
              "uniform float uSeed;\n"
              "\n"
              "#if defined(PLASMA6)\n"
              "#include \"colormanagement.glsl\"\n"
              "#endif\n"
              "\n"
              "uniform sampler2D sampler;\n"
              "uniform int textureWidth;\n"
              "uniform int textureHeight;\n"
              "varying vec2 texcoord0;\n"
              "\n"
              "vec2 uSize = vec2(textureWidth, textureHeight);\n"
              "vec2 iTexCoord = vec2(texcoord0.x, 1.0 - texcoord0.y);\n"
              "\n"
              "vec4 niriTexture2D(sampler2D tex, vec2 uv)\n"
              "{\n"
              "    vec4 color = texture2D(tex, vec2(uv.x, 1.0 - uv.y));\n"
              "    if (color.a > 0.0) {\n"
              "        color.rgb /= color.a;\n"
              "    }\n"
              "    return color;\n"
              "}\n"
              "\n"
              "vec4 getInputColor(vec2 coords)\n"
              "{\n"
              "    vec4 color = niriTexture2D(sampler, coords);\n"
              "    if (color.a > 0.0) {\n"
              "        color.rgb /= color.a;\n"
              "    }\n"
              "    return color;\n"
              "}\n"
              "\n"
              "void setOutputColor(vec4 outColor)\n"
              "{\n"
              "    if (outColor.a > 1000.0 && uForOpening && uIsFullscreen) {\n"
              "        outColor = vec4(0.0);\n"
              "    }\n"
              "    gl_FragColor = vec4(outColor.rgb * outColor.a, outColor.a);\n"
              "#if defined(PLASMA6)\n"
              "    gl_FragColor = sourceEncodingToNitsInDestinationColorspace(gl_FragColor);\n"
              "    gl_FragColor = nitsToDestinationEncoding(gl_FragColor);\n"
              "#endif\n"
              "}\n"
              "\n"
              "#define niri_clamped_progress clamp(uProgress, 0.0, 1.0)\n"
              "#define niri_random_seed uSeed\n"
              "#define niri_tex sampler\n"
              "const mat3 niri_geo_to_tex = mat3(\n"
              "    vec3(1.0, 0.0, 0.0),\n"
              "    vec3(0.0, -1.0, 0.0),\n"
              "    vec3(0.0, 1.0, 1.0)\n"
              ");\n"
              "\n");

    return preamble + sourceCode
        + QStringLiteral(
              "\n"
              "void main()\n"
              "{\n"
              "    vec3 coords_geo = vec3(iTexCoord, 0.0);\n"
              "    vec3 size_geo = vec3(uSize, 1.0);\n"
              "    setOutputColor(%1(coords_geo, size_geo));\n"
              "}\n")
              .arg(entryPoint);
}

QString GlslEffectInstaller::packageRoot() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/kwin/effects");
}

QString GlslEffectInstaller::buildPackageId(const InstallRequest &request) const
{
    const CategorySpec spec = categorySpec(request.category);
    return QStringLiteral("kwin6_effect_glsl_%1_%2")
        .arg(spec.id, sanitizeToken(request.effectName));
}

QString GlslEffectInstaller::buildMetadata(const InstallRequest &request, const QString &packageId) const
{
    const CategorySpec spec = categorySpec(request.category);

    QJsonObject pluginObject;
    pluginObject.insert(QStringLiteral("Id"), packageId);
    pluginObject.insert(QStringLiteral("Name"), request.effectName.trimmed());
    pluginObject.insert(QStringLiteral("Description"), QStringLiteral("Imported shader animation installed by Prism"));
    pluginObject.insert(QStringLiteral("Category"), spec.metadataCategory);
    pluginObject.insert(QStringLiteral("EnabledByDefault"), false);
    pluginObject.insert(QStringLiteral("Icon"), QStringLiteral("preferences-system-windows"));
    pluginObject.insert(QStringLiteral("License"), QStringLiteral("GPL-3.0"));
    pluginObject.insert(QStringLiteral("Version"), QStringLiteral("1.0"));
    pluginObject.insert(QStringLiteral("ServiceTypes"), QJsonArray{QStringLiteral("KWin/Effect")});

    QJsonObject kwinEffectObject;
    kwinEffectObject.insert(QStringLiteral("exclusiveGroup"), spec.exclusiveGroup);

    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("KPackageStructure"), QStringLiteral("KWin/Effect"));
    rootObject.insert(QStringLiteral("KPlugin"), pluginObject);
    rootObject.insert(QStringLiteral("X-KDE-ConfigModule"), QStringLiteral("kcm_kwin4_genericscripted"));
    rootObject.insert(QStringLiteral("X-KDE-PluginKeyword"), packageId);
    rootObject.insert(QStringLiteral("X-Plasma-MainScript"), QStringLiteral("code/main.js"));
    rootObject.insert(QStringLiteral("X-KDE-Ordering"), 60);
    rootObject.insert(QStringLiteral("X-KWin-Exclusive-Category"), spec.exclusiveGroup);
    rootObject.insert(QStringLiteral("X-Plasma-API"), QStringLiteral("javascript"));
    rootObject.insert(QStringLiteral("org.kde.kwin.effect"), kwinEffectObject);
    return QString::fromUtf8(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
}

QString GlslEffectInstaller::buildManifest(const InstallRequest &request, const QString &packageId, int defaultDurationMs, const QString &sourceKind) const
{
    const CategorySpec spec = categorySpec(request.category);

    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("packageId"), packageId);
    rootObject.insert(QStringLiteral("name"), request.effectName.trimmed());
    rootObject.insert(QStringLiteral("category"), spec.displayName);
    rootObject.insert(QStringLiteral("sourceKind"), sourceKind);
    rootObject.insert(QStringLiteral("defaultDurationMs"), defaultDurationMs);
    if (!request.kdlPath.trimmed().isEmpty()) {
        rootObject.insert(QStringLiteral("kdlPath"), request.kdlPath.trimmed());
    } else {
        rootObject.insert(QStringLiteral("firstShaderPath"), request.firstShaderPath);
        rootObject.insert(QStringLiteral("secondShaderPath"), request.secondShaderPath);
    }
    rootObject.insert(QStringLiteral("importedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return QString::fromUtf8(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
}

QString GlslEffectInstaller::buildMainScript(Category category, int defaultDurationMs) const
{
    return (category == Category::MinimizeRestore ? minimizeRestoreScript() : openCloseScript()).arg(defaultDurationMs);
}

QString GlslEffectInstaller::buildConfigXml(int defaultDurationMs) const
{
    return QStringLiteral(R"XML(<?xml version="1.0" encoding="UTF-8"?>
<kcfg xmlns="http://www.kde.org/standards/kcfg/1.0"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xsi:schemaLocation="http://www.kde.org/standards/kcfg/1.0 http://www.kde.org/standards/kcfg/1.0/kcfg.xsd">
  <kcfgfile name="" />
  <group name="">
    <entry name="Duration" type="UInt">
      <default>%1</default>
    </entry>
    <entry name="PreserveBlur" type="Bool">
      <default>false</default>
    </entry>
  </group>
</kcfg>
)XML").arg(defaultDurationMs);
}

QString GlslEffectInstaller::buildConfigUi() const
{
    return QStringLiteral(R"XML(<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
  <class>PrismConfigForm</class>
  <widget class="QWidget" name="PrismConfigForm">
    <property name="geometry">
      <rect>
        <x>0</x>
        <y>0</y>
        <width>420</width>
        <height>120</height>
      </rect>
    </property>
    <layout class="QGridLayout" name="gridLayout">
      <item row="0" column="0">
        <widget class="QLabel" name="labelDuration">
          <property name="text">
            <string>Animation Time [ms]</string>
          </property>
          <property name="alignment">
            <set>Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter</set>
          </property>
        </widget>
      </item>
      <item row="0" column="1">
        <widget class="QSpinBox" name="kcfg_Duration">
          <property name="minimum">
            <number>50</number>
          </property>
          <property name="maximum">
            <number>10000</number>
          </property>
          <property name="singleStep">
            <number>50</number>
          </property>
        </widget>
      </item>
      <item row="1" column="1">
        <widget class="QCheckBox" name="kcfg_PreserveBlur">
          <property name="text">
            <string>Preserve blur/background contrast during animation</string>
          </property>
        </widget>
      </item>
    </layout>
  </widget>
  <resources />
  <connections />
</ui>
)XML");
}

QJsonObject GlslEffectInstaller::readJsonObjectFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool GlslEffectInstaller::reloadKWin(QString *errorMessage) const
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("reconfigure"));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (errorMessage) {
            *errorMessage = reply.errorMessage();
        }
        return false;
    }
    return true;
}
