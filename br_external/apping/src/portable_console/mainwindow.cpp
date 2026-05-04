#include "portable_console/mainwindow.h"

#include "portable_console/asset_library_model.h"
#include "portable_console/audio_waveform_widget.h"
#include "portable_console/base_directory_model.h"
#include "portable_console/base_list_delegate.h"
#include "portable_console/base_map_widget.h"
#include "portable_console/portable_controller.h"
#include "ui_mainwindow.h"

#include <algorithm>

#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QEasingCurve>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QGridLayout>
#include <QIcon>
#include <QIntValidator>
#include <QItemSelectionModel>
#include <QLayout>
#include <QListView>
#include <QListWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyle>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QDoubleValidator>
#include <QVBoxLayout>
#include <QThread>

namespace apping {

namespace {

QIcon themedIcon(const QStringList& names,
                 QStyle::StandardPixmap fallback,
                 const QWidget* widget) {
    for (const QString& name : names) {
        const QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return (widget ? widget->style() : qApp->style())->standardIcon(fallback);
}

void prepareToolButton(QToolButton* button) {
    if (!button) {
        return;
    }
    button->setAutoRaise(false);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

QIcon recordCircleIcon() {
    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(QStringLiteral("#ef4444")));
    painter.setPen(QPen(QColor(QStringLiteral("#991b1b")), 2));
    painter.drawEllipse(QRectF(10, 10, 28, 28));
    return QIcon(pixmap);
}

void configureTouchScrollArea(QAbstractScrollArea* area, int singleStep = 36) {
    if (!area || !area->viewport()) {
        return;
    }

    area->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    area->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    area->verticalScrollBar()->setSingleStep(singleStep);
    area->verticalScrollBar()->setPageStep(singleStep * 4);

    QScroller::grabGesture(area->viewport(), QScroller::TouchGesture);
    QScroller::grabGesture(area->viewport(), QScroller::LeftMouseButtonGesture);

    QScroller* scroller = QScroller::scroller(area->viewport());
    QScrollerProperties props = scroller->scrollerProperties();
    props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.006);
    props.setScrollMetric(QScrollerProperties::AxisLockThreshold, 0.18);
    props.setScrollMetric(QScrollerProperties::ScrollingCurve, QEasingCurve(QEasingCurve::OutCubic));
    props.setScrollMetric(QScrollerProperties::DecelerationFactor, 0.14);
    props.setScrollMetric(QScrollerProperties::MaximumVelocity, 0.50);
    props.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 1.0);
    props.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.0);
    props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    props.setScrollMetric(QScrollerProperties::FrameRate, QScrollerProperties::Fps60);
    scroller->setScrollerProperties(props);
}

} // namespace

MainWindow::MainWindow(const PortableConfig& config, QWidget* parent)
    : QMainWindow(parent)
    , m_ui(std::make_unique<Ui::MainWindow>())
    , m_controller(std::make_unique<PortableController>(config, this)) {
    m_ui->setupUi(this);
    applyCompactStyle();
    installMapWidget();
    installRecordingWaveform();
    configureViews();
    setupTouchScrolling();
    setupConfirmationOverlay();
    setupBaseSettingsOverlay();
    setupIcons();
    connectSignals();

    m_ui->titleValueLabel->setText(m_controller->mapTitle());
    m_ui->titleStaticLabel->setMinimumWidth(54);
    m_ui->statusStaticLabel->setMinimumWidth(40);
    m_ui->liveInfoGroupBox->hide();
    setWindowTitle(QStringLiteral("Console portable Apping"));
    statusBar()->showMessage(QStringLiteral("Prêt"));

    updateStatus();
    updateBaseSelectionControls();
    updateLiveMegaphoneButton();
    updateLivePanel();
    updateRecordingButtons();
    updatePendingRecordingPanel();
    updateRecordingMeters();
    updateAssetButtons();
    clearBroadcastConfirmation();
}

MainWindow::~MainWindow() = default;

