
#include "Public/JuliusAnalyzeWindow.h"

#include <QFileDialog>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QApplication>
#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>
#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDebug>
#include <windows.h>

// sqrt, log10, fabs
#include <cmath>
#include <algorithm>

#include "Public/NoCommitDelegate.h"

static constexpr int FRAME_SHIFT_SAMPLES = 160; // Julius の 10ms フレーム幅


// CP932 (Shift-JIS) バイト列を QString に変換するヘルパー
static QString decodeCp932(const QByteArray& raw)
{
  if (raw.isEmpty()) return QString();
  int wlen = ::MultiByteToWideChar(932, MB_ERR_INVALID_CHARS,
    raw.constData(), raw.size(), nullptr, 0);
  if (wlen <= 0) return QString::fromLocal8Bit(raw);
  std::wstring wbuf(wlen, L'\0');
  ::MultiByteToWideChar(932, MB_ERR_INVALID_CHARS,
    raw.constData(), raw.size(), wbuf.data(), wlen);
  return QString::fromWCharArray(wbuf.data(), wlen);
}

// Julius のログは UTF-8 か CP932 のどちらかで出るため、両方試す
static QString decodeJuliusLog(const QByteArray& raw)
{
  // まず UTF-8 でデコード
  QString text = QString::fromUtf8(raw);
  // 置換文字がある、または空なら CP932 で再デコード
  if (text.contains(QChar::ReplacementCharacter) || text.trimmed().isEmpty()) {
    text = decodeCp932(raw);
  }
  return text;
}


namespace {
  QString consonantCodeToRoman(const QString& ph)
  {
    // Julius の base phone コードをざっくりローマ字っぽく整形
    // 必要に応じてここを拡張してください。
    const QString P = ph.toUpper();

    if (P == "SH") return "SH";
    if (P == "CH") return "CH";
    if (P == "TS") return "TS";
    if (P == "J")  return "J";
    if (P == "F")  return "F";
    if (P == "TH") return "TH";  // ないかも
    if (P == "KY") return "KY";  // 拡張例
    if (P == "BY") return "B";   // とりあえず最初文字に丸めるなど

    // 1文字で十分なケース
    if (!P.isEmpty())
      return P.left(1);

    return QString();
  }
} // anon namespace


JuliusAnalyzeWindow::JuliusAnalyzeWindow(QWidget* parent) : QMainWindow(parent)
{
  isPlaying_ = false;
  setupUi();
  setupConnections();
}


JuliusAnalyzeWindow::~JuliusAnalyzeWindow() = default;


