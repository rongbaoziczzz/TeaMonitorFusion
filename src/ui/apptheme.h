#pragma once

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QString>

namespace AppTheme {

inline QIcon tintedStandardIcon(QStyle *style,
                                QStyle::StandardPixmap standardPixmap,
                                const QColor &color = QColor(Qt::white))
{
    if (!style) {
        return {};
    }
    QPixmap pixmap = style->standardIcon(standardPixmap).pixmap(18, 18);
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    return QIcon(pixmap);
}

inline QString surfaceColor()
{
    return QStringLiteral("#f8fafc");
}

inline QString cardColor()
{
    return QStringLiteral("#ffffff");
}

inline QString cardBorderColor()
{
    return QStringLiteral("#dbe3ec");
}

inline QString textColor()
{
    return QStringLiteral("#0f172a");
}

inline QString secondaryTextColor()
{
    return QStringLiteral("#475569");
}

inline QString hintTextColor()
{
    return QStringLiteral("#64748b");
}

inline QString subtleButtonColor()
{
    return QStringLiteral("#e2e8f0");
}

inline QString subtleButtonHoverColor()
{
    return QStringLiteral("#cbd5e1");
}

inline QString subtleButtonPressedColor()
{
    return QStringLiteral("#bac7d6");
}

inline QString primaryButtonStyle(const QString &color)
{
    const QColor base(color);
    const QString hoverColor = base.lighter(112).name();
    const QString pressedColor = base.darker(112).name();
    return QStringLiteral(
               "QPushButton {"
               " background: %1;"
               " color: white;"
               " border: none;"
               " border-radius: 6px;"
               " font: 700 11pt 'Microsoft YaHei UI';"
               " padding: 8px 14px;"
               " }"
               "QPushButton:hover { background: %2; }"
               "QPushButton:pressed { background: %3; }"
               "QPushButton:disabled { background: #94a3b8; color: #e2e8f0; }")
        .arg(color, hoverColor, pressedColor);
}

inline QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        " background: %1;"
        " color: %2;"
        " border: none;"
        " border-radius: 6px;"
        " font: 700 11pt 'Microsoft YaHei UI';"
        " padding: 8px 14px;"
        " }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:pressed { background: %4; }"
        "QPushButton:disabled { background: #e2e8f0; color: #94a3b8; }")
        .arg(subtleButtonColor(), textColor(), subtleButtonHoverColor(), subtleButtonPressedColor());
}

inline QString sectionCardStyle()
{
    return QStringLiteral(
               "QWidget {"
               " background: %1;"
               " border: 1px solid %2;"
               " border-radius: 8px;"
               " }"
               "QLabel { border: none; background: transparent; color: %3; }")
        .arg(cardColor(), cardBorderColor(), textColor());
}

inline QString titleStyle(int px)
{
    return QStringLiteral("font: 700 %1px 'Microsoft YaHei UI'; color: %2;")
        .arg(px)
        .arg(textColor());
}

inline QString bodyTextStyle()
{
    return QStringLiteral("font: 13pt 'Microsoft YaHei UI'; color: %1; line-height: 1.6;")
        .arg(secondaryTextColor());
}

inline QString hintTextStyle()
{
    return QStringLiteral("font: 11pt 'Microsoft YaHei UI'; color: %1; line-height: 1.5;")
        .arg(hintTextColor());
}

inline QString tagStyle()
{
    return QStringLiteral(
               "font: 700 10pt 'Microsoft YaHei UI';"
               " color: #0f766e;"
               " background: #ecfdf5;"
               " border: 1px solid #a7f3d0;"
               " border-radius: 5px;"
               " padding: 5px 10px;");
}

inline QString noticeCardStyle()
{
    return QStringLiteral(
               "QWidget {"
               " background: #eff6ff;"
               " border: 1px solid #bfdbfe;"
               " border-radius: 8px;"
               " }"
               "QLabel { border: none; background: transparent; color: #1e3a8a; }");
}

inline QString semanticBorderColor(const QString &semantic)
{
    if (semantic == QStringLiteral("success")) {
        return QStringLiteral("#86efac");
    }
    if (semantic == QStringLiteral("running")) {
        return QStringLiteral("#93c5fd");
    }
    if (semantic == QStringLiteral("warning")) {
        return QStringLiteral("#fcd34d");
    }
    if (semantic == QStringLiteral("offline")) {
        return QStringLiteral("#fca5a5");
    }
    if (semantic == QStringLiteral("disabled")) {
        return QStringLiteral("#cbd5e1");
    }
    return QStringLiteral("#dbe3ec");
}

inline QString semanticBackgroundColor(const QString &semantic)
{
    if (semantic == QStringLiteral("success")) {
        return QStringLiteral("#f0fdf4");
    }
    if (semantic == QStringLiteral("running")) {
        return QStringLiteral("#eff6ff");
    }
    if (semantic == QStringLiteral("warning")) {
        return QStringLiteral("#fffbeb");
    }
    if (semantic == QStringLiteral("offline")) {
        return QStringLiteral("#fef2f2");
    }
    if (semantic == QStringLiteral("disabled")) {
        return QStringLiteral("#f8fafc");
    }
    return QStringLiteral("#ffffff");
}

