/*  Video Display
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include "Common/Cpp/PrettyPrint.h"
#include "VideoDisplayWidget.h"
#include "VideoDisplayWindow.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{



VideoDisplayWidget::VideoDisplayWidget(
    QWidget& parent, QLayout& holder,
    size_t id,
    CommandReceiver& command_receiver,
    CameraSession& camera,
    VideoOverlaySession& overlay
)
    : WidgetStackFixedAspectRatio(parent, WidgetStackFixedAspectRatio::ADJUST_HEIGHT_TO_WIDTH)
    , m_holder(holder)
    , m_id(id)
    , m_command_receiver(command_receiver)
    , m_overlay_session(overlay)
    , m_video(camera.make_QtWidget(this))
    , m_overlay(new VideoOverlayWidget(*this, overlay))
    , m_underlay(new QWidget(this))
    , m_source_fps(*this)
    , m_display_fps(*this)
{
    this->add_widget(*m_video);
    this->add_widget(*m_overlay);

    Resolution resolution = m_video->camera().current_resolution();
    if (resolution){
        set_aspect_ratio(resolution.aspect_ratio());
    }

    m_overlay->setVisible(true);
    m_overlay->setHidden(false);
    m_overlay->raise();

#if 1
    {
        m_underlay->setHidden(true);
        holder.addWidget(m_underlay);

        QVBoxLayout* layout = new QVBoxLayout(m_underlay);
        layout->setAlignment(Qt::AlignTop);

        QHBoxLayout* row_width = new QHBoxLayout();
        layout->addLayout(row_width);
        QHBoxLayout* row_height = new QHBoxLayout();
        layout->addLayout(row_height);

        row_width->addStretch(2);
        row_height->addStretch(2);
        row_width->addWidget(new QLabel("<b>Window Width:</b>", m_underlay), 1);
        row_height->addWidget(new QLabel("<b>Window Height:</b>", m_underlay), 1);

        m_width_box = new QLineEdit(m_underlay);
        row_width->addWidget(m_width_box, 1);
        m_height_box = new QLineEdit(m_underlay);
        row_height->addWidget(m_height_box, 1);

        row_width->addStretch(2);
        row_height->addStretch(2);

        connect(
            m_width_box, &QLineEdit::editingFinished,
            this, [this]{
                bool ok;
                int value = m_width_box->text().toInt(&ok);
                if (ok && 100 <= value){
                    m_last_width = value;
                    if (m_window){
                        m_window->resize(m_last_width, m_last_height);
                    }
                }
                m_width_box->setText(QString::number(m_last_width));
            }
        );
        connect(
            m_height_box, &QLineEdit::editingFinished,
            this, [this]{
                bool ok;
                int value = m_height_box->text().toInt(&ok);
                if (ok && 100 <= value){
                    m_last_height = value;
                    if (m_window){
                        m_window->resize(m_last_width, m_last_height);
                    }
                }
                m_height_box->setText(QString::number(m_last_height));
            }
        );
    }
#endif

    overlay.add_stat(m_source_fps);
    overlay.add_stat(m_display_fps);
}
VideoDisplayWidget::~VideoDisplayWidget(){
    //  Close the window popout first since it holds references to this class.
    move_back_from_window();
    m_overlay_session.remove_stat(m_display_fps);
    m_overlay_session.remove_stat(m_source_fps);
    delete m_underlay;
}


void VideoDisplayWidget::move_to_new_window(){
    if (m_window){
        return;
    }
    // The constructor of VideoDisplayWindow handles the transfer of this VideoDisplayWidget to the new window.
    // The constructor also displays the window.
    // So there is nothing else to do in VideoDisplayWidget::move_to_new_window() besides building VideoDisplayWindow.
    this->set_size_policy(EXPAND_TO_BOX);
    m_window.reset(new VideoDisplayWindow(this));
    m_underlay->setHidden(false);
}
void VideoDisplayWidget::move_back_from_window(){
    if (!m_window){
        return;
    }
    m_underlay->setHidden(true);
    this->set_size_policy(ADJUST_HEIGHT_TO_WIDTH);
    m_holder.addWidget(this);
//    this->resize(this->size());
//    cout << "VideoWidget Before: " << m_video->width() << " x " << m_video->height() << endl;
    m_video->resize(this->size());
//    cout << "VideoWidget After: " << m_video->width() << " x " << m_video->height() << endl;
    m_holder.update();
    m_window.reset();
}



void VideoDisplayWidget::mouseDoubleClickEvent(QMouseEvent* event){
//    if (!PreloadSettings::instance().DEVELOPER_MODE){
//        return;
//    }
    // If this widget is not already inside a VideoDisplayWindow, move it
    // into a VideoDisplayWindow
    if (!m_window){
        move_to_new_window();
    }else{
        QWidget::mouseDoubleClickEvent(event);
    }
}
void VideoDisplayWidget::paintEvent(QPaintEvent* event){
    WidgetStackFixedAspectRatio::paintEvent(event);
//    cout << "VideoDisplayWidget: " << this->width() << " x " << this->height() << endl;
//    cout << "VideoWidget: " << m_video->width() << " x " << m_video->height() << endl;
}
void VideoDisplayWidget::resizeEvent(QResizeEvent* event){
    WidgetStackFixedAspectRatio::resizeEvent(event);
    m_last_width = this->width();
    m_last_height = this->height();
    m_width_box->setText(QString::number(m_last_width));
    m_height_box->setText(QString::number(m_last_height));
}


OverlayStatSnapshot VideoSourceFPS::get_current(){
    double fps = m_parent.m_video->camera().fps_source();
    return OverlayStatSnapshot{
        "Video Source FPS: " + tostr_fixed(fps, 2),
        fps < 20 ? COLOR_RED : COLOR_WHITE
    };
}
OverlayStatSnapshot VideoDisplayFPS::get_current(){
    double fps = m_parent.m_video->camera().fps_display();
    return OverlayStatSnapshot{
        "Video Display FPS: " + (fps < 0 ? "???" : tostr_fixed(fps, 2)),
        fps >= 0 && fps < 20 ? COLOR_RED : COLOR_WHITE
    };
}



}
