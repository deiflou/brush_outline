#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>

class window : public QWidget
{
    Q_OBJECT

public:
    window();
    ~window();

    void paintEvent(QPaintEvent*) override;

private:
    QImage m_canvas;
};

#endif