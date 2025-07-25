
#ifndef JULIUS_ANALYZE_WINDOW_H
#define JULIUS_ANALYZE_WINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QTableView>
#include <QSlider>
#include <QString>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>

#include "WaveformWidget.h"
#include "ADXLipModel.h"

class JuliusAnalyzeWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit JuliusAnalyzeWindow(QWidget* parent = nullptr);
  ~JuliusAnalyzeWindow() override;

private slots:
  void onPlay();
  void onPause();
  void onStop();

  void onLoadWav();
  void onExportADXLip();
  void onImportADXLip();
  void onRunJulius();

private:
  void setupUi();
  void setupConnections();
  void updateSliderRangeFromWave();

  // top
  // WAV読込
  QPushButton* loadWavButton_{};
  // ADXLip Export
  QPushButton* exportAdxButton_{};
  // ADXLip Import
  QPushButton* importAdxButton_{};

  // middle
  // Middle-left: 波形
  WaveformWidget* waveformWidget_{};
  // Middle-right: テーブル
  QTableView* adxTableView_{};
  // データモデル
  ADXLipModel* adxModel_{};
  // 波形スクロブ
  QSlider* positionSlider_{};

  // bottom
  QPushButton* playButton_{};
  QPushButton* pauseButton_{};
  QPushButton* stopButton_{};
  QPushButton* runJuliusButton_{};

  QComboBox* labelModeCombo_{};   // 母音マーカラベル表示切替

  bool isPlaying_ = false;
  QString loadedWavPath_;
  QString logBuffer_;

  // 無音補正を行うか？
  QCheckBox* checkBoxSilenceCorrection_{};
  bool isApplySilenceCorrection = false;

  enum class AlignSection { None, Word, Phoneme, State };
  bool inForcedBlock_ = false;
  bool sawPhonemeSection_ = false;
  AlignSection currentAlignSection_ = AlignSection::None;

  QVector<VowelMarker> vowelMarkers_;
  QVector<PendingVowel> pendingVowels_;
  bool inPhAlign_ = false;

  void parseForcedAlignment(const QString& rawLine);
  void processForcedAlignmentResult();


  // threshold
  // AIUEONごとの閾値＋適用ボタン
  QDoubleSpinBox* thresholdA_;
  QDoubleSpinBox* thresholdI_;
  QDoubleSpinBox* thresholdU_;
  QDoubleSpinBox* thresholdE_;
  QDoubleSpinBox* thresholdO_;
  QDoubleSpinBox* thresholdN_;

  QPushButton* btnApplyA_;
  QPushButton* btnApplyI_;
  QPushButton* btnApplyU_;
  QPushButton* btnApplyE_;
  QPushButton* btnApplyO_;
  QPushButton* btnApplyN_;

  void onApplyThresholdA();
  void onApplyThresholdI();
  void onApplyThresholdU();
  void onApplyThresholdE();
  void onApplyThresholdO();
  void onApplyThresholdN();
};

#endif 
