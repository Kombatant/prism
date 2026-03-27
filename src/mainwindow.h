#pragma once

#include "glsleffectinstaller.h"

#include <QEvent>
#include <QList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTabWidget;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private Q_SLOTS:
    void browseFirstShader();
    void browseSecondShader();
    void installEffect();
    void updateCategoryUi();
    void updateReuseUi();
    void refreshInstalledEffects();
    void filterInstalledEffects(const QString &text);
    void updateInstalledEffectSelection();
    void removeSelectedEffect();
    void switchToImportTab();

private:
    void buildUi();
    QWidget *buildImportTab();
    QWidget *buildManageTab();
    QWidget *buildAboutTab();
    void applyStyle();
    void setShaderPath(QLineEdit *lineEdit);
    void updateStatus(const QString &message);
    GlslEffectInstaller::Category currentCategory() const;
    const GlslEffectInstaller::InstalledEffect *selectedInstalledEffect() const;
    QString effectSummary(const GlslEffectInstaller::InstalledEffect &effect) const;

    QTabWidget *m_tabs = nullptr;
    QWidget *m_importTab = nullptr;
    QWidget *m_manageTab = nullptr;
    QWidget *m_aboutTab = nullptr;

    QLineEdit *m_effectNameEdit = nullptr;
    QComboBox *m_categoryCombo = nullptr;
    QLabel *m_firstShaderLabel = nullptr;
    QLineEdit *m_firstShaderEdit = nullptr;
    QPushButton *m_firstShaderButton = nullptr;
    QLabel *m_secondShaderLabel = nullptr;
    QLineEdit *m_secondShaderEdit = nullptr;
    QPushButton *m_secondShaderButton = nullptr;
    QCheckBox *m_reuseSingleShaderCheck = nullptr;

    QLineEdit *m_effectSearchEdit = nullptr;
    QListWidget *m_effectsList = nullptr;
    QPushButton *m_refreshEffectsButton = nullptr;
    QPushButton *m_removeEffectButton = nullptr;

    QLabel *m_infoLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    QList<GlslEffectInstaller::InstalledEffect> m_installedEffects;
    GlslEffectInstaller m_installer;
};
