#include "ImageCropperLabel.h"

ImageCropperLabel::ImageCropperLabel(QWidget* parent)
    : QLabel(parent), m_isCropping(false), m_isDragging(false)
{
    m_cropRect = QRect(0, 0, 150, 150); // 初始默认大小
}

void ImageCropperLabel::setCroppingMode(bool enabled)
{
    m_isCropping = enabled;
    if (enabled) {
        // 开启裁剪时，将框居中
        m_cropRect.moveCenter(this->rect().center());
    }
    update(); // 触发重绘
}

void ImageCropperLabel::paintEvent(QPaintEvent* event)
{
    QLabel::paintEvent(event); // 先画原有的图片

    if (m_isCropping && !this->pixmap().isNull()) {
        QPainter painter(this);

        // 使用 QRegion 精准计算需要画遮罩的区域 (整个矩形 减去 中间的小矩形)
        QRegion bgRegion(this->rect());
        QRegion cropRegion(m_cropRect);
        QRegion maskRegion = bgRegion.subtracted(cropRegion);

        // 限制画笔只在这个“带洞的区域”里画画
        painter.setClipRegion(maskRegion);
        // 画半透明黑色 (100 是不透明度，可以按你的喜好在 0-255 之间微调)
        painter.fillRect(this->rect(), QColor(0, 0, 0, 100));

        // 恢复画笔限制，准备画白线
        painter.setClipping(false);

        // 画一圈白色的裁剪虚线边框
        QPen pen(Qt::white, 2, Qt::DashLine);
        painter.setPen(pen);
        painter.drawRect(m_cropRect);
    }
}

void ImageCropperLabel::mousePressEvent(QMouseEvent* event)
{
    if (m_isCropping && event->button() == Qt::LeftButton) {
        if (m_cropRect.contains(event->pos())) {
            m_isDragging = true;
            m_dragStartPos = event->pos() - m_cropRect.topLeft();
        }
    }
    QLabel::mousePressEvent(event);
}

void ImageCropperLabel::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging) {
        m_cropRect.moveTo(event->pos() - m_dragStartPos);
        ensureRectInside();
        update(); // 拖拽时不断重绘
    }
    QLabel::mouseMoveEvent(event);
}

void ImageCropperLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QLabel::mouseReleaseEvent(event);
}

void ImageCropperLabel::wheelEvent(QWheelEvent* event)
{
    if (m_isCropping) {
        int delta = event->angleDelta().y();
        int step = 10; // 每次滚轮缩放的像素量

        // 保持中心点不变进行缩放
        QPoint center = m_cropRect.center();
        if (delta > 0) {
            m_cropRect.adjust(-step, -step, step, step); // 放大
        }
        else {
            m_cropRect.adjust(step, step, -step, -step); // 缩小
        }

        // 限制最小尺寸
        if (m_cropRect.width() < 50) {
            m_cropRect.setWidth(50);
            m_cropRect.setHeight(50);
        }

        m_cropRect.moveCenter(center);
        ensureRectInside();
        update();
    }
    QLabel::wheelEvent(event);
}

void ImageCropperLabel::ensureRectInside()
{
    // 简易边界限制，防止框拖出 Label
    if (m_cropRect.left() < 0) m_cropRect.moveLeft(0);
    if (m_cropRect.top() < 0) m_cropRect.moveTop(0);
    if (m_cropRect.right() > this->width()) m_cropRect.moveRight(this->width());
    if (m_cropRect.bottom() > this->height()) m_cropRect.moveBottom(this->height());
}

QImage ImageCropperLabel::getCroppedImage(const QImage& originalImage)
{
    // 核心算法：将 Widget 上的裁剪框坐标映射回原始高清大图的坐标系
    if (this->pixmap().isNull() || originalImage.isNull()) return QImage();

    QSize labelSize = this->size();
    QPixmap displayedPixmap = this->pixmap();
    QSize scaledSize = displayedPixmap.size();

    // 计算图片在 Label 中的居中偏移量
    int offsetX = (labelSize.width() - scaledSize.width()) / 2;
    int offsetY = (labelSize.height() - scaledSize.height()) / 2;

    // 计算相对于渲染图片的坐标
    QRect relativeRect = m_cropRect.translated(-offsetX, -offsetY);

    // 映射回原始大图
    double ratioX = (double)originalImage.width() / scaledSize.width();
    double ratioY = (double)originalImage.height() / scaledSize.height();

    QRect originalRect(
        relativeRect.x() * ratioX,
        relativeRect.y() * ratioY,
        relativeRect.width() * ratioX,
        relativeRect.height() * ratioY
    );

    return originalImage.copy(originalRect);
}
