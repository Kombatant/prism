#include "mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{
QString formatSourceKind(const QString &sourceKind)
{
    return sourceKind.compare(QStringLiteral("kdl"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("KDL import")
        : QStringLiteral("GLSL import");
}

QString cssColor(const QColor &color)
{
    return color.name(QColor::HexArgb);
}

QColor withAlpha(const QColor &color, int alpha)
{
    QColor adjusted = color;
    adjusted.setAlpha(alpha);
    return adjusted;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    applyStyle();
    updateCategoryUi();
    updateReuseUi();
    refreshInstalledEffects();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Prism"));
    resize(760, 560);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 10);
    mainLayout->setSpacing(8);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_importTab = buildImportTab();
    m_manageTab = buildManageTab();
    m_aboutTab = buildAboutTab();
    m_tabs->addTab(m_importTab, QStringLiteral("Import Effect"));
    m_tabs->addTab(m_manageTab, QStringLiteral("Manage Effects"));
    m_tabs->addTab(m_aboutTab, QStringLiteral("About..."));
    mainLayout->addWidget(m_tabs, 1);

    auto *statusFrame = new QFrame(this);
    statusFrame->setObjectName(QStringLiteral("statusFrame"));
    auto *statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(8, 6, 8, 6);
    statusLayout->setSpacing(6);

    auto *statusDot = new QLabel(statusFrame);
    statusDot->setObjectName(QStringLiteral("statusDot"));
    statusDot->setFixedSize(8, 8);
    statusLayout->addWidget(statusDot, 0, Qt::AlignVCenter);

    m_statusLabel = new QLabel(QStringLiteral("Ready."), statusFrame);
    m_statusLabel->setObjectName(QStringLiteral("statusText"));
    statusLayout->addWidget(m_statusLabel, 1);

    mainLayout->addWidget(statusFrame);
}

QWidget *MainWindow::buildImportTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(12);

    auto *introLabel = new QLabel(
        QStringLiteral("Import GLSL shader pairs or supported .kdl files as standalone KWin animation effects."),
        tab);
    introLabel->setObjectName(QStringLiteral("subtitleLabel"));
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    auto *identityLabel = new QLabel(QStringLiteral("Effect Identity"), tab);
    identityLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(identityLabel);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(8);
    formLayout->setContentsMargins(0, 0, 0, 0);

    m_effectNameEdit = new QLineEdit(tab);
    m_effectNameEdit->setPlaceholderText(QStringLiteral("e.g. Smoke, Burn, Portal"));
    formLayout->addRow(QStringLiteral("Name"), m_effectNameEdit);

    m_categoryCombo = new QComboBox(tab);
    for (const GlslEffectInstaller::CategorySpec &spec : GlslEffectInstaller::categories()) {
        m_categoryCombo->addItem(spec.displayName, static_cast<int>(spec.category));
    }
    formLayout->addRow(QStringLiteral("Category"), m_categoryCombo);

    auto *firstShaderLayout = new QHBoxLayout();
    firstShaderLayout->setSpacing(10);
    m_firstShaderLabel = new QLabel(tab);
    m_firstShaderEdit = new QLineEdit(tab);
    m_firstShaderButton = new QPushButton(QStringLiteral("Browse..."), tab);
    firstShaderLayout->addWidget(m_firstShaderEdit, 1);
    firstShaderLayout->addWidget(m_firstShaderButton);
    formLayout->addRow(m_firstShaderLabel, firstShaderLayout);

    auto *secondShaderLayout = new QHBoxLayout();
    secondShaderLayout->setSpacing(10);
    m_secondShaderLabel = new QLabel(tab);
    m_secondShaderEdit = new QLineEdit(tab);
    m_secondShaderButton = new QPushButton(QStringLiteral("Browse..."), tab);
    secondShaderLayout->addWidget(m_secondShaderEdit, 1);
    secondShaderLayout->addWidget(m_secondShaderButton);
    formLayout->addRow(m_secondShaderLabel, secondShaderLayout);

    m_reuseSingleShaderCheck = new QCheckBox(QStringLiteral("Apply the first shader for both effects"), tab);
    formLayout->addRow(QString(), m_reuseSingleShaderCheck);

    layout->addLayout(formLayout);

    auto *infoFrame = new QFrame(tab);
    infoFrame->setObjectName(QStringLiteral("infoFrame"));
    auto *infoLayout = new QHBoxLayout(infoFrame);
    infoLayout->setContentsMargins(12, 10, 12, 10);
    infoLayout->setSpacing(10);

    auto *infoIcon = new QLabel(QStringLiteral("i"), infoFrame);
    infoIcon->setObjectName(QStringLiteral("infoIcon"));
    infoIcon->setAlignment(Qt::AlignCenter);
    infoIcon->setFixedSize(18, 18);
    infoLayout->addWidget(infoIcon, 0, Qt::AlignTop);

    m_infoLabel = new QLabel(
        QStringLiteral("Effect packages are installed into ~/.local/share/kwin/effects/ and appear in System Settings -> Animations. "
                       "If the first path is a supported .kdl file, the second field is ignored."),
        infoFrame);
    m_infoLabel->setWordWrap(true);
    infoLayout->addWidget(m_infoLabel, 1);

    layout->addWidget(infoFrame);
    layout->addStretch(1);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    auto *installButton = new QPushButton(QStringLiteral("Install Effect"), tab);
    installButton->setObjectName(QStringLiteral("primaryButton"));
    installButton->setDefault(true);
    buttonRow->addWidget(installButton);
    layout->addLayout(buttonRow);

    connect(m_categoryCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::updateCategoryUi);
    connect(m_firstShaderEdit, &QLineEdit::textChanged, this, &MainWindow::updateReuseUi);
    connect(m_secondShaderEdit, &QLineEdit::textChanged, this, &MainWindow::updateReuseUi);
    connect(m_reuseSingleShaderCheck, &QCheckBox::toggled, this, &MainWindow::updateReuseUi);
    connect(m_firstShaderButton, &QPushButton::clicked, this, &MainWindow::browseFirstShader);
    connect(m_secondShaderButton, &QPushButton::clicked, this, &MainWindow::browseSecondShader);
    connect(installButton, &QPushButton::clicked, this, &MainWindow::installEffect);

    return tab;
}

