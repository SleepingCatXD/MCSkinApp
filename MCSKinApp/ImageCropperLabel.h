#pragma once
#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
class ImageCropperLabel :
    public QLabel
{
    Q_OBJECT

public:
    explicit ImageCropperLabel(QWidget* parent = nullptr);

    void setCroppingMode(bool enabled);
    bool isCropping() const { return m_isCropping; }

    // 供外部调用，返回裁剪出的小图
    QImage getCroppedImage(const QImage& originalImage);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    bool m_isCropping;
    bool m_isDragging;
    QRect m_cropRect; // 裁剪框在 Widget 坐标系中的位置
    QPoint m_dragStartPos;

    void ensureRectInside(); // 防止框跑出图片边界
};

