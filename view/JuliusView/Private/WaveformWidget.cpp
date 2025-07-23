#include "Public/WaveformWidget.h"
#include <sndfile.h>
#include <QPainter>
#include <QPaintEvent>
#include <QDebug>
#include <QUrl>
#include <cmath>
#include <QPainterPath>


WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent),
  mediaPlayer_(new QMediaPlayer(this)),
  audioOutput_(new QAudioOutput(this))
{
  setMinimumHeight(120);
  mediaPlayer_->setAudioOutput(audioOutput_);

  connect(mediaPlayer_, &QMediaPlayer::positionChanged,
    this, [this](qint64 ms) {
      playheadSample_ = int(ms * sampleRate_ / 1000);
      emit positionChanged(ms);
      update();
  });

  connect(mediaPlayer_, &QMediaPlayer::mediaStatusChanged,
    this, [this](QMediaPlayer::MediaStatus status) {
      if (status == QMediaPlayer::EndOfMedia) {
	// 再生停止＆先頭に戻す
	mediaPlayer_->stop();
	mediaPlayer_->setPosition(0);
	playheadSample_ = 0;
	emit positionChanged(0);
	update();
      }
  });
}

bool WaveformWidget::loadFromFile(const QString& path)
{
  samples_.clear();
  viewOffset_ = 0;
  sampleRate_ = 0;

  // WAV を libsndfile で読み込み
  SF_INFO sfinfo;
  SNDFILE* file = sf_open(path.toStdString().c_str(), SFM_READ, &sfinfo);
  if (!file) {
    qWarning() << "Failed to open WAV file:" << path;
    return false;
  }
  sampleRate_ = sfinfo.samplerate;
  int channels = sfinfo.channels;
  sf_count_t frames = sfinfo.frames;
  std::vector<float> buffer(frames * channels);
  sf_count_t readcount = sf_readf_float(file, buffer.data(), frames);
  sf_close(file);
  if (readcount <= 0) {
    qWarning() << "Failed to read WAV data:" << path;
    return false;
  }

  // モノラル化して samples_ に格納
  samples_.resize(frames);
  for (sf_count_t i = 0; i < frames; ++i) {
    float sum = 0.0f;
    for (int ch = 0; ch < channels; ++ch) {
      sum += buffer[i * channels + ch];
    }
    samples_[i] = sum / channels;
  }

  emit waveformLoaded();

  // QMediaPlayer のソース設定
  mediaPlayer_->setSource(QUrl::fromLocalFile(path));
  return true;
}

void WaveformWidget::startPlayback()
{
  if (mediaPlayer_)
    mediaPlayer_->play();
}

void WaveformWidget::stopPlayback()
{
  if (mediaPlayer_)
    mediaPlayer_->stop();
}

void WaveformWidget::pausePlayback()
{
  if (mediaPlayer_)
    mediaPlayer_->pause();
}

void WaveformWidget::seek(qint64 positionMs)
{
  if (mediaPlayer_)
    mediaPlayer_->setPosition(positionMs);
}

void WaveformWidget::setViewPosition(int sampleIndex)
{
  viewOffset_ = qBound(0, sampleIndex, samples_.size());
  update();
}


