
#ifndef ADXLIP_MODEL_H
#define ADXLIP_MODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>

/**
 * マーカラベル表示モード
 */
enum class MarkerLabelMode {
  AIUEON = 0,   ///< 元の母音文字 (A/I/U/E/O/N)
  CV,      // 子音＋母音 ("KA","TE"...)
  dBFS,         ///< 20*log10(RMS)
  RelSum,       ///< RMS / (全母音 RMS 合計)
  RelMax,       ///< RMS / (最大母音 RMS)
  RelVowelSum,
  NormPos, // デバッグ用（現状）
  None
};

struct ADXLipFrame {
  int frameIndex = 0;
  int msec = 0;
  double width = 0.0, height = 0.0, tongue = 0.0;
  double A = 0.0, I = 0.0, U = 0.0, E = 0.0, O = 0.0, N = 0.0;
  double vol_dB = 0.0;
};


// JuliusAnalyzeWindow クラス内（private: あたり）
struct PendingVowel {
  QString vowel;
  int fromFrame;
  int toFrame;
  int midFrame;
  QString consonant; // 左セグメント抽出 (ex. "K","S","M"...)
  QString cv;        // consonant + vowel (ex. "KA")
};

/**
 * 母音マーカー情報
 *
 * frame       : Julius のフレーム番号
 * sample      : サンプル先頭位置 (frame * 160)
 * normPos     : 0..1 波形全体に対する正規化位置
 * vowel       : "A" 等
 * rms         : 該当母音区間 RMS (リニア振幅, 0..1)
 * dbfs        : dBFS (フルスケール = 1.0) 20*log10(rms)  rms==0 -> -inf
 * relSum      : RMS / 全母音 RMS 合計
 * relMax      : RMS / 全母音 RMS 最大値
 */
struct VowelMarker {

  int fromFrame{ 0 };
  int toFrame{ 0 };

  int frame;
  int sampleOffset;
  double normPos;
  QString vowel;

  double rms{ 0.0 };
  double peak{ 0.0 };
  double dbfs{ 0.0 };

  // 相対値
  double relMax{ 0.0 };      // RMS / global max RMS
  double relSum{ 0.0 };      // RMS / ΣRMS(all markers)
  double relVowelSum{ 0.0 }; // RMS / ΣRMS(vowel group)

  QString cv;              // 例: "KA","TE","MI"…


};




class ADXLipModel : public QAbstractTableModel
{
  Q_OBJECT
public:
  explicit ADXLipModel(QObject* parent = nullptr);

  Qt::ItemFlags flags(const QModelIndex& index) const override;

  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

  static const int fixedParamCount() { return 12; }

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  void setFrames(const QVector<ADXLipFrame>& frames);
  const QVector<ADXLipFrame>& frames() const { return frames_; }

  bool exportToFile(const QString& filePath, const QString& wavPath) const;

private:
  QVector<ADXLipFrame> frames_;
};

#endif

