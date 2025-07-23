#ifndef WAVEFORM_WIDGET_H
#define WAVEFORM_WIDGET_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QMediaPlayer>
#include <QAudioOutput>

#include "ADXLipModel.h"


class WaveformWidget : public QWidget
{
  Q_OBJECT
public:
  explicit WaveformWidget(QWidget* parent = nullptr);

  void setVowelMarkers(const QVector<VowelMarker>& markers);

  // WAV 読み込み（libsndfile 使用）
  bool loadFromFile(const QString& path);

  void startPlayback();
  void stopPlayback();
  void pausePlayback();

  // 表示開始サンプル位置変更
  void setViewPosition(int sampleIndex);
  int  totalSampleCount() const { return samples_.size(); }

  int sampleRate() const { return sampleRate_; }
  const QVector<float>& samples() const { return samples_; }

  void setMarkerLabelMode(MarkerLabelMode m);

  MarkerLabelMode markerLabelMode() const { return labelMode_; }

  // 再生位置変更（ms）によるシーク
public slots:
  void seek(qint64 positionMs);


signals:
  // 波形データ読込完了通知（スライダー更新用）
  void waveformLoaded();
  // 再生位置が変更されたとき(ms)
  void positionChanged(qint64 positionMs);

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  QVector<float> samples_; // -1.0 .. 1.0
  int sampleRate_{ 0 };
  int viewOffset_{ 0 };
  int playheadSample_{ -1 }; // 再生位置をサンプル単位で保持

  QMediaPlayer* mediaPlayer_{ nullptr };
  QAudioOutput* audioOutput_{ nullptr };

  QVector<VowelMarker> vowelMarkers_;
  MarkerLabelMode labelMode_{ MarkerLabelMode::AIUEON };
};

#endif