QWidget *MainWindow::buildManageTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(10);

    auto *introLabel = new QLabel(
        QStringLiteral("Manage effects previously installed by this application."),
        tab);
    introLabel->setObjectName(QStringLiteral("subtitleLabel"));
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(10);

    m_effectSearchEdit = new QLineEdit(tab);
    m_effectSearchEdit->setPlaceholderText(QStringLiteral("Search installed effects..."));
    toolbarLayout->addWidget(m_effectSearchEdit, 1);

    auto *importNewButton = new QPushButton(QStringLiteral("Import New"), tab);
    toolbarLayout->addWidget(importNewButton);

    m_refreshEffectsButton = new QPushButton(QStringLiteral("Refresh"), tab);
    toolbarLayout->addWidget(m_refreshEffectsButton);

    layout->addLayout(toolbarLayout);

    m_effectsList = new QListWidget(tab);
    m_effectsList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_effectsList->setAlternatingRowColors(false);
    m_effectsList->setUniformItemSizes(false);
    layout->addWidget(m_effectsList, 1);

    auto *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(10);
    actionsLayout->addStretch(1);
    m_removeEffectButton = new QPushButton(QStringLiteral("Remove Effect"), tab);
    m_removeEffectButton->setObjectName(QStringLiteral("dangerButton"));
    m_removeEffectButton->setEnabled(false);
    actionsLayout->addWidget(m_removeEffectButton);
    layout->addLayout(actionsLayout);

    connect(importNewButton, &QPushButton::clicked, this, &MainWindow::switchToImportTab);
    connect(m_refreshEffectsButton, &QPushButton::clicked, this, &MainWindow::refreshInstalledEffects);
    connect(m_effectSearchEdit, &QLineEdit::textChanged, this, &MainWindow::filterInstalledEffects);
    connect(m_effectsList, &QListWidget::itemSelectionChanged, this, &MainWindow::updateInstalledEffectSelection);
    connect(m_removeEffectButton, &QPushButton::clicked, this, &MainWindow::removeSelectedEffect);

    return tab;
}

QWidget *MainWindow::buildAboutTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(12);

    auto *introLabel = new QLabel(
        QStringLiteral("Prism is a standalone Qt utility for packaging GLSL shaders as scripted KWin animation effects."),
        tab);
    introLabel->setObjectName(QStringLiteral("subtitleLabel"));
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    auto *detailsLabel = new QLabel(tab);
    detailsLabel->setTextFormat(Qt::RichText);
    detailsLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    detailsLabel->setOpenExternalLinks(true);
    detailsLabel->setWordWrap(true);
    detailsLabel->setText(QStringLiteral(
        "<p><b>Version:</b> 0.3</p>"
        "<p><b>Author:</b> Pete Vagiakos</p>"
        "<p><b>GitHub:</b> <a href=\"https://www.github.com/Kombatant/prism\">https://www.github.com/Kombatant/prism</a></p>"
        "<p>Use Prism to import GLSL shader pairs or supported KDL sources, generate KWin effect packages, "
        "and make them available in System Settings under animation options.</p>"));
    layout->addWidget(detailsLabel);

    layout->addStretch(1);

    return tab;
}