bool MainWindow::saveReportScreenshots(const QString& outputDirectory) {
    QDir dir(outputDirectory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    resize(800, 480);
    show();

    const auto settle = []() {
        for (int i = 0; i < 8; ++i) {
            qApp->processEvents(QEventLoop::AllEvents, 80);
            QThread::msleep(25);
        }
    };
    const auto save = [&](const QString& fileName) {
        settle();
        return grab().save(dir.filePath(fileName));
    };

    struct TabShot {
        QWidget* widget;
        QString fileName;
    };
    const QVector<TabShot> tabShots{
        {m_ui->homeTab, QStringLiteral("01-bases.png")},
        {m_ui->mapTab, QStringLiteral("02-carte.png")},
        {m_ui->messagesTab, QStringLiteral("03-diffusion.png")},
        {m_ui->liveTab, QStringLiteral("04-direct.png")},
        {m_ui->recordingTab, QStringLiteral("05-enregistrement.png")},
    };

    bool ok = true;
    for (const TabShot& shot : tabShots) {
        m_ui->mainTabWidget->setCurrentWidget(shot.widget);
        ok = save(shot.fileName) && ok;
    }

    if (auto* model = baseDirectoryModel(); model && model->rowCount() > 0) {
        m_ui->mainTabWidget->setCurrentWidget(m_ui->homeTab);
        showBaseSettings(model->index(0, 0));
        ok = save(QStringLiteral("06-parametres-poteau.png")) && ok;
        hideBaseSettings();
    }

    return ok;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_controller->recording()) {
        m_controller->stopRecording();
    }
    if (m_controller->liveMegaphoneActive()) {
        m_controller->stopLiveMegaphone();
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (m_ui && watched == m_ui->baseListView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_baseLongPressIndex = m_ui->baseListView->indexAt(mouseEvent->pos());
                m_baseLongPressPos = mouseEvent->pos();
                m_basePointerMoved = false;
                if (m_baseLongPressIndex.isValid()) {
                    m_baseLongPressTimer.start();
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if ((mouseEvent->pos() - m_baseLongPressPos).manhattanLength() > 12) {
                m_basePointerMoved = true;
                m_ignoreNextBaseClick = true;
                m_baseLongPressTimer.stop();
            }
        } else if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick) {
            m_baseLongPressTimer.stop();
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::installMapWidget() {
    auto* layout = qobject_cast<QVBoxLayout*>(m_ui->mapContainer->layout());
    if (!layout) {
        layout = new QVBoxLayout(m_ui->mapContainer);
        layout->setContentsMargins(0, 0, 0, 0);
    }

    m_mapWidget = new BaseMapWidget(m_ui->mapContainer);
    m_mapWidget->setController(m_controller.get());
    layout->addWidget(m_mapWidget);
}

void MainWindow::installRecordingWaveform() {
    m_recordingWaveformWidget = new AudioWaveformWidget(m_ui->recordingGroupBox);
    m_ui->recordingMeterLayout->removeWidget(m_ui->previewProgressBar);
    m_ui->previewProgressBar->hide();
    m_ui->recordingMeterLayout->setWidget(2, QFormLayout::FieldRole, m_recordingWaveformWidget);
    m_ui->previewProgressLabel->setText(QStringLiteral("Signal"));
}

void MainWindow::configureViews() {
    m_ui->baseListView->setModel(m_controller->baseModel());
    m_ui->baseListView->setItemDelegate(new BaseListDelegate(this));
    m_ui->baseListView->setSelectionMode(QAbstractItemView::NoSelection);
    m_ui->baseListView->setUniformItemSizes(false);
    m_ui->baseListView->setWordWrap(true);
    m_ui->baseListView->setSpacing(6);
    m_ui->baseListView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_ui->baseListView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_ui->baseListView->setAutoScroll(false);
    m_ui->baseListView->setMovement(QListView::Static);
    m_ui->baseListView->viewport()->installEventFilter(this);
    m_baseLongPressTimer.setSingleShot(true);
    m_baseLongPressTimer.setInterval(650);
    connect(&m_baseLongPressTimer, &QTimer::timeout, this, [this]() {
        if (!m_baseLongPressIndex.isValid()) {
            return;
        }
        m_ignoreNextBaseClick = true;
        showBaseSettings(m_baseLongPressIndex);
    });

    m_ui->assetListView->setModel(m_controller->libraryModel());
    m_ui->assetListView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ui->assetListView->setUniformItemSizes(false);
    m_ui->assetListView->setWordWrap(true);
    m_ui->assetListView->setSpacing(4);
    m_ui->assetListView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_ui->assetListView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_ui->assetListView->setAutoScroll(false);
    m_ui->assetListView->setMovement(QListView::Static);
    m_ui->liveTargetsValueLabel->setTextFormat(Qt::RichText);

    m_ui->intervalLineEdit->setValidator(new QIntValidator(0, 86400000, this));
    m_ui->durationLineEdit->setValidator(new QIntValidator(0, 86400, this));

    prepareToolButton(m_ui->liveMegaphoneButton);
    prepareToolButton(m_ui->recordButton);
    prepareToolButton(m_ui->previewButton);
    prepareToolButton(m_ui->broadcastBufferedButton);
    prepareToolButton(m_ui->broadcastSaveButton);
    prepareToolButton(m_ui->saveOnlyButton);
    prepareToolButton(m_ui->discardButton);
    m_ui->recordTitleLineEdit->setClearButtonEnabled(true);
    m_ui->recordTitleLineEdit->setEnabled(false);
}

void MainWindow::setupTouchScrolling() {
    configureTouchScrollArea(m_ui->baseListView, 42);
    configureTouchScrollArea(m_ui->assetListView, 42);
}

void MainWindow::setupConfirmationOverlay() {
    m_confirmationOverlay = new QFrame(centralWidget());
    m_confirmationOverlay->setObjectName(QStringLiteral("confirmationOverlay"));
    m_confirmationOverlay->hide();

    auto* overlayLayout = new QVBoxLayout(m_confirmationOverlay);
    overlayLayout->setContentsMargins(24, 24, 24, 24);
    overlayLayout->setSpacing(0);

    overlayLayout->addStretch();

    m_confirmationCard = new QFrame(m_confirmationOverlay);
    m_confirmationCard->setObjectName(QStringLiteral("confirmationCard"));
    m_confirmationCard->setMaximumWidth(540);
    m_confirmationCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    auto* cardLayout = new QVBoxLayout(m_confirmationCard);
    cardLayout->setContentsMargins(18, 18, 18, 18);
    cardLayout->setSpacing(12);

    m_confirmationTitleLabel = new QLabel(m_confirmationCard);
    m_confirmationTitleLabel->setObjectName(QStringLiteral("confirmationTitleLabel"));
    m_confirmationTitleLabel->setWordWrap(true);

    m_confirmationTextLabel = new QLabel(m_confirmationCard);
    m_confirmationTextLabel->setObjectName(QStringLiteral("confirmationTextLabel"));
    m_confirmationTextLabel->setWordWrap(true);

    m_confirmationBasesLabel = new QLabel(m_confirmationCard);
    m_confirmationBasesLabel->setObjectName(QStringLiteral("confirmationBasesLabel"));
    m_confirmationBasesLabel->setWordWrap(true);
    m_confirmationBasesLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_confirmationBasesLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_confirmationBasesScrollArea = new QScrollArea(m_confirmationCard);
    m_confirmationBasesScrollArea->setObjectName(QStringLiteral("confirmationBasesScrollArea"));
    m_confirmationBasesScrollArea->setFrameShape(QFrame::NoFrame);
    m_confirmationBasesScrollArea->setWidgetResizable(true);
    m_confirmationBasesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_confirmationBasesScrollArea->setWidget(m_confirmationBasesLabel);
    configureTouchScrollArea(m_confirmationBasesScrollArea, 48);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    m_confirmationConfirmButton = new QPushButton(QStringLiteral("Confirmer"), m_confirmationCard);
    m_confirmationConfirmButton->setObjectName(QStringLiteral("overlayConfirmButton"));
    m_confirmationCancelButton = new QPushButton(QStringLiteral("Annuler"), m_confirmationCard);
    m_confirmationCancelButton->setObjectName(QStringLiteral("overlayCancelButton"));
    buttonLayout->addWidget(m_confirmationConfirmButton);
    buttonLayout->addWidget(m_confirmationCancelButton);

    cardLayout->addWidget(m_confirmationTitleLabel);
    cardLayout->addWidget(m_confirmationTextLabel);
    cardLayout->addWidget(m_confirmationBasesScrollArea);
    cardLayout->addLayout(buttonLayout);

    overlayLayout->addWidget(m_confirmationCard, 0, Qt::AlignHCenter);
    overlayLayout->addStretch();

    updateConfirmationOverlayGeometry();
}

void MainWindow::setupBaseSettingsOverlay() {
    m_baseSettingsOverlay = new QFrame(centralWidget());
    m_baseSettingsOverlay->setObjectName(QStringLiteral("baseSettingsOverlay"));
    m_baseSettingsOverlay->hide();

    auto* overlayLayout = new QVBoxLayout(m_baseSettingsOverlay);
    overlayLayout->setContentsMargins(24, 24, 24, 24);
    overlayLayout->setSpacing(0);
    overlayLayout->addStretch();

    m_baseSettingsCard = new QFrame(m_baseSettingsOverlay);
    m_baseSettingsCard->setObjectName(QStringLiteral("baseSettingsCard"));
    m_baseSettingsCard->setMinimumWidth(620);
    m_baseSettingsCard->setMaximumWidth(760);
    m_baseSettingsCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    auto* cardLayout = new QVBoxLayout(m_baseSettingsCard);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(8);

    m_baseSettingsTitleLabel = new QLabel(m_baseSettingsCard);
    m_baseSettingsTitleLabel->setObjectName(QStringLiteral("baseSettingsTitleLabel"));
    m_baseSettingsTitleLabel->setWordWrap(true);

    m_baseSettingsTabWidget = new QTabWidget(m_baseSettingsCard);
    m_baseSettingsTabWidget->setObjectName(QStringLiteral("baseSettingsTabWidget"));

    m_baseInfoTab = new QWidget(m_baseSettingsTabWidget);
    auto* infoTabLayout = new QVBoxLayout(m_baseInfoTab);
    infoTabLayout->setContentsMargins(4, 4, 4, 4);
    infoTabLayout->setSpacing(8);
    auto* infoLayout = new QHBoxLayout();
    infoLayout->setSpacing(8);

    auto* formLayout = new QFormLayout();
    formLayout->setHorizontalSpacing(8);
    formLayout->setVerticalSpacing(7);
    m_baseNameLineEdit = new QLineEdit(m_baseInfoTab);
    m_baseDescriptionLineEdit = new QLineEdit(m_baseInfoTab);
    m_baseLatitudeLineEdit = new QLineEdit(m_baseInfoTab);
    m_baseLongitudeLineEdit = new QLineEdit(m_baseInfoTab);
    auto* coordinateValidator = new QDoubleValidator(-180.0, 180.0, 8, this);
    coordinateValidator->setNotation(QDoubleValidator::StandardNotation);
    m_baseLatitudeLineEdit->setValidator(coordinateValidator);
    m_baseLongitudeLineEdit->setValidator(coordinateValidator);
    formLayout->addRow(QStringLiteral("Nom"), m_baseNameLineEdit);
    formLayout->addRow(QStringLiteral("Description"), m_baseDescriptionLineEdit);
    formLayout->addRow(QStringLiteral("Latitude"), m_baseLatitudeLineEdit);
    formLayout->addRow(QStringLiteral("Longitude"), m_baseLongitudeLineEdit);

    m_baseStatusInfoLabel = new QLabel(m_baseInfoTab);
    m_baseStatusInfoLabel->setObjectName(QStringLiteral("baseSettingsInfoLabel"));
    m_baseStatusInfoLabel->setWordWrap(true);
    m_baseStatusInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_baseStatusInfoLabel->setMinimumWidth(240);
    m_baseNetworkInfoLabel = new QLabel(m_baseInfoTab);
    m_baseNetworkInfoLabel->setObjectName(QStringLiteral("baseNetworkInfoLabel"));
    m_baseNetworkInfoLabel->setWordWrap(true);
    m_baseNetworkInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* formWidget = new QWidget(m_baseInfoTab);
    formWidget->setLayout(formLayout);
    formWidget->setMinimumWidth(270);
    infoLayout->addWidget(formWidget, 2);
    infoLayout->addWidget(m_baseStatusInfoLabel);
    infoLayout->addWidget(m_baseNetworkInfoLabel);
    auto* infoActionsLayout = new QHBoxLayout();
    infoActionsLayout->setSpacing(8);
    m_baseSettingsEditButton = new QPushButton(QStringLiteral("Enregistrer"), m_baseInfoTab);
    m_baseSettingsCloseButton = new QPushButton(QStringLiteral("Fermer"), m_baseInfoTab);
    m_baseSettingsEditButton->setObjectName(QStringLiteral("baseSettingsEditButton"));
    m_baseSettingsCloseButton->setObjectName(QStringLiteral("baseSettingsCloseButton"));
    infoActionsLayout->addStretch();
    infoActionsLayout->addWidget(m_baseSettingsEditButton);
    infoActionsLayout->addWidget(m_baseSettingsCloseButton);
    infoTabLayout->addLayout(infoLayout, 1);
    infoTabLayout->addLayout(infoActionsLayout);

    m_baseAudioTab = new QWidget(m_baseSettingsTabWidget);
    auto* audioLayout = new QVBoxLayout(m_baseAudioTab);
    audioLayout->setContentsMargins(4, 4, 4, 4);
    audioLayout->setSpacing(8);
    m_baseAudioListWidget = new QListWidget(m_baseAudioTab);
    m_baseAudioListWidget->setObjectName(QStringLiteral("baseAudioListWidget"));
    m_baseAudioListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_baseAudioListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    configureTouchScrollArea(m_baseAudioListWidget, 42);
    auto* audioButtonsLayout = new QGridLayout();
    audioButtonsLayout->setSpacing(8);
    m_baseSettingsSyncButton = new QPushButton(QStringLiteral("Synchroniser"), m_baseAudioTab);
    m_baseAudioDeleteButton = new QPushButton(QStringLiteral("Supprimer sélection"), m_baseAudioTab);
    m_baseAudioSaveButton = new QPushButton(QStringLiteral("Enregistrer"), m_baseAudioTab);
    m_baseAudioCloseButton = new QPushButton(QStringLiteral("Fermer"), m_baseAudioTab);
    m_baseSettingsSyncButton->setObjectName(QStringLiteral("baseSettingsSyncButton"));
    m_baseAudioDeleteButton->setObjectName(QStringLiteral("baseAudioDeleteButton"));
    m_baseAudioSaveButton->setObjectName(QStringLiteral("baseAudioSaveButton"));
    m_baseAudioCloseButton->setObjectName(QStringLiteral("baseAudioCloseButton"));
    audioButtonsLayout->addWidget(m_baseSettingsSyncButton, 0, 0);
    audioButtonsLayout->addWidget(m_baseAudioDeleteButton, 0, 1);
    audioButtonsLayout->addWidget(m_baseAudioSaveButton, 1, 0);
    audioButtonsLayout->addWidget(m_baseAudioCloseButton, 1, 1);
    audioLayout->addWidget(m_baseAudioListWidget, 1);
    audioLayout->addLayout(audioButtonsLayout);

    m_baseSettingsTabWidget->addTab(m_baseInfoTab, QStringLiteral("Infos"));
    m_baseSettingsTabWidget->addTab(m_baseAudioTab, QStringLiteral("Audios"));

    cardLayout->addWidget(m_baseSettingsTitleLabel);
    cardLayout->addWidget(m_baseSettingsTabWidget);

    overlayLayout->addWidget(m_baseSettingsCard, 0, Qt::AlignHCenter);
    overlayLayout->addStretch();

    connect(m_baseSettingsCloseButton, &QPushButton::clicked, this, &MainWindow::hideBaseSettings);
    connect(m_baseAudioCloseButton, &QPushButton::clicked, this, &MainWindow::hideBaseSettings);
    connect(m_baseAudioSaveButton, &QPushButton::clicked, this, [this]() {
        statusBar()->showMessage(QStringLiteral("Réglages audio enregistrés"), 2000);
        hideBaseSettings();
    });
    connect(m_baseSettingsSyncButton, &QPushButton::clicked, this, &MainWindow::syncCurrentBaseAudio);
    connect(m_baseSettingsEditButton, &QPushButton::clicked, this, &MainWindow::saveBaseSettings);
    connect(m_baseAudioDeleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedBaseAudio);

    updateBaseSettingsOverlayGeometry();
}

void MainWindow::connectSignals() {
    connect(m_ui->refreshLibraryButton,
            &QAbstractButton::clicked,
            m_controller.get(),
            &PortableController::refreshLibraries);
    connect(m_ui->toggleBaseSelectionButton,
            &QAbstractButton::clicked,
            this,
            [this]() {
                auto* model = baseDirectoryModel();
                if (!model) {
                    return;
                }
                if (model->hasSelected()) {
                    m_controller->clearBaseSelection();
                } else {
                    m_controller->selectAllOnlineBases();
                }
            });
    connect(m_ui->stopSelectedButton,
            &QAbstractButton::clicked,
            m_controller.get(),
            &PortableController::stopSelected);

    connect(m_ui->baseListView, &QListView::clicked, this, [this](const QModelIndex& index) {
        if (m_ignoreNextBaseClick || m_basePointerMoved) {
            m_ignoreNextBaseClick = false;
            m_basePointerMoved = false;
            return;
        }
        m_controller->toggleBaseSelection(index.row());
    });

    connect(m_ui->assetListView, &QListView::doubleClicked, this, [this](const QModelIndex&) {
        playSelectedAsset(QStringLiteral("once"));
    });
    connect(m_ui->assetListView->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            [this]() {
                rememberSelectedAsset();
                updateAssetButtons();
            });
    connect(m_controller->libraryModel(),
            &QAbstractItemModel::modelReset,
            this,
            [this]() { QTimer::singleShot(0, this, &MainWindow::restoreSelectedAssetSelection); });

    connect(m_ui->playOnceButton, &QAbstractButton::clicked, this, [this]() {
        playSelectedAsset(QStringLiteral("once"));
    });
    connect(m_ui->playLoopButton, &QAbstractButton::clicked, this, [this]() {
        playSelectedAsset(QStringLiteral("repeat"));
    });

    connect(m_ui->liveMegaphoneButton, &QAbstractButton::clicked, this, [this]() {
        if (m_controller->liveMegaphoneActive()) {
            m_controller->stopLiveMegaphone();
        } else {
            if (selectedBroadcastBaseNames().isEmpty()) {
                statusBar()->showMessage(QStringLiteral("Sélectionne au moins un poteau dans le menu Bases"), 3500);
                m_ui->mainTabWidget->setCurrentWidget(m_ui->homeTab);
                return;
            }
            requestBroadcastConfirmation(ConfirmationScope::Live,
                                         QStringLiteral("Le direct micro va démarrer en multicast sur les poteaux sélectionnés."),
                                         [this]() { m_controller->startLiveMegaphone(); });
        }
    });

    connect(m_ui->recordButton, &QAbstractButton::clicked, this, [this]() {
        if (m_controller->recording()) {
            m_controller->stopRecording();
        } else {
            m_controller->startRecording();
        }
    });

    connect(m_ui->antiFeedbackCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) {
                m_controller->setAntiFeedbackEnabled(checked);
                if (m_ui->antiFeedbackCheckBox->isChecked() != m_controller->antiFeedbackEnabled()) {
                    QSignalBlocker blocker(m_ui->antiFeedbackCheckBox);
                    m_ui->antiFeedbackCheckBox->setChecked(m_controller->antiFeedbackEnabled());
                }
                updateLivePanel();
            });
    connect(m_controller.get(),
            &PortableController::antiFeedbackEnabledChanged,
            this,
            [this]() {
                QSignalBlocker blocker(m_ui->antiFeedbackCheckBox);
                m_ui->antiFeedbackCheckBox->setChecked(m_controller->antiFeedbackEnabled());
                updateLivePanel();
            });
    connect(m_ui->recordTitleLineEdit,
            &QLineEdit::textChanged,
            m_controller.get(),
            &PortableController::setPendingRecordingTitle);
    connect(m_recordingWaveformWidget,
            &AudioWaveformWidget::selectionChanged,
            m_controller.get(),
            &PortableController::setPendingRecordingSelection);

    connect(m_ui->previewButton,
            &QAbstractButton::clicked,
            m_controller.get(),
            &PortableController::previewRecording);
    connect(m_ui->cropButton,
            &QAbstractButton::clicked,
            m_controller.get(),
            &PortableController::applyPendingRecordingCrop);
    connect(m_ui->broadcastBufferedButton,
            &QAbstractButton::clicked,
            this,
            [this]() {
                requestBroadcastConfirmation(ConfirmationScope::Recording,
                                             QStringLiteral("Le message enregistré va être diffusé en multicast sur les poteaux sélectionnés."),
                                             [this]() { m_controller->broadcastPendingRecording(false); });
            });
    connect(m_ui->broadcastSaveButton,
            &QAbstractButton::clicked,
            this,
            [this]() {
                requestBroadcastConfirmation(ConfirmationScope::Recording,
                                             QStringLiteral("Le message enregistré va être diffusé en multicast puis sauvé sur les poteaux sélectionnés."),
                                             [this]() { m_controller->broadcastPendingRecording(true); });
            });
    connect(m_ui->saveOnlyButton,
            &QAbstractButton::clicked,
            m_controller.get(),
            &PortableController::savePendingRecordingToLibraries);
    connect(m_ui->discardButton,
            &QAbstractButton::clicked,
            m_controller.get(),
            &PortableController::discardPendingRecording);

    connect(m_controller.get(), &PortableController::statusMessageChanged, this, &MainWindow::updateStatus);
    connect(m_controller.get(),
            &PortableController::liveMegaphoneActiveChanged,
            this,
            &MainWindow::updateLiveMegaphoneButton);
    connect(m_controller.get(),
            &PortableController::liveMegaphoneActiveChanged,
            this,
            &MainWindow::updatePendingRecordingPanel);
    connect(m_controller.get(),
            &PortableController::recordingChanged,
            this,
            &MainWindow::updateRecordingButtons);
    connect(m_controller.get(),
            &PortableController::pendingRecordingChanged,
            this,
            &MainWindow::updatePendingRecordingPanel);
    connect(m_controller.get(),
            &PortableController::recordingInputLevelChanged,
            this,
            &MainWindow::updateRecordingMeters);
    connect(m_controller.get(),
            &PortableController::previewProgressChanged,
            this,
            &MainWindow::updateRecordingMeters);

    auto* model = m_controller->baseModel();
    connect(model, &QAbstractItemModel::dataChanged, this, [this]() { updateBaseSelectionControls(); });
    connect(model, &QAbstractItemModel::rowsInserted, this, [this]() { updateBaseSelectionControls(); });
    connect(model, &QAbstractItemModel::rowsRemoved, this, [this]() { updateBaseSelectionControls(); });
    connect(model, &QAbstractItemModel::modelReset, this, [this]() { updateBaseSelectionControls(); });

    connect(m_confirmationConfirmButton,
            &QPushButton::clicked,
            this,
            &MainWindow::executePendingBroadcastConfirmation);
    connect(m_confirmationCancelButton,
            &QPushButton::clicked,
            this,
            &MainWindow::clearBroadcastConfirmation);
}

void MainWindow::applyCompactStyle() {
    resize(800, 480);
    setMinimumSize(720, 420);
    updateAdaptiveMetrics();
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updateConfirmationOverlayGeometry();
    updateBaseSettingsOverlayGeometry();
    updateAdaptiveMetrics();
}

void MainWindow::updateAdaptiveMetrics() {
    const QSize windowSize = size();
    const bool compactScreen = windowSize.width() <= 900 || windowSize.height() <= 520;
    const int fontPx = std::clamp(std::min(windowSize.width() / 80, windowSize.height() / 40), 10, compactScreen ? 13 : 14);
    const int buttonFontPx = std::clamp(fontPx + 2, 12, 16);
    const int headerHeight = std::clamp(windowSize.height() / 13, 32, 48);
    const int buttonHeight = std::clamp(windowSize.height() / 15, 30, 42);
    const int toolButtonHeight = std::clamp(buttonHeight + 18, 52, 72);
    const int recordingToolButtonHeight = std::clamp(windowSize.height() / 13, 34, 46);
    const int fieldHeight = std::clamp(buttonHeight - 3, 26, 38);
    const int recordingFieldHeight = std::clamp(windowSize.height() / 14, 30, 40);
    const int tabHeight = std::clamp(buttonHeight - 4, 26, 36);
    const int mapHeight = std::clamp(windowSize.height() - 150, 220, 420);
    const int baseListHeight = std::clamp(windowSize.height() / 2, 180, 330);
    const int assetListHeight = std::clamp(windowSize.height() / 2, 180, 280);
    const int outerMargin = std::clamp(std::min(windowSize.width(), windowSize.height()) / 100, 4, 8);
    const int innerMargin = std::clamp(outerMargin - 1, 3, 6);
    const int recordingMargin = compactScreen ? 2 : innerMargin;
    const int sectionSpacing = std::clamp(outerMargin, 4, 8);
    const int recordingSpacing = compactScreen ? 3 : sectionSpacing;
    const int recordingMeterSpacing = compactScreen ? 8 : sectionSpacing + 2;
    const int listSpacing = std::clamp(sectionSpacing - 1, 3, 6);
    const int pillPaddingV = compactScreen ? 1 : 2;
    const int pillPaddingH = compactScreen ? 6 : 8;
    const int toolIconPx = std::clamp(buttonHeight - 2, 18, 26);
    const int tabIconPx = std::clamp(buttonHeight - 8, 14, 20);
    const int groupTitleMargin = std::clamp(buttonFontPx + 6, 16, 22);

    const auto setLayoutMetrics = [](QLayout* layout, int margin, int spacing) {
        if (!layout) {
            return;
        }
        layout->setSpacing(spacing);
        layout->setContentsMargins(margin, margin, margin, margin);
    };

    setLayoutMetrics(m_ui->verticalLayout, outerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->headerLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->homeTabLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->basesLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->mapTabLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->mapGroupBoxLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->messagesTabLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->prerecordedLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->messagesConfirmLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->liveTabLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->liveActionsLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->liveStatusLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->liveTargetsLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->liveInfoLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->liveConfirmLayout, innerMargin, sectionSpacing);
    setLayoutMetrics(m_ui->recordingTabLayout, recordingMargin, recordingSpacing);
    setLayoutMetrics(m_ui->recordingLayout, recordingMargin, recordingSpacing);
    setLayoutMetrics(m_ui->recordingManageLayout, compactScreen ? 1 : recordingMargin, recordingSpacing);
    setLayoutMetrics(m_ui->recordingConfirmLayout, recordingMargin, recordingSpacing);
    m_ui->recordingLayout->setColumnStretch(0, 1);
    m_ui->recordingLayout->setColumnStretch(1, 1);
    m_ui->recordingLayout->setRowStretch(0, 0);
    m_ui->recordingLayout->setRowStretch(1, 1);
    m_ui->recordingLayout->setRowStretch(2, 0);

    m_ui->basesButtonsLayout->setSpacing(sectionSpacing);
    m_ui->playbackFormLayout->setHorizontalSpacing(sectionSpacing);
    m_ui->playbackFormLayout->setVerticalSpacing(sectionSpacing);
    m_ui->assetButtonsLayout->setSpacing(sectionSpacing);
    m_ui->messagesConfirmButtonsLayout->setSpacing(sectionSpacing);
    m_ui->pendingFormLayout->setHorizontalSpacing(recordingSpacing);
    m_ui->pendingFormLayout->setVerticalSpacing(recordingSpacing);
    m_ui->recordingMeterLayout->setHorizontalSpacing(sectionSpacing);
    m_ui->recordingMeterLayout->setVerticalSpacing(recordingMeterSpacing);
    m_ui->captureActionsLayout->setSpacing(recordingSpacing);
    m_ui->recordingActionsLayout->setHorizontalSpacing(sectionSpacing);
    m_ui->recordingActionsLayout->setVerticalSpacing(recordingSpacing);
    m_ui->liveConfirmButtonsLayout->setSpacing(sectionSpacing);
    m_ui->recordingConfirmButtonsLayout->setSpacing(sectionSpacing);

    setStyleSheet(QStringLiteral(
        "QWidget { font-size: %1px; }"
        "QFrame#headerFrame { border: 1px solid #d7dde3; border-radius: 8px; background: #fbfcfd; }"
        "QFrame#liveStatusFrame { border: 1px solid #d9e2ec; border-radius: 8px; background: #f8fbff; }"
        "QFrame#confirmationOverlay { background: rgba(6, 8, 11, 236); }"
        "QFrame#confirmationCard { background: rgba(255, 251, 245, 250); border: 1px solid #e7bc74; border-radius: 18px; }"
        "QFrame#baseSettingsOverlay { background: rgba(6, 8, 11, 232); }"
        "QFrame#baseSettingsCard { background: rgba(248, 250, 252, 250); border: 1px solid #93a4b8; border-radius: 18px; }"
        "QLabel#titleValueLabel { font-weight: 600; color: #0f1720; }"
        "QLabel#statusValueLabel { background: #eef6ff; color: #155b9a; border: 1px solid #c7def7; border-radius: 8px; padding: %5px %6px; }"
        "QLabel#liveStateValueLabel { background: #fff0d8; color: #8b4a00; border: 1px solid #e7bc74; border-radius: 8px; padding: %5px %6px; font-weight: 700; }"
        "QLabel#liveTargetCountValueLabel { background: #e8f1ff; color: #12407b; border: 1px solid #a5c4f2; border-radius: 8px; padding: %5px %6px; font-weight: 700; min-width: 24px; }"
        "QLabel#liveTargetsValueLabel { border: 1px solid #d7dde3; border-radius: 8px; background: #fbfcfd; padding: 6px; }"
        "QLabel#confirmationTitleLabel { font-size: %7px; font-weight: 700; color: #111827; }"
        "QLabel#baseSettingsTitleLabel { font-size: %7px; font-weight: 700; color: #111827; }"
        "QLabel#confirmationTextLabel { font-size: %7px; color: #1f2937; }"
        "QLabel#confirmationBasesLabel { border: 1px solid #e7bc74; border-radius: 12px; background: rgba(255,255,255,220); padding: 10px; font-size: %1px; color: #2d3748; }"
        "QLabel#baseSettingsInfoLabel, QLabel#baseNetworkInfoLabel { border: 1px solid #cbd5e1; border-radius: 10px; background: white; padding: 8px; font-size: %1px; color: #1f2937; }"
        "QListWidget#baseAudioListWidget { border: 1px solid #cbd5e1; border-radius: 10px; background: white; padding: 4px 4px 10px 4px; font-size: %7px; }"
        "QScrollArea#confirmationBasesScrollArea { border: 0; background: transparent; }"
        "QGroupBox { font-size: %7px; font-weight: 600; margin-top: %9px; padding-top: 4px; border: 1px solid #d7dde3; border-radius: 8px; background: white; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 8px; top: 1px; padding: 0 4px; background: white; color: #111827; }"
        "QPushButton, QToolButton { font-size: %7px; font-weight: 600; min-height: %2px; padding: 4px 8px; border-radius: 8px; border: 1px solid #cfd8e3; background: #f4f7fa; color: #16202a; }"
        "QPushButton:hover, QToolButton:hover { background: #eaf0f6; }"
        "QToolButton { min-height: %8px; padding: 6px 6px; }"
        "QPushButton#toggleBaseSelectionButton, QPushButton#playOnceButton, QToolButton#broadcastBufferedButton { background: #dff7ea; border-color: #8fd2aa; color: #14532d; }"
        "QPushButton#playLoopButton, QPushButton#refreshLibraryButton, QToolButton#previewButton, QToolButton#cropButton, QToolButton#saveOnlyButton { background: #e8f1ff; border-color: #a5c4f2; color: #12407b; }"
        "QToolButton#liveMegaphoneButton, QToolButton#broadcastSaveButton { background: #fff0d8; border-color: #e7bc74; color: #8b4a00; }"
        "QToolButton#recordButton { background: #ffe4e6; border-color: #ef4444; color: #991b1b; }"
        "QPushButton#stopSelectedButton, QToolButton#discardButton { background: #ffe4e6; border-color: #f2b1b7; color: #9a1f2e; }"
        "QPushButton#messagesConfirmButton, QPushButton#liveConfirmButton, QPushButton#recordingConfirmButton { background: #fff0d8; border-color: #e7bc74; color: #8b4a00; }"
        "QPushButton#overlayConfirmButton { background: #fff0d8; border-color: #e7bc74; color: #8b4a00; }"
        "QPushButton#overlayCancelButton { background: #f3f6f9; border-color: #cfd8e3; color: #1f2937; }"
        "QPushButton#baseSettingsSyncButton { background: #e8f1ff; border-color: #a5c4f2; color: #12407b; }"
        "QPushButton#baseSettingsEditButton { background: #fff0d8; border-color: #e7bc74; color: #8b4a00; }"
        "QPushButton#baseSettingsCloseButton { background: #f3f6f9; border-color: #cfd8e3; color: #1f2937; }"
        "QPushButton#baseAudioSaveButton { background: #fff0d8; border-color: #e7bc74; color: #8b4a00; }"
        "QPushButton#baseAudioCloseButton { background: #f3f6f9; border-color: #cfd8e3; color: #1f2937; }"
        "QPushButton#baseAudioDeleteButton { background: #ffe4e6; border-color: #f2b1b7; color: #9a1f2e; }"
        "QTabWidget#baseSettingsTabWidget::pane { border: 1px solid #cbd5e1; border-radius: 8px; background: #f8fafc; }"
        "QTabWidget#baseSettingsTabWidget QTabBar::tab { min-height: %4px; min-width: 120px; padding: 4px 12px; font-size: %7px; }"
        "QCheckBox { spacing: 8px; color: #1f2937; }"
        "QCheckBox::indicator { width: 22px; height: 22px; border-radius: 6px; border: 1px solid #cfd8e3; background: white; }"
        "QCheckBox::indicator:checked { background: #12407b; border-color: #12407b; }"
        "QLineEdit { min-height: %3px; padding: 3px 6px; border: 1px solid #cfd8e3; border-radius: 7px; background: white; }"
        "QWidget#recordingTab QLineEdit { min-height: %11px; padding: 1px 5px; }"
        "QWidget#recordingTab QLabel { padding: 0px; }"
        "QWidget#recordingTab QGroupBox { margin-top: %12px; padding-top: 2px; }"
        "QWidget#recordingTab QToolButton { min-height: %13px; padding: 2px 5px; }"
        "QWidget#recordingTab QProgressBar { min-height: %11px; margin-top: 2px; margin-bottom: 2px; }"
        "QProgressBar { min-height: %10px; border: 1px solid #cfd8e3; border-radius: 7px; background: #eef2f6; }"
        "QProgressBar::chunk { border-radius: 6px; background: #22c55e; }"
        "QProgressBar#previewProgressBar::chunk { background: #2563eb; }"
        "QLabel#saturationValueLabel { border-radius: 7px; padding: 3px 8px; background: #dcfce7; color: #166534; font-weight: 700; }"
        "QListView { border: 1px solid #d7dde3; border-radius: 8px; padding: 2px 2px 10px 2px; background: white; }"
        "QScrollBar:vertical { width: 24px; background: #edf2f7; border-radius: 12px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: #9fb3c8; min-height: 44px; border-radius: 11px; margin: 2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical, QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { height: 0px; background: transparent; }"
        "QTabWidget::pane { border: 1px solid #d7dde3; border-radius: 8px; background: white; }"
        "QTabBar::tab { min-height: %4px; min-width: 82px; padding: 4px 10px; border-top-left-radius: 7px; border-top-right-radius: 7px; background: #eef2f6; border: 1px solid #d7dde3; color: #425466; font-size: %7px; }"
        "QTabBar::tab:selected { background: white; color: #111827; font-weight: 700; }")
                          .arg(fontPx)
                          .arg(buttonHeight)
                          .arg(fieldHeight)
                          .arg(tabHeight)
                          .arg(pillPaddingV)
                          .arg(pillPaddingH)
                          .arg(buttonFontPx)
                          .arg(toolButtonHeight)
                          .arg(groupTitleMargin)
                          .arg(std::clamp(recordingFieldHeight - 8, 12, 18))
                          .arg(recordingFieldHeight)
                          .arg(compactScreen ? 13 : groupTitleMargin)
                          .arg(recordingToolButtonHeight));

    m_ui->headerFrame->setMaximumHeight(headerHeight);
    m_ui->titleStaticLabel->setMinimumWidth(compactScreen ? 48 : 54);
    m_ui->statusStaticLabel->setMinimumWidth(compactScreen ? 34 : 40);
    m_ui->statusValueLabel->setMinimumWidth(std::clamp(windowSize.width() / 5, 120, 190));
    m_ui->baseListView->setSpacing(listSpacing);
    m_ui->assetListView->setSpacing(listSpacing);
    m_ui->mapContainer->setMinimumHeight(mapHeight);
    m_ui->baseListView->setMinimumHeight(baseListHeight);
    const int boundedAssetListHeight = compactScreen
        ? std::clamp(windowSize.height() - 275, 170, 225)
        : std::clamp(windowSize.height() - 270, 140, assetListHeight);
    m_ui->assetListView->setMinimumHeight(boundedAssetListHeight);
    m_ui->assetListView->setMaximumHeight(boundedAssetListHeight);
    m_ui->assetListView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (m_confirmationCard) {
        m_confirmationCard->setMaximumWidth(std::clamp(windowSize.width() - 28, 340, 560));
    }
    if (m_confirmationOverlay) {
        if (auto* overlayLayout = qobject_cast<QVBoxLayout*>(m_confirmationOverlay->layout())) {
            const int overlayMargin = std::clamp(std::min(windowSize.width(), windowSize.height()) / 18, 14, 28);
            overlayLayout->setContentsMargins(overlayMargin, overlayMargin, overlayMargin, overlayMargin);
        }
    }
    if (m_confirmationBasesScrollArea) {
        m_confirmationBasesScrollArea->setMinimumHeight(std::clamp(windowSize.height() / 5, 90, 130));
        m_confirmationBasesScrollArea->setMaximumHeight(std::clamp(windowSize.height() / 3, 140, 220));
    }
    if (m_baseSettingsCard) {
        m_baseSettingsCard->setMinimumWidth(std::clamp(windowSize.width() - 52, 560, 720));
        m_baseSettingsCard->setMaximumWidth(std::clamp(windowSize.width() - 24, 620, 760));
        m_baseSettingsCard->setMaximumHeight(std::clamp(windowSize.height() - 34, 380, 450));
    }
    if (m_baseSettingsTabWidget) {
        m_baseSettingsTabWidget->setMinimumHeight(std::clamp(windowSize.height() - 168, 230, 300));
        m_baseSettingsTabWidget->setMaximumHeight(std::clamp(windowSize.height() - 132, 260, 340));
    }
    if (m_baseAudioListWidget) {
        const int boundedBaseAudioHeight = compactScreen
            ? std::clamp(windowSize.height() - 345, 88, 135)
            : std::clamp(windowSize.height() - 280, 130, 220);
        m_baseAudioListWidget->setMinimumHeight(boundedBaseAudioHeight);
        m_baseAudioListWidget->setMaximumHeight(boundedBaseAudioHeight);
        m_baseAudioListWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_baseAudioListWidget->setIconSize(QSize(toolIconPx, toolIconPx));
    }
    if (m_baseSettingsOverlay) {
        if (auto* overlayLayout = qobject_cast<QVBoxLayout*>(m_baseSettingsOverlay->layout())) {
            const int overlayMargin = std::clamp(std::min(windowSize.width(), windowSize.height()) / 18, 14, 28);
            overlayLayout->setContentsMargins(overlayMargin, overlayMargin, overlayMargin, overlayMargin);
        }
    }

    const QSize toolIconSize(toolIconPx, toolIconPx);
    m_ui->mainTabWidget->setIconSize(QSize(tabIconPx, tabIconPx));
    m_ui->liveMegaphoneButton->setIconSize(toolIconSize);
    m_ui->recordButton->setIconSize(toolIconSize);
    m_ui->previewButton->setIconSize(toolIconSize);
    m_ui->cropButton->setIconSize(toolIconSize);
    m_ui->broadcastBufferedButton->setIconSize(toolIconSize);
    m_ui->broadcastSaveButton->setIconSize(toolIconSize);
    m_ui->saveOnlyButton->setIconSize(toolIconSize);
    m_ui->discardButton->setIconSize(toolIconSize);
    m_ui->inputLevelProgressBar->setMaximumHeight(recordingFieldHeight);
    m_ui->previewProgressBar->setMaximumHeight(recordingFieldHeight);
    if (m_recordingWaveformWidget) {
        m_recordingWaveformWidget->setMinimumHeight(std::clamp(recordingFieldHeight + 24, 54, 70));
        m_recordingWaveformWidget->setMaximumHeight(std::clamp(recordingFieldHeight + 32, 62, 82));
    }
    m_ui->recordingGroupBox->setMinimumWidth(std::clamp(windowSize.width() / 2, 360, 520));
    m_ui->recordingManageGroupBox->setMinimumWidth(std::clamp(windowSize.width() / 4, 210, 320));
    m_ui->recordingGroupBox->setMinimumHeight(compactScreen ? 258 : 280);
    m_ui->recordingGroupBox->setMaximumHeight(compactScreen ? 286 : 330);
    m_ui->recordingManageGroupBox->setMinimumHeight(compactScreen ? 72 : 86);
    m_ui->recordingManageGroupBox->setMaximumHeight(compactScreen ? 76 : 96);
    m_ui->recordingConfirmGroupBox->setMaximumHeight(compactScreen ? 118 : 180);
    m_ui->pendingPathValueLabel->setMaximumHeight(compactScreen ? 28 : 44);
    for (QToolButton* button : {m_ui->recordButton,
                                m_ui->previewButton,
                                m_ui->cropButton,
                                m_ui->broadcastBufferedButton,
                                m_ui->broadcastSaveButton,
                                m_ui->saveOnlyButton,
                                m_ui->discardButton}) {
        button->setMaximumHeight(recordingToolButtonHeight);
        button->setToolButtonStyle(compactScreen ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonTextUnderIcon);
    }
}

void MainWindow::updateStatus() {
    const QString message = m_controller->statusMessage().isEmpty()
        ? QStringLiteral("Prêt")
        : m_controller->statusMessage();
    m_ui->statusValueLabel->setText(message);
    statusBar()->showMessage(message);
}

void MainWindow::updateLiveMegaphoneButton() {
    m_ui->liveMegaphoneButton->setText(m_controller->liveMegaphoneActive()
                                           ? QStringLiteral("Couper direct")
                                           : QStringLiteral("Parler direct"));
    updateLivePanel();
}

void MainWindow::updateLivePanel() {
    auto* model = baseDirectoryModel();
    const bool active = m_controller->liveMegaphoneActive();
    const QStringList baseNames = selectedBroadcastBaseNames();
    const int selectedOnlineCount = baseNames.size();

    m_ui->liveStateValueLabel->setText(active
                                           ? QStringLiteral("Direct actif")
                                           : QStringLiteral("En attente"));
    m_ui->liveTargetCountValueLabel->setText(QString::number(selectedOnlineCount));
    {
        QSignalBlocker blocker(m_ui->antiFeedbackCheckBox);
        m_ui->antiFeedbackCheckBox->setChecked(m_controller->antiFeedbackEnabled());
    }
    m_ui->antiFeedbackCheckBox->setEnabled(false);
    m_ui->antiFeedbackCheckBox->setToolTip(
        QStringLiteral("Désactivé : le direct utilise le micro standard pour garder l'audio stable"));

    if (baseNames.isEmpty()) {
        m_ui->liveTargetsValueLabel->setText(
            QStringLiteral("<span style='color:#64748b;'>Aucun poteau en ligne ciblé.</span>"));
    } else {
        QString html;
        html.reserve(baseNames.size() * 90);
        html += QStringLiteral("<table cellspacing='4' cellpadding='0'>");
        for (int i = 0; i < baseNames.size(); ++i) {
            if (i % 2 == 0) {
                html += QStringLiteral("<tr>");
            }
            html += QStringLiteral(
                        "<td style='background:#eef6ff;border:1px solid #a5c4f2;"
                        "border-radius:6px;padding:5px 8px;color:#12407b;font-weight:700;'>%1</td>")
                        .arg(baseNames.at(i).toHtmlEscaped());
            if (i % 2 == 1) {
                html += QStringLiteral("</tr>");
            }
        }
        if (baseNames.size() % 2 == 1) {
            html += QStringLiteral("<td></td></tr>");
        }
        html += QStringLiteral("</table>");
        m_ui->liveTargetsValueLabel->setText(html);
    }

    Q_UNUSED(model);
    m_ui->liveMegaphoneButton->setEnabled(true);
    m_ui->liveMegaphoneButton->setToolTip(active
                                              ? QStringLiteral("Arrêter la diffusion micro en direct")
                                              : (selectedOnlineCount > 0
                                                     ? QStringLiteral("Préparer le direct multicast vers les poteaux ciblés")
                                                     : QStringLiteral("Clique pour retourner choisir un poteau")));
}

void MainWindow::updateRecordingButtons() {
    m_ui->recordButton->setText(m_controller->recording()
                                    ? QStringLiteral("Stop enreg.")
                                    : QStringLiteral("Enregistrer"));
    updatePendingRecordingPanel();
}

void MainWindow::updatePendingRecordingPanel() {
    const bool hasPending = m_controller->hasPendingRecording();
    const QString pendingPath = m_controller->pendingRecordingPath();
    const QString pendingTitle = m_controller->pendingRecordingTitle().trimmed();
    const QString pendingFileName = QFileInfo(pendingPath).fileName();
    const qint64 durationMs = m_controller->pendingRecordingDurationMs();
    const qint64 selectionStartMs = m_controller->pendingRecordingSelectionStartMs();
    const qint64 selectionEndMs = m_controller->pendingRecordingSelectionEndMs();
    const QString pendingLabel = hasPending
        ? (pendingTitle.isEmpty() ? pendingFileName : pendingTitle)
        : QStringLiteral("Aucun message en attente");
    m_ui->pendingPathValueLabel->setText(pendingLabel);
    m_ui->pendingPathValueLabel->setToolTip(hasPending
                                                ? QStringLiteral("%1\n%2")
                                                      .arg(pendingTitle.isEmpty() ? pendingFileName : pendingTitle,
                                                           pendingPath)
                                                : QString());
    if (hasPending && durationMs > 0) {
        const QString selectionText =
            selectionStartMs <= 0 && selectionEndMs >= durationMs
                ? QStringLiteral("tout")
                : QStringLiteral("%1-%2")
                      .arg(QString::number(selectionStartMs / 1000.0, 'f', 1),
                           QString::number(selectionEndMs / 1000.0, 'f', 1));
        m_ui->pendingDurationValueLabel->setText(
            QStringLiteral("%1  |  sélection %2").arg(m_controller->pendingRecordingDuration(),
                                                       selectionText));
    } else {
        m_ui->pendingDurationValueLabel->setText(QStringLiteral("--:--"));
    }

    if (m_recordingWaveformWidget) {
        if (hasPending && durationMs > 0) {
            m_recordingWaveformWidget->setWaveform(m_controller->pendingRecordingPeaks(), durationMs);
            m_recordingWaveformWidget->setSelection(selectionStartMs, selectionEndMs);
        } else {
            m_recordingWaveformWidget->clearWaveform();
        }
    }

    const bool canUsePending = hasPending && !m_controller->recording()
        && !m_controller->liveMegaphoneActive();
    if (m_recordingWaveformWidget) {
        m_recordingWaveformWidget->setEnabled(canUsePending);
    }
    m_ui->previewButton->setEnabled(canUsePending);
    m_ui->cropButton->setEnabled(canUsePending && durationMs > 0
                                 && !(selectionStartMs <= 0 && selectionEndMs >= durationMs));
    m_ui->broadcastBufferedButton->setEnabled(canUsePending);
    m_ui->broadcastSaveButton->setEnabled(canUsePending);
    m_ui->saveOnlyButton->setEnabled(canUsePending);
    m_ui->discardButton->setEnabled(hasPending);
    m_ui->recordTitleLineEdit->setEnabled(hasPending);
    m_ui->recordTitleLineEdit->setPlaceholderText(hasPending
                                                      ? QStringLiteral("Titre du message enregistré")
                                                      : QStringLiteral("Aucun message enregistré"));
    if (m_ui->recordTitleLineEdit->text() != pendingTitle) {
        QSignalBlocker blocker(m_ui->recordTitleLineEdit);
        m_ui->recordTitleLineEdit->setText(pendingTitle);
    }
}

void MainWindow::updateRecordingMeters() {
    const int inputLevel = m_controller->recordingInputLevel();
    const bool saturated = m_controller->recordingSaturated();
    m_ui->inputLevelProgressBar->setValue(inputLevel);
    m_ui->inputLevelProgressBar->setToolTip(QStringLiteral("Niveau micro : %1%").arg(inputLevel));
    m_ui->saturationValueLabel->setText(saturated ? QStringLiteral("Trop fort") : QStringLiteral("OK"));
    m_ui->saturationValueLabel->setStyleSheet(saturated
                                                  ? QStringLiteral("background: #fee2e2; color: #991b1b;")
                                                  : QStringLiteral("background: #dcfce7; color: #166534;"));

    const int previewProgress = m_controller->previewProgress();
    m_ui->previewProgressBar->setValue(previewProgress);
    m_ui->previewProgressBar->setToolTip(m_controller->previewPlaying()
                                             ? QStringLiteral("Lecture en cours : %1%").arg(previewProgress)
                                             : QStringLiteral("Lecture arrêtée"));
    if (m_recordingWaveformWidget) {
        m_recordingWaveformWidget->setPlaybackPosition(
            m_controller->previewPlaying() ? m_controller->previewWaveformPositionMs() : -1);
        m_recordingWaveformWidget->setToolTip(m_controller->previewPlaying()
                                                  ? QStringLiteral("Lecture en cours : %1%").arg(previewProgress)
                                                  : QStringLiteral("Déplace les deux bornes pour rogner le message"));
    }
}

void MainWindow::updateAssetButtons() {
    const bool hasAsset = !selectedAssetId().isEmpty();
    m_ui->playOnceButton->setEnabled(hasAsset);
    m_ui->playLoopButton->setEnabled(hasAsset);
}

void MainWindow::updateBaseSelectionControls() {
    auto* model = baseDirectoryModel();
    if (!model) {
        return;
    }

    const int selectedCount = model->selectedCount();
    const int onlineCount = model->onlineCount();
    const int selectedOnlineCount = model->selectedOnlineSnapshots().size();
    const int totalCount = model->rowCount();

    m_ui->toggleBaseSelectionButton->setText(selectedCount > 0
                                                 ? QStringLiteral("Tout décocher")
                                                 : QStringLiteral("Tout cocher"));
    m_ui->toggleBaseSelectionButton->setEnabled(selectedCount > 0 || onlineCount > 0);
    m_ui->stopSelectedButton->setEnabled(selectedOnlineCount > 0);

    if (totalCount <= 0) {
        m_ui->basesHintLabel->setText(QStringLiteral("Aucune base détectée pour le moment."));
        updateLivePanel();
        refreshBroadcastConfirmationPanel();
        return;
    }

    m_ui->basesHintLabel->setText(
        QStringLiteral("%1 en ligne • %2 ciblée(s) sur %3. Touchez une base pour la cocher.")
            .arg(onlineCount)
            .arg(selectedCount)
            .arg(totalCount));

    updateLivePanel();
    refreshBroadcastConfirmationPanel();
}

void MainWindow::setupIcons() {
    m_ui->toggleBaseSelectionButton->setIcon(
        themedIcon({QStringLiteral("edit-select-all")}, QStyle::SP_DialogYesButton, this));
    m_ui->refreshLibraryButton->setIcon(
        themedIcon({QStringLiteral("view-refresh")}, QStyle::SP_BrowserReload, this));
    m_ui->stopSelectedButton->setIcon(
        themedIcon({QStringLiteral("media-playback-stop")}, QStyle::SP_MediaStop, this));
    m_ui->playOnceButton->setIcon(
        themedIcon({QStringLiteral("media-playback-start")}, QStyle::SP_MediaPlay, this));
    m_ui->playLoopButton->setIcon(
        themedIcon({QStringLiteral("media-playlist-repeat"), QStringLiteral("view-refresh")},
                   QStyle::SP_BrowserReload,
                   this));

    m_ui->liveMegaphoneButton->setIcon(
        themedIcon({QStringLiteral("audio-input-microphone"), QStringLiteral("microphone-sensitivity-high")},
                   QStyle::SP_MediaVolume,
                   this));
    m_ui->recordButton->setIcon(recordCircleIcon());
    m_ui->previewButton->setIcon(
        themedIcon({QStringLiteral("media-playback-start")}, QStyle::SP_MediaPlay, this));
    m_ui->cropButton->setIcon(
        themedIcon({QStringLiteral("edit-cut"), QStringLiteral("transform-crop")},
                   QStyle::SP_DialogApplyButton,
                   this));
    m_ui->broadcastBufferedButton->setIcon(
        themedIcon({QStringLiteral("audio-speakers"), QStringLiteral("media-playback-start")},
                   QStyle::SP_ArrowForward,
                   this));
    m_ui->broadcastSaveButton->setIcon(
        themedIcon({QStringLiteral("document-save-all"), QStringLiteral("document-save")},
                   QStyle::SP_DialogSaveButton,
                   this));
    m_ui->saveOnlyButton->setIcon(
        themedIcon({QStringLiteral("document-save")}, QStyle::SP_DialogSaveButton, this));
    m_ui->discardButton->setIcon(
        themedIcon({QStringLiteral("edit-delete"), QStringLiteral("user-trash")},
                   QStyle::SP_TrashIcon,
                   this));

    m_ui->mainTabWidget->setTabIcon(
        m_ui->mainTabWidget->indexOf(m_ui->homeTab),
        themedIcon({QStringLiteral("network-wireless"), QStringLiteral("computer")},
                   QStyle::SP_DriveNetIcon,
                   this));
    m_ui->mainTabWidget->setTabIcon(
        m_ui->mainTabWidget->indexOf(m_ui->mapTab),
        themedIcon({QStringLiteral("mark-location"), QStringLiteral("emblem-photos")},
                   QStyle::SP_FileDialogContentsView,
                   this));
    m_ui->mainTabWidget->setTabIcon(
        m_ui->mainTabWidget->indexOf(m_ui->messagesTab),
        themedIcon({QStringLiteral("audio-speakers"), QStringLiteral("folder-sound")},
                   QStyle::SP_MediaPlay,
                   this));
    m_ui->mainTabWidget->setTabIcon(
        m_ui->mainTabWidget->indexOf(m_ui->liveTab),
        themedIcon({QStringLiteral("audio-input-microphone"), QStringLiteral("microphone-sensitivity-high")},
                   QStyle::SP_MediaVolume,
                   this));
    m_ui->mainTabWidget->setTabIcon(
        m_ui->mainTabWidget->indexOf(m_ui->recordingTab),
        themedIcon({QStringLiteral("media-record"), QStringLiteral("document-edit")},
                   QStyle::SP_DialogSaveButton,
                   this));
}

void MainWindow::rememberSelectedAsset() {
    const QString assetId = selectedAssetId();
    if (!assetId.isEmpty()) {
        m_selectedAssetId = assetId;
    }
}

void MainWindow::restoreSelectedAssetSelection() {
    if (m_selectedAssetId.isEmpty()) {
        updateAssetButtons();
        return;
    }

    auto* libraryModel = qobject_cast<AssetLibraryModel*>(m_controller->libraryModel());
    if (!libraryModel) {
        updateAssetButtons();
        return;
    }

    const int row = libraryModel->findRowByAssetId(m_selectedAssetId);
    if (row < 0) {
        updateAssetButtons();
        return;
    }

    const QModelIndex index = libraryModel->index(row, 0);
    if (!index.isValid()) {
        updateAssetButtons();
        return;
    }

    m_ui->assetListView->setCurrentIndex(index);
    m_ui->assetListView->scrollTo(index, QAbstractItemView::PositionAtCenter);
    updateAssetButtons();
}

void MainWindow::playSelectedAsset(const QString& mode) {
    const QString assetId = selectedAssetId();
    if (assetId.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Sélectionnez d'abord un message audio"), 3000);
        return;
    }

    bool intervalOk = true;
    const int intervalMs = playbackIntervalMs(&intervalOk);
    if (!intervalOk) {
        statusBar()->showMessage(QStringLiteral("L'intervalle doit être un nombre entier"), 3000);
        return;
    }

    bool durationOk = true;
    const int durationMs = playbackDurationMs(&durationOk);
    if (!durationOk) {
        statusBar()->showMessage(QStringLiteral("La durée doit être un nombre entier de secondes"), 3000);
        return;
    }

    auto* libraryModel = qobject_cast<AssetLibraryModel*>(m_controller->libraryModel());
    const QString assetLabel = libraryModel
        ? libraryModel->data(m_ui->assetListView->currentIndex(), AssetLibraryModel::LabelRole).toString()
        : assetId;
    const QString modeText = mode == QStringLiteral("repeat")
        ? QStringLiteral("Le message \"%1\" va être lancé en boucle sur les poteaux sélectionnés.").arg(assetLabel)
        : QStringLiteral("Le message \"%1\" va être lancé sur les poteaux sélectionnés.").arg(assetLabel);
    requestBroadcastConfirmation(ConfirmationScope::Messages,
                                 modeText,
                                 [this, assetId, mode, intervalMs, durationMs]() {
                                     m_controller->playAsset(assetId, mode, intervalMs, durationMs);
                                 });
}

void MainWindow::requestBroadcastConfirmation(ConfirmationScope scope,
                                              const QString& actionText,
                                              std::function<void()> action,
                                              const QString& detailText) {
    const QStringList baseNames = selectedBroadcastBaseNames();
    if (baseNames.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Sélectionnez au moins une base en ligne"), 3000);
        return;
    }

    m_confirmationScope = scope;
    m_pendingConfirmationAction = std::move(action);
    m_pendingConfirmationText = actionText;
    m_pendingConfirmationDetail = detailText;
    refreshBroadcastConfirmationPanel();
}

void MainWindow::refreshBroadcastConfirmationPanel() {
    if (m_confirmationScope == ConfirmationScope::None || !m_pendingConfirmationAction) {
        if (m_confirmationOverlay) {
            m_confirmationOverlay->hide();
        }
        return;
    }

    QVector<BaseSnapshot> snapshots;
    if (auto* model = baseDirectoryModel()) {
        snapshots = model->selectedOnlineSnapshots();
    }

    QStringList baseLines;
    baseLines.reserve(snapshots.size());
    for (const BaseSnapshot& snapshot : snapshots) {
        const QString description = snapshot.description.trimmed();
        baseLines.push_back(description.isEmpty()
                                ? QStringLiteral("• %1").arg(snapshot.name)
                                : QStringLiteral("• %1\n  %2").arg(snapshot.name, description));
    }

    QString basesText = baseLines.isEmpty()
        ? QStringLiteral("Aucun poteau en ligne n'est sélectionné.")
        : QStringLiteral("%1 poteau(x) vont diffuser.\n\n%2")
              .arg(baseLines.size())
              .arg(baseLines.join(QStringLiteral("\n\n")));
    if (!m_pendingConfirmationDetail.isEmpty()) {
        basesText.append(QStringLiteral("\n\n")).append(m_pendingConfirmationDetail);
    }

    QString titleText;
    switch (m_confirmationScope) {
    case ConfirmationScope::Messages:
        titleText = QStringLiteral("Confirmer la diffusion");
        break;
    case ConfirmationScope::Live:
        titleText = QStringLiteral("Confirmer le direct");
        break;
    case ConfirmationScope::Recording:
        titleText = QStringLiteral("Confirmer l'enregistrement");
        break;
    case ConfirmationScope::None:
        break;
    }

    if (!m_confirmationOverlay || !m_confirmationTitleLabel || !m_confirmationTextLabel
        || !m_confirmationBasesLabel || !m_confirmationConfirmButton) {
        return;
    }

    m_confirmationTitleLabel->setText(titleText);
    m_confirmationTextLabel->setText(m_pendingConfirmationText);
    m_confirmationBasesLabel->setText(basesText);
    m_confirmationConfirmButton->setEnabled(!snapshots.isEmpty());
    m_confirmationOverlay->show();
    m_confirmationOverlay->raise();
}

void MainWindow::clearBroadcastConfirmation() {
    m_confirmationScope = ConfirmationScope::None;
    m_pendingConfirmationAction = {};
    m_pendingConfirmationText.clear();
    m_pendingConfirmationDetail.clear();
    if (m_confirmationOverlay) {
        m_confirmationOverlay->hide();
    }
}

void MainWindow::executePendingBroadcastConfirmation() {
    if (!m_pendingConfirmationAction) {
        return;
    }

    auto action = std::move(m_pendingConfirmationAction);
    clearBroadcastConfirmation();
    action();
}

void MainWindow::updateConfirmationOverlayGeometry() {
    if (!m_confirmationOverlay || !centralWidget()) {
        return;
    }
    m_confirmationOverlay->setGeometry(centralWidget()->rect());
}

void MainWindow::updateBaseSettingsOverlayGeometry() {
    if (!m_baseSettingsOverlay || !centralWidget()) {
        return;
    }
    m_baseSettingsOverlay->setGeometry(centralWidget()->rect());
}

void MainWindow::showBaseSettings(const QModelIndex& index) {
    if (!index.isValid() || !m_baseSettingsOverlay) {
        return;
    }

    auto* model = baseDirectoryModel();
    m_currentSettingsBase = model ? model->snapshotAt(index.row()) : BaseSnapshot{};
    const QString name = index.data(BaseDirectoryModel::NameRole).toString();
    const QString description = index.data(BaseDirectoryModel::DescriptionRole).toString();
    const QString status = index.data(BaseDirectoryModel::StatusRole).toString();
    const QString audioState = index.data(BaseDirectoryModel::AudioStateRole).toString();
    const QString lastSeen = index.data(BaseDirectoryModel::LastSeenTextRole).toString();
    const QString host = index.data(BaseDirectoryModel::HostRole).toString();
    const int controlPort = index.data(BaseDirectoryModel::ControlPortRole).toInt();
    const int megaphonePort = index.data(BaseDirectoryModel::MegaphonePortRole).toInt();
    const double latitude = index.data(BaseDirectoryModel::LatitudeRole).toDouble();
    const double longitude = index.data(BaseDirectoryModel::LongitudeRole).toDouble();
    const bool selected = index.data(BaseDirectoryModel::SelectedRole).toBool();

    const int batteryPercent = 82 - (index.row() % 4) * 7;
    const QString signalQuality = status == QStringLiteral("offline")
        ? QStringLiteral("Indisponible")
        : (status == QStringLiteral("stale") ? QStringLiteral("Faible") : QStringLiteral("Bon"));
    const QString lastBroadcast = audioState == QStringLiteral("playing_prerecorded")
        ? QStringLiteral("En cours")
        : (audioState == QStringLiteral("live_megaphone") ? QStringLiteral("Direct en cours") : QStringLiteral("Aucune diffusion active"));

    m_baseNameLineEdit->setText(name);
    m_baseDescriptionLineEdit->setText(description);
    m_baseLatitudeLineEdit->setText(QString::number(latitude, 'f', 8));
    m_baseLongitudeLineEdit->setText(QString::number(longitude, 'f', 8));

    m_baseSettingsTitleLabel->setText(QStringLiteral("Paramètres - %1").arg(name));
    m_baseStatusInfoLabel->setText(
        QStringLiteral(
            "<b>État</b><br>"
            "Ciblé : %1<br>"
            "Statut : %2<br>"
            "Signal : %3<br>"
            "Batterie : %4%<br>"
            "Vie estimée : %5 h<br>"
            "Dernière diffusion : %6")
            .arg(selected ? QStringLiteral("oui") : QStringLiteral("non"),
                 status,
                 signalQuality,
                 QString::number(batteryPercent),
                 QString::number(4 + (batteryPercent / 18)),
                 lastBroadcast));
    m_baseNetworkInfoLabel->setText(
        QStringLiteral(
            "<b>Réseau</b><br>"
            "Hôte : %1<br>"
            "Contrôle : %2<br>"
            "Direct : %3<br>"
            "Multicast : 239.42.0.10:19100<br>"
            "Dernière vue : %4")
            .arg(host,
                 QString::number(controlPort),
                 QString::number(megaphonePort),
                 lastSeen.isEmpty() ? QStringLiteral("--:--:--") : lastSeen));

    populateBaseAudioList();
    m_baseSettingsTabWidget->setCurrentIndex(0);
    m_baseSettingsOverlay->show();
    m_baseSettingsOverlay->raise();
}

void MainWindow::hideBaseSettings() {
    if (m_baseSettingsOverlay) {
        m_baseSettingsOverlay->hide();
    }
}

void MainWindow::populateBaseAudioList() {
    if (!m_baseAudioListWidget || m_currentSettingsBase.baseId.isEmpty()) {
        return;
    }

    m_baseAudioListWidget->clear();
    const QVector<AudioAssetMetadata> assets = m_controller->libraryForBase(m_currentSettingsBase);
    if (assets.isEmpty()) {
        auto* item = new QListWidgetItem(QStringLiteral("Aucun audio sur cette base"));
        item->setFlags(Qt::NoItemFlags);
        m_baseAudioListWidget->addItem(item);
        return;
    }

    for (const AudioAssetMetadata& asset : assets) {
        const qint64 seconds = asset.durationMs / 1000;
        auto* item = new QListWidgetItem(
            QStringLiteral("%1\nDurée %2:%3  •  Source %4")
                .arg(asset.label,
                     QString::number(seconds / 60).rightJustified(2, QLatin1Char('0')),
                     QString::number(seconds % 60).rightJustified(2, QLatin1Char('0')),
                     asset.source));
        item->setData(Qt::UserRole, asset.assetId);
        item->setToolTip(QStringLiteral("ID: %1\nFichier: %2").arg(asset.assetId, asset.fileName));
        item->setSizeHint(QSize(0, 54));
        m_baseAudioListWidget->addItem(item);
    }
}

void MainWindow::saveBaseSettings() {
    bool latOk = false;
    bool lonOk = false;
    const double latitude = m_baseLatitudeLineEdit->text().trimmed().toDouble(&latOk);
    const double longitude = m_baseLongitudeLineEdit->text().trimmed().toDouble(&lonOk);
    if (!latOk || !lonOk || m_baseNameLineEdit->text().trimmed().isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Nom ou coordonnées invalides"), 3000);
        return;
    }

    QString error;
    if (!m_controller->updateBaseInfo(m_currentSettingsBase,
                                      m_baseNameLineEdit->text().trimmed(),
                                      m_baseDescriptionLineEdit->text().trimmed(),
                                      latitude,
                                      longitude,
                                      &error)) {
        statusBar()->showMessage(error, 3500);
        return;
    }
    hideBaseSettings();
}

void MainWindow::deleteSelectedBaseAudio() {
    QListWidgetItem* item = m_baseAudioListWidget ? m_baseAudioListWidget->currentItem() : nullptr;
    if (!item || item->data(Qt::UserRole).toString().isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Sélectionne un audio à supprimer"), 2500);
        return;
    }

    QString error;
    if (!m_controller->deleteAssetFromBase(m_currentSettingsBase, item->data(Qt::UserRole).toString(), &error)) {
        statusBar()->showMessage(error, 3500);
        return;
    }
    populateBaseAudioList();
    statusBar()->showMessage(QStringLiteral("Audio supprimé"), 2500);
}

void MainWindow::syncCurrentBaseAudio() {
    QString error;
    const int count = m_controller->syncMissingAssetsToBase(m_currentSettingsBase, &error);
    if (count < 0) {
        statusBar()->showMessage(error, 3500);
        return;
    }
    populateBaseAudioList();
    statusBar()->showMessage(QStringLiteral("%1 audio(s) synchronisé(s)").arg(count), 3000);
}

QStringList MainWindow::selectedBroadcastBaseNames() const {
    auto* model = baseDirectoryModel();
    if (!model) {
        return {};
    }

    QStringList names;
    const auto snapshots = model->selectedOnlineSnapshots();
    for (const BaseSnapshot& snapshot : snapshots) {
        names.push_back(snapshot.name);
    }
    return names;
}

int MainWindow::playbackIntervalMs(bool* ok) const {
    if (m_ui->intervalLineEdit->text().trimmed().isEmpty()) {
        if (ok) {
            *ok = true;
        }
        return 0;
    }
    return m_ui->intervalLineEdit->text().trimmed().toInt(ok);
}

int MainWindow::playbackDurationMs(bool* ok) const {
    const QString text = m_ui->durationLineEdit->text().trimmed();
    if (text.isEmpty()) {
        if (ok) {
            *ok = true;
        }
        return -1;
    }

    bool localOk = false;
    const int seconds = text.toInt(&localOk);
    if (ok) {
        *ok = localOk;
    }
    return localOk ? seconds * 1000 : -1;
}

QString MainWindow::selectedAssetId() const {
    const QModelIndex index = m_ui->assetListView->currentIndex();
    if (!index.isValid()) {
        return {};
    }

    return m_controller->libraryModel()->data(index, AssetLibraryModel::AssetIdRole).toString();
}

BaseDirectoryModel* MainWindow::baseDirectoryModel() const {
    return qobject_cast<BaseDirectoryModel*>(m_controller->baseModel());
}

} // namespace apping