inline QString semanticTextColor(const QString &semantic)
{
    if (semantic == QStringLiteral("success")) {
        return QStringLiteral("#166534");
    }
    if (semantic == QStringLiteral("running")) {
        return QStringLiteral("#1d4ed8");
    }
    if (semantic == QStringLiteral("warning")) {
        return QStringLiteral("#a16207");
    }
    if (semantic == QStringLiteral("offline")) {
        return QStringLiteral("#b91c1c");
    }
    if (semantic == QStringLiteral("disabled")) {
        return QStringLiteral("#64748b");
    }
    return textColor();
}

inline QString listWidgetStyle()
{
    return QStringLiteral(
        "QListWidget {"
        " background: #f8fafc;"
        " color: #0f172a;"
        " border: 1px solid #dbe3ec;"
        " border-radius: 8px;"
        " padding: 8px;"
        " outline: none;"
        " font: 12pt 'Microsoft YaHei UI';"
        " }"
        "QListWidget::item {"
        " color: #0f172a;"
        " background: #ffffff;"
        " border: 1px solid #d7e1ee;"
        " border-radius: 6px;"
        " padding: 14px;"
        " margin: 4px 2px;"
        " }"
        "QListWidget::item:hover {"
        " color: #0f172a;"
        " background: #eff6ff;"
        " border: 1px solid #93c5fd;"
        " }"
        "QListWidget::item:selected {"
        " color: #0f172a;"
        " background: #dbeafe;"
        " border: 1px solid #60a5fa;"
        " }"
        "QListWidget::item:selected:active {"
        " color: #0f172a;"
        " background: #dbeafe;"
        " border: 1px solid #3b82f6;"
        " }"
        "QListWidget:focus { border: 1px solid #93c5fd; }");
}

inline QString spinBoxStyle()
{
    return QStringLiteral(
        "QSpinBox {"
        " min-height: 42px;"
        " border: 1px solid #cbd5e1;"
        " border-radius: 6px;"
        " padding: 0 12px;"
        " font: 12pt 'Microsoft YaHei UI';"
        " color: #0f172a;"
        " background: #f8fafc;"
        " selection-background-color: #bfdbfe;"
        " selection-color: #0f172a;"
        " }"
        "QSpinBox:focus { border: 1px solid #60a5fa; background: white; }"
        "QSpinBox::up-button, QSpinBox::down-button { width: 0px; border: none; }"
        "QSpinBox::up-arrow, QSpinBox::down-arrow { width: 0px; height: 0px; }");
}

inline QString verticalScrollBarStyle()
{
    return QStringLiteral(
        "QScrollBar:vertical {"
        " background: #e2e8f0;"
        " width: 12px;"
        " margin: 8px 4px 8px 0;"
        " border-radius: 6px;"
        " }"
        "QScrollBar::handle:vertical {"
        " background: #94a3b8;"
        " min-height: 40px;"
        " border-radius: 6px;"
        " }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");
}

inline QString applicationStyleSheet()
{
    return QStringLiteral(
        "QWidget {"
        " color: #0f172a;"
        " background: #f8fafc;"
        " font-family: 'Microsoft YaHei UI';"
        " }"
        "QLabel { color: #0f172a; }"
        "QMainWindow, QDialog { background: #f8fafc; }"
        "QStatusBar {"
        " background: #eef2f7;"
        " color: #475569;"
        " border-top: 1px solid #dbe3ec;"
        " padding: 2px 8px;"
        " }"
        "QLineEdit, QComboBox {"
        " min-height: 38px;"
        " padding: 0 10px;"
        " border: 1px solid #cbd5e1;"
        " border-radius: 6px;"
        " background: #ffffff;"
        " selection-background-color: #bfdbfe;"
        " }"
        "QLineEdit:focus, QComboBox:focus { border-color: #2563eb; }"
        "QTableWidget {"
        " background: #ffffff;"
        " alternate-background-color: #f8fafc;"
        " border: 1px solid #dbe3ec;"
        " gridline-color: #e2e8f0;"
        " selection-background-color: #dbeafe;"
        " selection-color: #0f172a;"
        " }"
        "QHeaderView::section {"
        " background: #eef2f7;"
        " color: #334155;"
        " border: none;"
        " border-bottom: 1px solid #dbe3ec;"
        " padding: 8px 10px;"
        " font-weight: 700;"
        " }"
        "QToolTip {"
        " color: #0f172a;"
        " background: #ffffff;"
        " border: 1px solid #cbd5e1;"
        " padding: 6px 8px;"
        " }"
        "QPlainTextEdit, QTextEdit {"
        " selection-background-color: #bfdbfe;"
        " selection-color: #0f172a;"
        " }"
        "QLineEdit, QAbstractSpinBox, QListWidget, QComboBox {"
        " color: #0f172a;"
        " }"
        "QMessageBox, QFileDialog, QDialog {"
        " background: #f8fafc;"
        " }");
}

} // namespace AppTheme
