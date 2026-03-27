#include "glsleffectinstaller.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("prism"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Import GLSL shaders as standalone KWin animation effects."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption installOption(QStringLiteral("install"), QStringLiteral("Install or replace an effect package non-interactively."));
    const QCommandLineOption nameOption(QStringLiteral("name"), QStringLiteral("Effect name."), QStringLiteral("name"));
    const QCommandLineOption categoryOption(QStringLiteral("category"), QStringLiteral("Category id: open-close or minimize-restore."), QStringLiteral("category"));
    const QCommandLineOption kdlOption(QStringLiteral("kdl"), QStringLiteral("Path to a supported .kdl file containing window-open/window-close custom shaders."), QStringLiteral("path"));
    const QCommandLineOption firstOption(QStringLiteral("first"), QStringLiteral("Path to the first shader file."), QStringLiteral("path"));
    const QCommandLineOption secondOption(QStringLiteral("second"), QStringLiteral("Path to the second shader file."), QStringLiteral("path"));
    const QCommandLineOption reuseOption(QStringLiteral("reuse-first-for-both"), QStringLiteral("Reuse the first shader for both directions when the second shader is omitted."));

    parser.addOption(installOption);
    parser.addOption(nameOption);
    parser.addOption(categoryOption);
    parser.addOption(kdlOption);
    parser.addOption(firstOption);
    parser.addOption(secondOption);
    parser.addOption(reuseOption);
    parser.process(app);

    if (parser.isSet(installOption)) {
        GlslEffectInstaller installer;
        GlslEffectInstaller::InstallRequest request;
        request.effectName = parser.value(nameOption).trimmed();
        request.kdlPath = parser.value(kdlOption).trimmed();
        request.firstShaderPath = parser.value(firstOption).trimmed();
        request.secondShaderPath = parser.value(secondOption).trimmed();
        request.reuseSingleShaderForBoth = parser.isSet(reuseOption);

        const QString category = parser.value(categoryOption).trimmed();
        if (category == QStringLiteral("minimize-restore")) {
            request.category = GlslEffectInstaller::Category::MinimizeRestore;
        } else {
            request.category = GlslEffectInstaller::Category::OpenClose;
        }

        const GlslEffectInstaller::InstallResult result = installer.installEffect(request);
        QTextStream stream(result.ok ? stdout : stderr);
        stream << result.message << Qt::endl;
        if (result.ok) {
            stream << result.packagePath << Qt::endl;
            return 0;
        }
        return 1;
    }

    MainWindow window;
    window.show();

    return app.exec();
}
