
#include "Public/ADXLipModel.h"

#include <QFile>
#include <QTextStream>

ADXLipModel::ADXLipModel(QObject* parent) : QAbstractTableModel(parent)
{
}

// アイテムのフラグを返す（編集可能フラグを追加）
Qt::ItemFlags ADXLipModel::flags(const QModelIndex & index) const
{
  if (!index.isValid())
    return Qt::NoItemFlags;
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

// 編集時に呼ばれる。frames_ を書き換えて dataChanged を通知する
bool ADXLipModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (role != Qt::EditRole || !index.isValid())
    return false;

  auto& f = frames_[index.row()];
  switch (index.column()) {
  case 0:
    f.frameIndex = value.toInt();
    break;
  case 1:
    f.msec = value.toInt();
    break;
  case 2:
    f.width = value.toDouble();
    break;
  case 3:
    f.height = value.toDouble();
    break;
  case 4:
    f.tongue = value.toDouble();
    break;
  case 5:
    f.A = value.toDouble();
    break;
  case 6:
    f.I = value.toDouble();
    break;
  case 7:
    f.U = value.toDouble();
    break;
  case 8:
    f.E = value.toDouble();
    break;
  case 9:
    f.O = value.toDouble();
    break;
  case 10:
    f.N = value.toDouble();
    break;
  case 11:
    f.vol_dB = value.toDouble();
    break;
  default: return false;
  }

  // ビューに更新を通知
  emit dataChanged(index, index);
  return true;
}


int ADXLipModel::rowCount(const QModelIndex&) const
{
  return frames_.size();
}

int ADXLipModel::columnCount(const QModelIndex&) const
{
  // frameIndex, msec, width, height, tongue, A, I, U, E, O, N, vol_dB
  return fixedParamCount();
}

QVariant ADXLipModel::data(const QModelIndex& index, int role) const
{

  if (!index.isValid() || role != Qt::DisplayRole)
    return QVariant();
  const auto& f = frames_.at(index.row());
  switch (index.column()) {
  case 0: return f.frameIndex;
  case 1: return f.msec;
  case 2: return f.width;
  case 3: return f.height;
  case 4: return f.tongue;
  case 5: return f.A;
  case 6: return f.I;
  case 7: return f.U;
  case 8: return f.E;
  case 9: return f.O;
  case 10: return f.N;
  case 11: return f.vol_dB;
  default: break;
  }
  return QVariant();
}

QVariant ADXLipModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    return {};

  static const char* headers[] = {
    "frameIndex", "msec", "width", "height", "tongue",
    "A", "I", "U", "E", "O", "N", "vol_dB"
  };

  auto max = fixedParamCount();
  if (section >= 0 && section < max)
    return headers[section];
  return {};
}

void ADXLipModel::setFrames(const QVector<ADXLipFrame>& frames)
{
  beginResetModel();
  frames_ = frames;
  endResetModel();
}

bool ADXLipModel::exportToFile(const QString& filePath, const QString& wavPath) const
{
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  QTextStream out(&file);
  // 固定小数点で6桁に揃える
  out.setRealNumberNotation(QTextStream::FixedNotation);
  out.setRealNumberPrecision(6);

  out << "// input: " << wavPath << "\n";
  out << "// framerate: 60 [fps]\n";
  //out << "// frame count, msec, width(0-1 def=0.000), height(0-1 def=0.000), tongue(0-1 def=0.000), A(0-1), I(0-1), U(0-1), E(0-1), O(0-1), N(0-1), Vol(dB)\n";
  out << "// frame count, msec, width(0-1 def=0.000), height(0-1 def=0.000), tongue(0-1 def=0.000), A(0-1), I(0-1), U(0-1), E(0-1), O(0-1), Vol(dB)\n";

  for (const auto& f : frames_) {
    out << f.frameIndex << ", "
      << f.msec << ", "
      << f.width << ", "
      << f.height << ", "
      << f.tongue << ", "
      << f.A << ", "
      << f.I << ", "
      << f.U << ", "
      << f.E << ", "
      << f.O << ", "
      //<< f.N << ", "
      << f.vol_dB << "\n";
  }
  return true;
}