void JuliusAnalyzeWindow::setupUi()
{
  auto* central = new QWidget(this);
  setCentralWidget(central);

  loadWavButton_ = new QPushButton(tr("Import Wav"), this);
  exportAdxButton_ = new QPushButton(tr("Export ADXLip"), this);
  importAdxButton_ = new QPushButton(tr("Import ADXLip"), this);

  checkBoxSilenceCorrection_ = new QCheckBox(tr("Silence Collect"), this);
  checkBoxSilenceCorrection_->setChecked(false);

  auto* topLayout = new QHBoxLayout;
  topLayout->addWidget(loadWavButton_);
  topLayout->addWidget(exportAdxButton_);
  topLayout->addWidget(importAdxButton_);
  topLayout->addWidget(checkBoxSilenceCorrection_);
  topLayout->addStretch();

  waveformWidget_ = new WaveformWidget(this);
  positionSlider_ = new QSlider(Qt::Horizontal, this);
  auto* waveformArea = new QVBoxLayout;
  waveformArea->addWidget(waveformWidget_);
  waveformArea->addWidget(positionSlider_);

  adxModel_ = new ADXLipModel(this);
  adxTableView_ = new QTableView(this);
  adxTableView_->setModel(adxModel_);
  adxTableView_->setSelectionBehavior(QAbstractItemView::SelectItems);
  adxTableView_->setSelectionMode(QAbstractItemView::SingleSelection);
  adxTableView_->setAlternatingRowColors(true);

  auto* delegate = new NoCommitDelegate(this);
  adxTableView_->setItemDelegate(delegate);

  adxTableView_->setEditTriggers(QAbstractItemView::DoubleClicked);
  // | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed

  for (int i = 1; i < ADXLipModel::fixedParamCount(); ++i)
  {
    adxTableView_->setColumnWidth(i, 60);
  }

  // テーブルをアクション付きコンテキストメニュー化
  adxTableView_->setContextMenuPolicy(Qt::ActionsContextMenu);

  auto* middleLayout = new QHBoxLayout;
  middleLayout->addLayout(waveformArea, 2);
  middleLayout->addWidget(adxTableView_, 3);

  playButton_ = new QPushButton(tr("Play"), this);
  pauseButton_ = new QPushButton(tr("Pause"), this);
  stopButton_ = new QPushButton(tr("Stop"), this);
  runJuliusButton_ = new QPushButton(tr("Analize Julius"), this);

  labelModeCombo_ = new QComboBox(this);
  labelModeCombo_->addItem(tr("AIUEON"), int(MarkerLabelMode::AIUEON));
  labelModeCombo_->addItem(tr("CV"), int(MarkerLabelMode::CV));
  labelModeCombo_->addItem(tr("dBFS"), int(MarkerLabelMode::dBFS));
  labelModeCombo_->addItem(tr("RelSum"), int(MarkerLabelMode::RelSum));
  labelModeCombo_->addItem(tr("RelMax"), int(MarkerLabelMode::RelMax));
  labelModeCombo_->addItem(tr("RelVowelSum"), int(MarkerLabelMode::RelVowelSum));
  labelModeCombo_->addItem(tr("NormPos"), int(MarkerLabelMode::NormPos));
  labelModeCombo_->addItem(tr("None"), int(MarkerLabelMode::None));


  auto* bottomLayout = new QHBoxLayout;
  bottomLayout->addWidget(playButton_);
  bottomLayout->addWidget(pauseButton_);
  bottomLayout->addWidget(stopButton_);
  bottomLayout->addWidget(runJuliusButton_);
  bottomLayout->addWidget(labelModeCombo_);
  bottomLayout->addStretch();

  auto* mainLayout = new QVBoxLayout(central);
  mainLayout->addLayout(topLayout);
  mainLayout->addLayout(middleLayout);
  mainLayout->addLayout(bottomLayout);

  resize(1400, 600);
}


void JuliusAnalyzeWindow::setupConnections()
{
  connect(loadWavButton_, &QPushButton::clicked, this, &JuliusAnalyzeWindow::onLoadWav);
  connect(exportAdxButton_, &QPushButton::clicked, this, &JuliusAnalyzeWindow::onExportADXLip);
  connect(importAdxButton_, &QPushButton::clicked, this, &JuliusAnalyzeWindow::onImportADXLip);

  connect(playButton_, &QPushButton::clicked, this, &JuliusAnalyzeWindow::onPlay);
  connect(pauseButton_, &QPushButton::clicked, this, &JuliusAnalyzeWindow::onPause);
  connect(stopButton_, &QPushButton::clicked, this, &JuliusAnalyzeWindow::onStop);

  connect(runJuliusButton_, &QPushButton::clicked, this, &JuliusAnalyzeWindow::onRunJulius);

  // マーカラベル表示切替
  connect(labelModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int row)
  {
    const QVariant v = labelModeCombo_->itemData(row);
    if (!v.isValid())
      return;
    auto mode = static_cast<MarkerLabelMode>(v.toInt());
    waveformWidget_->setMarkerLabelMode(mode);
  });


  // 波形読み込み完了時にスライダーの範囲を更新
  connect(waveformWidget_, &WaveformWidget::waveformLoaded, this, &JuliusAnalyzeWindow::updateSliderRangeFromWave);

  // 再生中の位置が変わったらスライダーを動かす
  connect(waveformWidget_, &WaveformWidget::positionChanged, positionSlider_, &QSlider::setValue);

  // スライダーを動かしたら波形をシーク
  connect(positionSlider_, &QSlider::sliderMoved, waveformWidget_, &WaveformWidget::seek);

  connect(checkBoxSilenceCorrection_, &QCheckBox::toggled, this, [&](bool on)
  {
    isApplySilenceCorrection = on;
  });
}


void JuliusAnalyzeWindow::updateSliderRangeFromWave()
{
  const int totalSamples = waveformWidget_->totalSampleCount();
  const int rate = waveformWidget_->sampleRate();// 追加
  const int totalMs = (rate > 0 ? totalSamples * 1000 / rate : 0);
  positionSlider_->setRange(0, totalMs);
  positionSlider_->setValue(0);
}