void WaveformWidget::paintEvent(QPaintEvent* /*event*/)
{

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  // 1) 背景グラデ
  QLinearGradient grad(0, 0, 0, height());
  grad.setColorAt(0.0, QColor(40, 40, 50));
  grad.setColorAt(1.0, QColor(20, 20, 30));
  p.fillRect(rect(), grad);

  // --- 波形描画（既存） ---
  if (samples_.isEmpty()) {
    p.setPen(Qt::lightGray);
    p.drawText(rect(), Qt::AlignCenter, tr("(No Wave)"));
    return;
  }

  const int W = width();
  const int H = height();
  const int total = samples_.size();
  const int offset = viewOffset_;
  const int window = total - offset;
  if (window <= 0)
    return;

  // ゼロライン
  p.setPen(QPen(QColor(80, 80, 100), 1, Qt::DashLine));
  p.drawLine(0, H / 2, W, H / 2);

  // 各 X 座標に対応するサンプル範囲の min/max を計算
  struct MinMax { float mi, ma; };
  QVector<MinMax> agg(W, { +1.0f, -1.0f });
  for (int i = 0; i < window; ++i) {
    int x = int(double(i) / window * W);
    float v = samples_[offset + i];
    agg[x].mi = qMin(agg[x].mi, v);
    agg[x].ma = qMax(agg[x].ma, v);
  }

  // 塗りつぶしパス作成
  QPainterPath fillPath;
  fillPath.moveTo(0, H / 2);
  for (int x = 0; x < W; ++x) {
    const int y = H / 2 - int(agg[x].ma * (H / 2));
    fillPath.lineTo(x, y);
  }
  fillPath.lineTo(W - 1, H / 2);
  for (int x = W - 1; x >= 0; --x) {
    const int y = H / 2 - int(agg[x].mi * (H / 2));
    fillPath.lineTo(x, y);
  }
  fillPath.closeSubpath();

  // 半透明で塗りつぶし
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(100, 180, 255, 80));
  p.drawPath(fillPath);

  // 上辺・下辺ライン描画
  p.setPen(QPen(QColor(100, 180, 255), 1));
  for (int x = 0; x < W; ++x) {
    int y1 = H / 2 - int(agg[x].ma * (H / 2));
    int y2 = H / 2 - int(agg[x].mi * (H / 2));
    p.drawLine(x, y1, x, y2);
  }

  const int windowLen = total - viewOffset_;

  // --- 母音マーカー描画（修正） ---
  for (auto& vm : vowelMarkers_) {
    // 1) normalized pos → 実サンプル位置
    int samplePos = int(vm.normPos * total);

    // 2) viewOffset_ 範囲内かチェック
    if (samplePos < viewOffset_ || samplePos >= total)
      continue;

    // 3) 相対位置 → X 座標に変換
    int rel = samplePos - viewOffset_;
    double relNorm = double(samplePos - viewOffset_) / double(windowLen);
    int mx = int(relNorm * W);

    // 色と描画は元のまま
    QColor c;
    if (vm.vowel == "A")
      c = Qt::red;
    else if (vm.vowel == "I")
      c = Qt::green;
    else if (vm.vowel == "U")
      c = Qt::blue;
    else if (vm.vowel == "E")
      c = Qt::yellow;
    else if (vm.vowel == "O")
      c = Qt::magenta;
    else c = Qt::cyan;

    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(mx, H / 2.0), 5, 5);

    // ラベル
    //p.setPen(Qt::white);
    //p.drawText(mx + 6, H / 2.0 - 6, QString::number(vm.normPos, 'f', 2));

    // ラベル (表示モードに応じて)
    QString label;
    switch (labelMode_) {
    case MarkerLabelMode::AIUEON:
      label = vm.vowel;
      break;
    case MarkerLabelMode::CV:
      label = vm.cv;
      break;
    case MarkerLabelMode::dBFS:
      label = QString::number(vm.dbfs, 'f', 1);
      break;
    case MarkerLabelMode::RelSum:
      label = QString::number(vm.relSum, 'f', 2);
      break;
    case MarkerLabelMode::RelMax:
      label = QString::number(vm.relMax, 'f', 2);
      break;
    case MarkerLabelMode::RelVowelSum:
      label = QString::number(vm.relVowelSum, 'f', 2);
      break;
    case MarkerLabelMode::NormPos:
      label = QString::number(vm.normPos, 'f', 2);
      break;
    case MarkerLabelMode::None:
    default:
      break;
    }
    if (!label.isEmpty()) {
      p.setPen(Qt::white);
      p.drawText(mx + 6, H / 2.0 - 6, label);

    }

  }

  // 再生位置マーカー（縦線）
  if (playheadSample_ >= viewOffset_) {
    int relSample = playheadSample_ - viewOffset_;
    int x = int(double(relSample) / (total - viewOffset_) * W);
    p.setPen(QPen(Qt::yellow, 2));
    p.drawLine(x, 0, x, H);
  }


}

void WaveformWidget::setVowelMarkers(const QVector<VowelMarker>& markers)
{
  vowelMarkers_ = markers;
  update();
}

void WaveformWidget::setMarkerLabelMode(MarkerLabelMode m)
{
  if (labelMode_ != m)
  {
    labelMode_ = m;
    update();
  }
}


