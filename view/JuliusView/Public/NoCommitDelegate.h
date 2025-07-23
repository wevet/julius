#pragma once
#include <QStyledItemDelegate>
#include <QKeyEvent>
#include <QLineEdit>
#include <QEvent>
#include <QGuiApplication>

class NoCommitDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit NoCommitDelegate(QObject* parent = nullptr)
    : QStyledItemDelegate(parent) {
  }

  // エディタ作成時に自身と内部の QLineEdit に eventFilter をインストール
  QWidget* createEditor(QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const override
  {
    QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
    // 自身にフィルター
    editor->installEventFilter(const_cast<NoCommitDelegate*>(this));
    // 内部に QLineEdit があればそちらにも
    if (auto* le = editor->findChild<QLineEdit*>()) {
      le->installEventFilter(const_cast<NoCommitDelegate*>(this));
    }
    return editor;
  }

  // 編集開始時にモデルの値を取得し、エディタにセット
  void setEditorData(QWidget* editor, const QModelIndex& index) const override
  {
    // EditRole が未実装な場合でも DisplayRole から読み込む
    QVariant value = index.model()->data(index, Qt::EditRole);
    if (!value.isValid() || value.toString().isEmpty()) {
      value = index.model()->data(index, Qt::DisplayRole);
    }
    if (auto* le = qobject_cast<QLineEdit*>(editor)) {
      le->setText(value.toString());
      le->selectAll();
      return;
    }
    // デフォルトの処理
    QStyledItemDelegate::setEditorData(editor, index);
  }

  // キーイベントを監視してコピー・貼り付け・キャンセル・コミットをハンドリング
  bool eventFilter(QObject* obj, QEvent* ev) override
  {
    if (ev->type() == QEvent::KeyPress) {
      auto* ke = static_cast<QKeyEvent*>(ev);
      // Ctrl+C: コピー
      if ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_C) {
	if (auto* le = qobject_cast<QLineEdit*>(obj)) {
	  le->copy();
	  return true;
	}
      }
      // Ctrl+V: 貼り付け
      if ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_V) {
	if (auto* le = qobject_cast<QLineEdit*>(obj)) {
	  le->paste();
	  return true;
	}
      }
      // Esc: 編集キャンセル
      if (ke->key() == Qt::Key_Escape) {
	emit closeEditor(qobject_cast<QWidget*>(obj),
	  QAbstractItemDelegate::RevertModelCache);
	return true;
      }
      // Enter: 編集コミット
      if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
	emit commitData(qobject_cast<QWidget*>(obj));
	emit closeEditor(qobject_cast<QWidget*>(obj),
	  QAbstractItemDelegate::NoHint);
	return true;
      }
    }
    else if (ev->type() == QEvent::FocusOut) {
      // フォーカスアウトでキャンセル
      QWidget* w = qobject_cast<QWidget*>(obj);
      emit closeEditor(w, QAbstractItemDelegate::RevertModelCache);
    }
    return QStyledItemDelegate::eventFilter(obj, ev);
  }
};