void JuliusAnalyzeWindow::onLoadWav()
{
  const QString path = QFileDialog::getOpenFileName(this, tr("Select WAV File"), QString(), tr("WAV Files (*.wav)"));
  if (path.isEmpty()) {
    return;
  }

  loadedWavPath_ = path;
  if (!waveformWidget_->loadFromFile(path)) {
    QMessageBox::warning(this, tr("Error"), tr("Failed Import Wav"));
    return;
  }

  updateSliderRangeFromWave();
  statusBar()->showMessage(tr("Complete Import: %1").arg(path), 5000);
}


void JuliusAnalyzeWindow::onExportADXLip()
{
  const QString outPath = QFileDialog::getSaveFileName(this, tr("Export ADXLip"), QString(), tr("ADXLip Files (*.adxlip)"));
  if (outPath.isEmpty()) return;

  if (!adxModel_->exportToFile(outPath, loadedWavPath_)) {
    QMessageBox::warning(this, tr("Error"), tr("Failed Export ADXLip"));
    return;
  }
  statusBar()->showMessage(tr("ADXLip Success: %1").arg(outPath), 5000);
}


void JuliusAnalyzeWindow::onImportADXLip()
{
  qDebug() << "[ImportADXLip] start";

  // 1) ファイル選択
  const QString path = QFileDialog::getOpenFileName(
    this,
    tr("Import ADXLip File"),
    QString(),
    tr("ADXLip Files (*.adxlip);;All Files (*)"));
  if (path.isEmpty()) {
    qDebug() << "[ImportADXLip] canceled by user";
    return;
  }
  qDebug() << "[ImportADXLip] selected file:" << path;

  // 2) ファイルオープン
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "[ImportADXLip] cannot open file:" << path;
    QMessageBox::warning(this,
      tr("Error"),
      tr("Cannot open file:\n%1").arg(path));
    return;
  }

  QTextStream in(&file);

  // 3) ヘッダー読み飛ばし
  for (int i = 1; i <= 3; ++i) {
    if (in.atEnd()) {
      qWarning() << "[ImportADXLip] unexpected EOF while skipping header at line" << i;
      break;
    }
    QString hdr = in.readLine();
    qDebug() << "[ImportADXLip] skip header line" << i << ":" << hdr;
  }

  QVector<ADXLipFrame> frames;
  int lineNum = 4;
  while (!in.atEnd()) {
    QString line = in.readLine();
    qDebug() << "[ImportADXLip] read line" << lineNum << ":" << line;
    ++lineNum;

    line = line.trimmed();
    if (line.isEmpty()) {
      qDebug() << "[ImportADXLip] skip empty line" << lineNum - 1;
      continue;
    }

    // 4) カンマ区切り
    QStringList parts = line.split(',', Qt::SkipEmptyParts);
    qDebug() << "[ImportADXLip] parts count =" << parts.size();

    // 最低限の列数チェック（11列以上）
    if (parts.size() < ADXLipModel::fixedParamCount() - 1) {
      qWarning() << "[ImportADXLip] too few columns on line" << lineNum - 1;
      continue;
    }

    // 5) パース
    ADXLipFrame f;
    bool ok = true;
    f.frameIndex = parts[0].toInt(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse frameIndex failed at line" << lineNum - 1;
    f.msec = parts[1].toInt(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse msec failed at line" << lineNum - 1;
    f.width = parts[2].toDouble(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse width failed at line" << lineNum - 1;
    f.height = parts[3].toDouble(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse height failed at line" << lineNum - 1;
    f.tongue = parts[4].toDouble(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse tongue failed at line" << lineNum - 1;
    f.A = parts[5].toDouble(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse A failed at line" << lineNum - 1;
    f.I = parts[6].toDouble(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse I failed at line" << lineNum - 1;
    f.U = parts[7].toDouble(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse U failed at line" << lineNum - 1;
    f.E = parts[8].toDouble(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse E failed at line" << lineNum - 1;
    f.O = parts[9].toDouble(&ok);
    //if (!ok) qWarning() << "[ImportADXLip] parse O failed at line" << lineNum - 1;

    if (parts.size() >= ADXLipModel::fixedParamCount()) {
      // 12列：N + vol_dB
      f.N = parts[10].toDouble(&ok);
      //if (!ok) qWarning() << "[ImportADXLip] parse N failed at line" << lineNum - 1;
      f.vol_dB = parts[11].toDouble(&ok);
      //if (!ok) qWarning() << "[ImportADXLip] parse vol_dB failed at line" << lineNum - 1;
    }
    else {
      // 11列：N=0 + last column → vol_dB
      f.N = 0.0;
      f.vol_dB = parts[10].toDouble(&ok);
      //if (!ok) qWarning() << "[ImportADXLip] parse vol_dB failed at line" << lineNum - 1;
    }

    frames.append(f);
    //qDebug() << "[ImportADXLip] appended frame" << f.frameIndex;
  }

  file.close();
  qDebug() << "[ImportADXLip] total frames loaded =" << frames.size();

  // 6) モデルにセット & UI 更新
  adxModel_->setFrames(frames);
  adxModel_->layoutChanged();

  qDebug() << "[ImportADXLip] done";
}


void JuliusAnalyzeWindow::onPlay()
{
  waveformWidget_->startPlayback();
  statusBar()->showMessage(tr("Play"), 2000);
}

void JuliusAnalyzeWindow::onPause()
{
  waveformWidget_->pausePlayback();
  statusBar()->showMessage(tr("Pause"), 2000);
}

void JuliusAnalyzeWindow::onStop()
{
  waveformWidget_->stopPlayback();
  statusBar()->showMessage(tr("Stop"), 2000);
}


/// <summary>
/// juliusの解析を行う
/// </summary>
void JuliusAnalyzeWindow::onRunJulius()
{

  if (loadedWavPath_.isEmpty()) {
    QMessageBox::warning(this, tr("Error"), tr("WAV must be loaded first"));
    return;
  }

  // QProcess を作成
  auto* proc = new QProcess(this);

  vowelMarkers_.clear();
  connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
    QByteArray raw = proc->readAllStandardOutput();

    // 1) 生ログをまず UTF-8 風につくり直し
    QString rawText = QString::fromLocal8Bit(raw);

    // 2) 表示用には文字化け対策＋日本語のみフィルタ
    const QString disp = decodeJuliusLog(raw);
    if (!disp.isEmpty()) {
      qDebug().noquote() << disp;
      statusBar()->showMessage(disp, 0);
    }

    // 3) 行ごとに分割してパース
    for (const QString& line : rawText.split(u'\n', Qt::SkipEmptyParts)) {
      parseForcedAlignment(line);
    }

  });


  // stderr の全ログ出力に戻す
  connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
    QByteArray raw = proc->readAllStandardError();
    QString errText = decodeJuliusLog(raw);
    qDebug().noquote() << errText;
    statusBar()->showMessage(errText, 0);
  });

  connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QProcess::deleteLater);

  QDir baseDir(QCoreApplication::applicationDirPath());

  // １階層上 (…/x64)
  baseDir.cdUp();
  // さらに１階層上 (…/JuliusView) ← ここが thirdparty の直上
  baseDir.cdUp();

  const QString juliusExe = QDir(baseDir).filePath("thirdparty/julius-4.6-win32/bin/julius.exe");
  const QString mainJconf = baseDir.filePath("thirdparty/dictation-kit/main.jconf");
  const QString amDnnJconf = baseDir.filePath("thirdparty/dictation-kit/am-dnn.jconf");
  const QString dnnConf = baseDir.filePath("thirdparty/dictation-kit/julius.dnnconf");
    
  // ファイル存在チェック
  for (auto p : { juliusExe, mainJconf, amDnnJconf, dnnConf }) {
    if (!QFile::exists(p)) {
      QMessageBox::critical(this, tr("Error"), tr("File not found:\n%1").arg(p));
      return;
    }
  }


  // WAV 読み込み後に waveformWidget_->sampleRate() で得られるサンプルレート
  const int rate = waveformWidget_->sampleRate();
  const int shift = int(rate * 0.01); // 10ms 分のサンプル数
  const int period = int(10000000.0 / rate); // ナノ秒単位

  QStringList args;
  args << "-C" << mainJconf
    << "-C" << amDnnJconf
    << "-dnnconf" << dnnConf
    << "-smpFreq" << QString::number(rate)    // ← ここで正しいサンプルレートを指定
    << "-smpPeriod" << QString::number(period)  // ← 必要に応じて
    << "-fshift" << QString::number(shift)   // ← ここでフレームシフト幅を指定
    << "-input" << "file";                  // ヘッダー付き WAV モード

  // プロセス開始
  proc->start(juliusExe, args);

  if (!proc->waitForStarted()) {
    QMessageBox::critical(this, tr("Error"), tr("Julius failed to start"));
    proc->deleteLater();
  }

  // WAVファイル名を改行付きで stdin に渡す
  {
    QByteArray inputBytes = loadedWavPath_.toLocal8Bit() + "\n";
    proc->write(inputBytes);
    // stdin を閉じて Julius にEOFを通知
    proc->closeWriteChannel();
  }
}


void JuliusAnalyzeWindow::parseForcedAlignment(const QString& rawLine)
{
  QString line = decodeJuliusLog(rawLine.toLocal8Bit()).trimmed();
  if (line.isEmpty())
    return;

  // ---- forced alignment begin / end ----
  if (line.startsWith("=== begin forced alignment ===")) {
    inForcedBlock_ = true;
    sawPhonemeSection_ = false;
    currentAlignSection_ = AlignSection::None;
    pendingVowels_.clear(); 
    qDebug() << "[parseForcedAlignment] forced alignment BEGIN";
    return;
  }

  if (line.startsWith("=== end forced alignment ===")) {
    if (inForcedBlock_ && sawPhonemeSection_) {
      // phonemeセクションがあった場合のみ計算
      processForcedAlignmentResult();
    }
    inForcedBlock_ = false;
    currentAlignSection_ = AlignSection::None;
    return;
  }

  if (!inForcedBlock_)
    return;

  if (line.startsWith("-- word alignment --")) {
    currentAlignSection_ = AlignSection::Word;
    return;
  }

  if (line.startsWith("-- phoneme alignment --")) {
    currentAlignSection_ = AlignSection::Phoneme;
    sawPhonemeSection_ = true;
    pendingVowels_.clear();
    // phonemeセクション再入時もクリア
    return;
  }

  if (line.startsWith("-- state alignment --")) {
    currentAlignSection_ = AlignSection::State;
    return;
  }

  // ---- data lines: we only care about PHONEME section ----
  if (currentAlignSection_ != AlignSection::Phoneme)
    return;

  // [ from  to ] score unit
  static const QRegularExpression rePhLine(R"(^\s*\[\s*(\d+)\s+(\d+)\]\s+[-\d\.]+\s+([A-Za-z0-9_+\-]+))");

  auto m = rePhLine.match(line);
  if (!m.hasMatch())
    return;

  const int fromFrame = m.captured(1).toInt();
  const int toFrame = m.captured(2).toInt();
  const QString unit = m.captured(3);

  // 中央フレーム
  int midFrame = fromFrame;
  if (toFrame > fromFrame) {
    midFrame = fromFrame + (toFrame - fromFrame) / 2;
  }

  // セグメント分割
  static const QRegularExpression reSplit(R"([-+])");
  const QStringList parts = unit.split(reSplit);

  // center phoneme
  const QString center = (parts.size() >= 2 ? parts.at(1) : parts.at(0));
  const QString ph = center.section('_', 0, 0).toUpper();

  // 左隣（子音候補）
  QString consRoman;
  if (!parts.isEmpty()) {
    const QString leftPh = parts.at(0).section('_', 0, 0).toUpper();
    if (leftPh != ph && leftPh != "SP" && leftPh != "SIL") {
      consRoman = consonantCodeToRoman(leftPh); // 実装は適宜
    }
  }

  // 母音判定
  static const QSet<QString> vowels = { "A","I","U","E","O","N" };
  if (!vowels.contains(ph))
    return;

  const QString cvLabel = consRoman + ph;
  pendingVowels_.append({ ph, fromFrame, toFrame, midFrame, consRoman, cvLabel });
}


void JuliusAnalyzeWindow::processForcedAlignmentResult()
{
  // --- 1) VowelMarker の計算部分 (旧 finalizeVowelMarkers) ---
  vowelMarkers_.clear();

  const int totalSamples = waveformWidget_->totalSampleCount();
  if (totalSamples <= 0) {
    qWarning() << "[processForcedAlignmentResult] no waveform samples.";
    return;
  }
  const QVector<float>& samples = waveformWidget_->samples();

  constexpr double REF = 1.0;
  constexpr double EPS = 1e-12;

  QVector<VowelMarker> vmList;
  vmList.reserve(pendingVowels_.size());

  double sumRms = 0.0, maxRms = 0.0;

  for (const PendingVowel& pv : pendingVowels_) {
    int start = pv.fromFrame * FRAME_SHIFT_SAMPLES;
    int end = (pv.toFrame + 1) * FRAME_SHIFT_SAMPLES;
    if (end <= start)
      end = start + FRAME_SHIFT_SAMPLES;
    start = qBound(0, start, totalSamples);
    end = qBound(0, end, totalSamples);
    int n = end - start;

    double sumSq = 0.0;
    for (int i = 0; i < n; ++i) {
      double v = samples[start + i];
      sumSq += v * v;
    }
    double rms = n > 0 ? std::sqrt(sumSq / n) : 0.0;
    double dbfs = 20.0 * std::log10(max(rms, EPS) / REF);

    int midSample = qBound(0, pv.midFrame * FRAME_SHIFT_SAMPLES, totalSamples - 1);
    double norm = double(midSample) / double(totalSamples);

    VowelMarker vm;
    vm.fromFrame = pv.fromFrame;
    vm.toFrame = pv.toFrame;
    vm.frame = pv.midFrame;
    vm.sampleOffset = midSample;
    vm.normPos = norm;
    vm.vowel = pv.vowel;
    vm.cv = pv.cv;
    vm.rms = rms;
    vm.dbfs = dbfs;
    vmList.append(vm);

    sumRms += rms;
    maxRms = max(maxRms, rms);
  }
  // RelSum / RelMax の二次パス
  for (auto& vm : vmList) {
    vm.relSum = sumRms > 0.0 ? vm.rms / sumRms : 0.0;
    vm.relMax = maxRms > 0.0 ? vm.rms / maxRms : 0.0;
  }
  vowelMarkers_ = std::move(vmList);
  waveformWidget_->setVowelMarkers(vowelMarkers_);
  qDebug() << "[processForcedAlignmentResult] markers =" << vowelMarkers_.size()
    << "sumRms=" << sumRms << "maxRms=" << maxRms;

  const double frame = 60.0f;

  // --- 2) ADXLipFrame の生成部分 (旧 buildAndInjectAdxFrames) ---
  const double sampleRate = waveformWidget_->sampleRate(); // e.g.16kHz
  const double totalSec = double(totalSamples) / sampleRate;
  const int frame60Cnt = int(std::ceil(totalSec * frame));

  QVector<ADXLipFrame> allFrames;
  allFrames.reserve(frame60Cnt);

  const bool applySilenceCorrection = isApplySilenceCorrection;

  // 直前の「非無音フレーム」を保持する変数
  ADXLipFrame lastFrame;
  bool hasLastFrame = false;


  for (int f60 = 0; f60 < frame60Cnt; ++f60) {
    double tSec = double(f60) / frame;
    int julFrame = int(std::floor(tSec * sampleRate / FRAME_SHIFT_SAMPLES));

    ADXLipFrame row;
    row.frameIndex = f60;
    row.msec = int(std::round(tSec * 1000.0));
    // mouth parameters は無視
    row.width = row.height = row.tongue = 0.0;
    // デフォルト全ゼロ
    row.A = row.I = row.U = row.E = row.O = row.N = 0.0;
    row.vol_dB = 0.0;

    // この時間帯にあてはまる vowelMarkers_ を探して値をセット
    for (auto& vm : vowelMarkers_) {
      if (vm.fromFrame <= julFrame && julFrame <= vm.toFrame) {
	// RelMax を AIUEON に割り当て
	if (vm.vowel == "A") row.A = vm.relMax;
	if (vm.vowel == "I") row.I = vm.relMax;
	if (vm.vowel == "U") row.U = vm.relMax;
	if (vm.vowel == "E") row.E = vm.relMax;
	if (vm.vowel == "O") row.O = vm.relMax;
	if (vm.vowel == "N") row.N = vm.relMax;
	row.vol_dB = vm.dbfs;
	break;
      }
    }

    // --- 追加：無音補正（ホールド）ロジック ---
    if (applySilenceCorrection) {
      const bool isSilent = row.A == 0.0 && row.I == 0.0 && row.U == 0.0 && row.E == 0.0 && row.O == 0.0 && row.N == 0.0;
      if (isSilent && hasLastFrame) {
	// 無音なら前回のビズムをコピー
	row.A = lastFrame.A;
	row.I = lastFrame.I;
	row.U = lastFrame.U;
	row.E = lastFrame.E;
	row.O = lastFrame.O;
	row.N = lastFrame.N;
	row.vol_dB = lastFrame.vol_dB;
      }
      else if (!isSilent) {
	// 有声音なら lastFrame を更新
	lastFrame = row;
	hasLastFrame = true;
      }
    }

    allFrames.append(row);
  }



  // モデルにセットして UI 更新
  adxModel_->setFrames(allFrames);
  adxModel_->layoutChanged();

  statusBar()->showMessage(tr("Julius Analyze Success"), 2000);
}


