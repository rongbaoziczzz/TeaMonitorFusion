#pragma once

#include <QString>

namespace AppTheme {

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
    return QStringLiteral(
               "QPushButton {"
               " background: %1;"
               " color: white;"
               " border: none;"
               " border-radius: 15px;"
               " font: 700 12pt 'Microsoft YaHei UI';"
               " padding: 10px 18px;"
               " }"
               "QPushButton:hover { background: %2; }"
               "QPushButton:pressed { background: %3; }"
               "QPushButton:disabled { background: #94a3b8; color: #e2e8f0; }")
        .arg(color, color, color);
}

inline QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        " background: %1;"
        " color: %2;"
        " border: none;"
        " border-radius: 15px;"
        " font: 700 12pt 'Microsoft YaHei UI';"
        " padding: 10px 18px;"
        " }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:pressed { background: %4; }")
        .arg(subtleButtonColor(), textColor(), subtleButtonHoverColor(), subtleButtonPressedColor());
}

inline QString sectionCardStyle()
{
    return QStringLiteral(
               "QWidget {"
               " background: %1;"
               " border: 1px solid %2;"
               " border-radius: 20px;"
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
               " border-radius: 999px;"
               " padding: 6px 12px;");
}

inline QString noticeCardStyle()
{
    return QStringLiteral(
               "QWidget {"
               " background: #eff6ff;"
               " border: 1px solid #bfdbfe;"
               " border-radius: 22px;"
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
        " border-radius: 20px;"
        " padding: 10px;"
        " outline: none;"
        " font: 12pt 'Microsoft YaHei UI';"
        " }"
        "QListWidget::item {"
        " color: #0f172a;"
        " background: #ffffff;"
        " border: 1px solid #d7e1ee;"
        " border-radius: 14px;"
        " padding: 18px;"
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
        " min-height: 56px;"
        " border: 1px solid #cbd5e1;"
        " border-radius: 14px;"
        " padding: 0 16px;"
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