void MainWindow::applyStyle()
{
    const QPalette palette = this->palette();
    const QColor mutedText = palette.color(QPalette::Disabled, QPalette::WindowText);
    const QColor accentColor = palette.color(QPalette::Highlight);
    const QColor infoBackground = withAlpha(accentColor, 28);
    const QColor infoBorder = withAlpha(accentColor, 56);
    const QColor dangerColor = QColor(QStringLiteral("#da4453"));
    const QColor successColor = QColor(QStringLiteral("#27ae60"));

    setStyleSheet(QStringLiteral(R"CSS(
QLabel#subtitleLabel, QLabel#statusText {
    color: %1;
}
QLabel#sectionLabel {
    color: %1;
    font-weight: 700;
}
QFrame#infoFrame {
    background: %2;
    border: 1px solid %3;
    border-radius: 6px;
}
QLabel#infoIcon {
    background: %2;
    border-radius: 9px;
    color: %4;
    font-weight: 700;
}
QFrame#statusFrame {
    border: 1px solid %3;
    border-radius: 6px;
}
QLabel#statusDot {
    background: %5;
    border-radius: 4px;
    min-width: 8px;
    min-height: 8px;
    max-width: 8px;
    max-height: 8px;
}
)CSS")
        .arg(cssColor(mutedText))
        .arg(cssColor(infoBackground))
        .arg(cssColor(infoBorder))
        .arg(cssColor(accentColor))
        .arg(cssColor(successColor)));

    m_effectsList->setFrameShape(QFrame::StyledPanel);
    m_effectsList->setSpacing(4);
    m_tabs->tabBar()->setExpanding(false);
    m_tabs->tabBar()->setUsesScrollButtons(false);
    m_removeEffectButton->setStyleSheet(QStringLiteral("color: %1;").arg(cssColor(dangerColor)));
}

void MainWindow::browseFirstShader()
{
    setShaderPath(m_firstShaderEdit);
}

void MainWindow::browseSecondShader()
{
    setShaderPath(m_secondShaderEdit);
}

void MainWindow::installEffect()
{
    GlslEffectInstaller::InstallRequest request;
    request.effectName = m_effectNameEdit->text().trimmed();
    request.category = currentCategory();
    request.firstShaderPath = m_firstShaderEdit->text().trimmed();
    request.secondShaderPath = m_secondShaderEdit->text().trimmed();
    request.reuseSingleShaderForBoth = m_reuseSingleShaderCheck->isChecked();

    const GlslEffectInstaller::InstallResult result = m_installer.installEffect(request);
    updateStatus(result.message);

    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("Install Failed"), result.message);
        return;
    }

    refreshInstalledEffects();
    QMessageBox::information(
        this,
        QStringLiteral("Effect Installed"),
        QStringLiteral("%1\n\nPackage ID: %2\nInstalled to: %3")
            .arg(result.message, result.packageId, result.packagePath));
}

void MainWindow::updateCategoryUi()
{
    const GlslEffectInstaller::CategorySpec spec = GlslEffectInstaller::categorySpec(currentCategory());
    m_firstShaderLabel->setText(spec.firstShaderLabel);
    m_firstShaderEdit->setPlaceholderText(QStringLiteral("/path/to/%1.glsl or /path/to/effect.kdl").arg(spec.firstShaderLabel.toLower().replace(' ', '-')));
    m_secondShaderLabel->setText(spec.secondShaderLabel);
    m_secondShaderEdit->setPlaceholderText(QStringLiteral("/path/to/%1.glsl").arg(spec.secondShaderLabel.toLower().replace(' ', '-')));
    m_infoLabel->setText(QStringLiteral("Effect packages are installed into ~/.local/share/kwin/effects/ and appear in System Settings -> Animations. "
                                        "If the first path is a supported .kdl file, the second field is ignored."));
    updateReuseUi();
}

void MainWindow::updateReuseUi()
{
    const QString firstPath = m_firstShaderEdit->text().trimmed();
    const QString secondPath = m_secondShaderEdit->text().trimmed();
    const bool firstIsKdl = firstPath.endsWith(QStringLiteral(".kdl"), Qt::CaseInsensitive);
    const int populatedShaderCount = (firstPath.isEmpty() ? 0 : 1) + (secondPath.isEmpty() ? 0 : 1);
    const bool canReuse = !firstIsKdl && populatedShaderCount == 1;

    m_reuseSingleShaderCheck->setEnabled(canReuse);
    if (!canReuse && m_reuseSingleShaderCheck->isChecked()) {
        m_reuseSingleShaderCheck->setChecked(false);
    }

    const bool reuseEnabled = canReuse && m_reuseSingleShaderCheck->isChecked();
    m_secondShaderEdit->setEnabled(!reuseEnabled && !firstIsKdl);
    m_secondShaderButton->setEnabled(!reuseEnabled && !firstIsKdl);

    if (reuseEnabled) {
        m_secondShaderEdit->setPlaceholderText(QStringLiteral("The first shader will be reused for this side."));
    } else if (firstIsKdl) {
        m_secondShaderEdit->setPlaceholderText(QStringLiteral("Ignored for supported .kdl imports."));
    } else {
        const GlslEffectInstaller::CategorySpec spec = GlslEffectInstaller::categorySpec(currentCategory());
        m_secondShaderEdit->setPlaceholderText(QStringLiteral("/path/to/%1.glsl").arg(spec.secondShaderLabel.toLower().replace(' ', '-')));
    }
}

void MainWindow::refreshInstalledEffects()
{
    const QString filterText = m_effectSearchEdit ? m_effectSearchEdit->text().trimmed() : QString();
    m_installedEffects = m_installer.installedEffects();

    m_effectsList->clear();
    for (const GlslEffectInstaller::InstalledEffect &effect : m_installedEffects) {
        auto *item = new QListWidgetItem(effectSummary(effect), m_effectsList);
        item->setData(Qt::UserRole, effect.packageId);
        item->setToolTip(effect.packagePath);
    }

    m_tabs->setTabText(1, QStringLiteral("Manage Effects (%1)").arg(m_installedEffects.size()));
    filterInstalledEffects(filterText);
    updateInstalledEffectSelection();

    updateStatus(QStringLiteral("KWin compositor active — %1 imported effects detected.").arg(m_installedEffects.size()));
}

void MainWindow::filterInstalledEffects(const QString &text)
{
    const QString needle = text.trimmed().toLower();
    for (int index = 0; index < m_effectsList->count(); ++index) {
        QListWidgetItem *item = m_effectsList->item(index);
        const QString haystack = item->text().toLower();
        item->setHidden(!needle.isEmpty() && !haystack.contains(needle));
    }
}

void MainWindow::updateInstalledEffectSelection()
{
    const bool hasSelection = m_effectsList->currentItem() != nullptr && !m_effectsList->currentItem()->isHidden();
    m_removeEffectButton->setEnabled(hasSelection);
}

void MainWindow::removeSelectedEffect()
{
    QListWidgetItem *item = m_effectsList->currentItem();
    if (!item) {
        return;
    }

    const QString packageId = item->data(Qt::UserRole).toString();
    const QString displayName = item->text().section(QLatin1Char('\n'), 0, 0);
    const int response = QMessageBox::question(
        this,
        QStringLiteral("Remove Effect"),
        QStringLiteral("Remove \"%1\" from ~/.local/share/kwin/effects/?").arg(displayName));
    if (response != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!m_installer.removeInstalledEffect(packageId, &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("Remove Failed"), errorMessage);
        updateStatus(errorMessage);
        return;
    }

    refreshInstalledEffects();
    updateStatus(errorMessage.isEmpty()
        ? QStringLiteral("Removed %1.").arg(displayName)
        : errorMessage);
}

void MainWindow::switchToImportTab()
{
    m_tabs->setCurrentIndex(0);
}

void MainWindow::setShaderPath(QLineEdit *lineEdit)
{
    const QString selectedFile = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select Shader or KDL File"),
        QFileInfo(lineEdit->text()).absolutePath(),
        QStringLiteral("Shader or KDL Files (*.glsl *.frag *.fs *.kdl);;All Files (*)"));
    if (!selectedFile.isEmpty()) {
        lineEdit->setText(selectedFile);
    }
}

void MainWindow::updateStatus(const QString &message)
{
    m_statusLabel->setText(message);
}

GlslEffectInstaller::Category MainWindow::currentCategory() const
{
    return static_cast<GlslEffectInstaller::Category>(m_categoryCombo->currentData().toInt());
}

QString MainWindow::effectSummary(const GlslEffectInstaller::InstalledEffect &effect) const
{
    const QString state = effect.enabled ? QStringLiteral("enabled") : QStringLiteral("disabled");
    return QStringLiteral("%1\n%2 · %3 · %4 KiB · %5")
        .arg(effect.name, effect.category, formatSourceKind(effect.sourceKind))
        .arg(effect.sizeKiB)
        .arg(state);
}
