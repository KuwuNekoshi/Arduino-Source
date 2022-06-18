#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>

#include "Common/Compiler.h"
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Options/EditableTableOption.h"
#include "CommonFramework/Options/EditableTableWidget.h"
#include "Common/Qt/QtJsonTools.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/Options/EditableTableOption-EnumTableCell.h"
#include "Pokemon/Options/Pokemon_IVCheckerWidget.h"
#include "NintendoSwitch/Options/TestPathMakerTable.h"

#include <iostream>
#include <fstream>
using std::cout;
using std::endl;

namespace PokemonAutomation{
namespace NintendoSwitch{

const QString PathAction_NAMES[] = {
    "No Action",
    "Left Joystick",
    "Right Joystick",
    "Press B",
    "Press A",
    "Press Y",
    "Press X",
    "Press R",
    "Press L",
    "Press ZR",
    "Press ZL",
    "Press PLUS",
    "Press MINUS",
    "Press DPad Left",
    "Press DPad Right",
    "Press DPad Up",
    "Press DPad Down",
    "Wait"
};

const std::map<QString, PathAction> PathAction_MAP{
    {PathAction_NAMES[0], PathAction::NO_ACTION},
    {PathAction_NAMES[1], PathAction::LEFT_JOYSTICK},
    {PathAction_NAMES[2], PathAction::RIGHT_JOYSTICK},
    {PathAction_NAMES[3], PathAction::B},
    {PathAction_NAMES[4], PathAction::A},
    {PathAction_NAMES[5], PathAction::Y},
    {PathAction_NAMES[6], PathAction::X},
    {PathAction_NAMES[7], PathAction::R},
    {PathAction_NAMES[8], PathAction::L},
    {PathAction_NAMES[9], PathAction::ZR},
    {PathAction_NAMES[10], PathAction::ZL},
    {PathAction_NAMES[11], PathAction::PLUS},
    {PathAction_NAMES[12], PathAction::MINUS},
    {PathAction_NAMES[13], PathAction::DPADLEFT},
    {PathAction_NAMES[14], PathAction::DPADRIGHT},
    {PathAction_NAMES[15], PathAction::DPADUP},
    {PathAction_NAMES[16], PathAction::DPADDOWN},
    {PathAction_NAMES[17], PathAction::WAIT}
};

TestPathMakerTableRow::TestPathMakerTableRow() {}

void TestPathMakerTableRow::load_json(const QJsonValue& json){
    QJsonObject obj = json.toObject();
    {
        QString value;
        if (json_get_string(value, obj, "Action")){
            const auto iter = PathAction_MAP.find(value);
            if (iter != PathAction_MAP.end()){
                action = iter->second;
            }
        }
        switch(action){
        case PathAction::LEFT_JOYSTICK:
        case PathAction::RIGHT_JOYSTICK:
            json_get_int(x_axis, obj, "MoveDirectionX");
            json_get_int(y_axis, obj, "MoveDirectionY");
            json_get_double(button_hold_ticks, obj, "Hold");
            json_get_double(button_release_ticks, obj, "Release");
            break;
        case PathAction::B:
        case PathAction::A:
        case PathAction::Y:
        case PathAction::X:
        case PathAction::R:
        case PathAction::L:
        case PathAction::ZR:
        case PathAction::ZL:
        case PathAction::PLUS:
        case PathAction::MINUS:
        case PathAction::DPADLEFT:
        case PathAction::DPADRIGHT:
        case PathAction::DPADUP:
        case PathAction::DPADDOWN:
            json_get_double(button_hold_ticks, obj, "Hold");
            json_get_double(button_release_ticks, obj, "Release");
            break;
        case PathAction::WAIT:
            json_get_double(wait_ticks,obj,"Wait");
        default:
            break;
        }
    }

}

QJsonValue TestPathMakerTableRow::to_json() const{
    QJsonObject obj;
    obj.insert("Action", PathAction_NAMES[(size_t)action]);
    switch(action){
    case PathAction::B:
    case PathAction::A:
    case PathAction::Y:
    case PathAction::X:
    case PathAction::R:
    case PathAction::L:
    case PathAction::ZR:
    case PathAction::ZL:
    case PathAction::PLUS:
    case PathAction::MINUS:
    case PathAction::DPADLEFT:
    case PathAction::DPADRIGHT:
    case PathAction::DPADUP:
    case PathAction::DPADDOWN:
        obj.insert("Hold", button_hold_ticks);
        obj.insert("Release", button_release_ticks);
        break;
    case PathAction::LEFT_JOYSTICK:
    case PathAction::RIGHT_JOYSTICK:
        obj.insert("MoveDirectionX", x_axis);
        obj.insert("MoveDirectionY", y_axis);
        obj.insert("Hold", button_hold_ticks);
        obj.insert("Release", button_release_ticks);
        break;
    case PathAction::WAIT:
        obj.insert("Wait", wait_ticks);
        break;
    default:
        break;
    }

    return obj;
}

std::unique_ptr<EditableTableRow> TestPathMakerTableRow::clone() const{
    return std::unique_ptr<EditableTableRow>(new TestPathMakerTableRow(*this));
}

class ActionParameterWidget : public QWidget{
public:
    ActionParameterWidget(QWidget& parent, TestPathMakerTableRow& row);

    void setParameter();

private:
    TestPathMakerTableRow& m_row;

    QHBoxLayout* m_layout = nullptr;

    QWidget* m_hold_label;
    QWidget* m_hold_duration;
    QWidget* m_release_label;
    QWidget* m_release_duration;

    QWidget* m_axis_x_label;
    QWidget* m_axis_x_value;
    QWidget* m_axis_y_label;
    QWidget* m_axis_y_value;

    QWidget* m_wait_start_label;
    QWidget* m_wait_end_label;
    QWidget* m_wait_value;

};

ActionParameterWidget::ActionParameterWidget(QWidget& parent, TestPathMakerTableRow& row)
    : QWidget(&parent), m_row(row)
{
    this->setContentsMargins(0, 0, 0, 0);
    m_layout = new QHBoxLayout(this);

    m_layout->setContentsMargins(5, 0, 5, 0);

    m_hold_label = new QLabel("Ticks to hold:", this);
    m_hold_duration = make_integer_table_cell(*this, m_row.button_hold_ticks);
    m_release_label = new QLabel("Ticks to release:", this);
    m_release_duration = make_integer_table_cell(*this, m_row.button_release_ticks);

    m_axis_x_label = new QLabel("X:", this);
    m_axis_x_value = make_limited_integer_table_cell(*this, m_row.x_axis, 0, 255);
    m_axis_x_value->setMaximumWidth(80);

    m_axis_y_label = new QLabel("Y:", this);
    m_axis_y_value = make_limited_integer_table_cell(*this, m_row.y_axis, 0, 255);
    m_axis_y_value->setMaximumWidth(80);

    m_wait_start_label = new QLabel("Wait for:");
    m_wait_value = make_double_table_cell(*this, m_row.wait_ticks, -1.0, 100.0);
    m_wait_end_label = new QLabel("seconds.");

    m_layout->addWidget(m_axis_x_label);
    m_layout->addWidget(m_axis_x_value);
    m_layout->addWidget(m_axis_y_label);
    m_layout->addWidget(m_axis_y_value);

    m_layout->addWidget(m_hold_label);
    m_layout->addWidget(m_hold_duration);
    m_layout->addWidget(m_release_label);
    m_layout->addWidget(m_release_duration);

    m_layout->addWidget(m_wait_start_label);
    m_layout->addWidget(m_wait_value);
    m_layout->addWidget(m_wait_end_label);

    setParameter();
}

void ActionParameterWidget::setParameter(){
    for(int i = 0; i < m_layout->count(); i++){
        m_layout->itemAt(i)->widget()->hide();
    }
    switch(m_row.action){
    case PathAction::B:
    case PathAction::A:
    case PathAction::Y:
    case PathAction::X:
    case PathAction::R:
    case PathAction::L:
    case PathAction::ZR:
    case PathAction::ZL:
    case PathAction::PLUS:
    case PathAction::MINUS:
    case PathAction::DPADLEFT:
    case PathAction::DPADRIGHT:
    case PathAction::DPADUP:
    case PathAction::DPADDOWN:
            m_hold_label->show();
            m_hold_duration->show();
            m_release_label->show();
            m_release_duration->show();
            break;
    case PathAction::LEFT_JOYSTICK:
    case PathAction::RIGHT_JOYSTICK:
            m_axis_x_label->show();
            m_axis_x_value->show();
            m_axis_y_label->show();
            m_axis_y_value->show();
            m_hold_label->show();
            m_hold_duration->show();
            m_release_label->show();
            m_release_duration->show();
            break;
    case PathAction::WAIT:
            m_wait_start_label->show();
            m_wait_value->show();
            m_wait_end_label->show();
        break;

    default:
        break;
    }
}

std::vector<QWidget*> TestPathMakerTableRow::make_widgets(QWidget& parent){
    std::vector<QWidget*> widgets;
    auto parameterWidget = new ActionParameterWidget(parent, *this);
    widgets.emplace_back(make_action_box(parent, action, parameterWidget));
    widgets.emplace_back(parameterWidget);
    return widgets;
}

QWidget* TestPathMakerTableRow::make_action_box(QWidget& parent, PathAction& action, ActionParameterWidget* parameterWidget){
    QComboBox* box = new NoWheelComboBox(&parent);
    for(size_t i = 0; i < PathAction_MAP.size(); i++){
        box->addItem(PathAction_NAMES[i]);
    }
    box->setCurrentIndex((int)action);
    box->connect(
        box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        box, [&action, parameterWidget](int index){
            if (index < 0){
                index = 0;
            }
            action = (PathAction)index;
            parameterWidget->setParameter();
        }
    );
    return box;
}

QStringList TestPathMakerTableFactory::make_header() const{
    QStringList list;
    list << "Action" << "Parameters";
    return list;
}

std::unique_ptr<EditableTableRow> TestPathMakerTableFactory::make_row() const{
    return std::unique_ptr<EditableTableRow>(new TestPathMakerTableRow());
}



std::vector<std::unique_ptr<EditableTableRow>> TestPathMakerTable::make_defaults() const{
    std::vector<std::unique_ptr<EditableTableRow>> ret;
    auto row = std::make_unique<TestPathMakerTableRow>();
    row = std::make_unique<TestPathMakerTableRow>();
    row->action = PathAction::LEFT_JOYSTICK;
    row->button_hold_ticks = 400;
    row->button_release_ticks = 500;
    row->x_axis = 127;
    row->y_axis = 127;
    ret.emplace_back(std::move(row));

    return ret;
}

TestPathMakerTable::TestPathMakerTable()
    : PATH(
        "<b>Custom Path Table:</b><br>"
        "Set a sequence of actions to navigate the map.",
        m_factory, make_defaults()
    )
{
    PA_ADD_OPTION(PATH);
}

class TestPathMakerTableWidget : public QWidget, public ConfigWidget{
public:
    TestPathMakerTableWidget(QWidget& parent, TestPathMakerTable& value)
        : QWidget(&parent)
        , ConfigWidget(value, *this)
    {
        m_table_widget = static_cast<EditableTableWidget*>(value.PATH.make_ui(*this));

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(&m_table_widget->widget());

        QHBoxLayout* button_layout = new QHBoxLayout();

        button_layout->setContentsMargins(0, 0, 0, 0);
        layout->addLayout(button_layout);

        auto load_button = new QPushButton("Load Options", this);
        button_layout->addWidget(load_button);

        auto save_button = new QPushButton("Save Options", this);
        button_layout->addWidget(save_button);

        auto dump_button = new QPushButton("Dump Actions",this);
        button_layout->addWidget(dump_button);

        connect(load_button,  &QPushButton::clicked, this, [&value, this](bool){
            QString path = QFileDialog::getOpenFileName(this, tr("Open option file"), ".", "*.json");
            std::cout << "Load TestPath from " << path.toStdString() << std::endl;
            if (path.size() > 0){
                QJsonDocument doc = read_json_file(path);
                if (!doc.isObject()){
                    QMessageBox box;
                    box.critical(nullptr, "Error", "Invalid option file: " + path + ", no Json object.");
                    return;
                }
                QJsonObject root = doc.object();
                auto it = root.find("TEST_PATH_MAKER_TABLE");
                if (it == root.end()){
                    QMessageBox box;
                    box.critical(nullptr, "Error", "Invalid option file: " + path + ", no TEST_PATH_MAKER_TABLE.");
                    return;
                }
                QJsonValue obj = json_get_value_nothrow(root, "TEST_PATH_MAKER_TABLE");
                value.load_json(obj);
                if (m_table_widget == nullptr){
                    QMessageBox box;
                    box.critical(nullptr, "Error", "Internal code error, cannot convert to EditableTableBaseWidget.");
                    return;
                }
                m_table_widget->update_ui();
            }
        });

        connect(save_button,  &QPushButton::clicked, this, [&value, this](bool){
            auto path = QFileDialog::getSaveFileName(this, tr("Open option file"), ".", "*.json");
            std::cout << "Save TestPath from " << path.toStdString() << std::endl;
            if (path.size() > 0){
                try{
                    QJsonObject root;
                    root.insert("TEST_PATH_MAKER_TABLE", value.to_json());
                    write_json_file(path, QJsonDocument(root));
                }catch (FileException&){
                    QMessageBox box;
                    box.critical(nullptr, "Error", "Failed to save to file: " + path);
                    return;
                }
            }
        });

        connect(dump_button,  &QPushButton::clicked, this,[&value](bool){
            std::ofstream myfile;
            myfile.open("dump.txt");
            for (size_t action_index = 0; action_index < value.num_actions(); action_index++){
                const auto& row = value.get_action(action_index);
                switch(row.action){
                case PathAction::LEFT_JOYSTICK:
                    myfile << "pbf_move_left_joystick(context, " + std::to_string(row.x_axis) + ", " + std::to_string(row.y_axis) + ", " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::RIGHT_JOYSTICK:
                    myfile << "pbf_move_right_joystick(context, " + std::to_string(row.x_axis) + ", " + std::to_string(row.y_axis) + ", " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::B:
                    myfile << "pbf_move_right_joystick(context, BUTTON_B, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::A:
                    myfile << "pbf_move_right_joystick(context, BUTTON_A, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::Y:
                    myfile << "pbf_move_right_joystick(context, BUTTON_Y, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::X:
                    myfile << "pbf_move_right_joystick(context, BUTTON_X, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::R:
                    myfile << "pbf_move_right_joystick(context, BUTTON_R, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::L:
                    myfile << "pbf_move_right_joystick(context, BUTTON_L, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::ZR:
                    myfile << "pbf_move_right_joystick(context, BUTTON_ZR, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::ZL:
                    myfile << "pbf_move_right_joystick(context, BUTTON_ZL, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::PLUS:
                    myfile << "pbf_move_right_joystick(context, BUTTON_PLUS, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::MINUS:
                    myfile << "pbf_move_right_joystick(context, BUTTON_MINUS, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::DPADLEFT:
                    myfile << "pbf_press_dpad(context, DPAD_LEFT, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::DPADRIGHT:
                    myfile << "pbf_press_dpad(context, DPAD_RIGHT, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::DPADUP:
                    myfile << "pbf_press_dpad(context, DPAD_UP, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::DPADDOWN:
                    myfile << "pbf_press_dpad(context, DPAD_DOWN, " + std::to_string(row.button_hold_ticks) + ", " + std::to_string(row.button_release_ticks) + ");\n";
                    break;
                case PathAction::WAIT:
                    myfile << "pbf_wait(context, (" + std::to_string(row.wait_ticks) + "* TICKS_PER_SECOND));\n";
                default:
                    break;
                }
            }
            myfile.close();
        });

    }

    virtual void restore_defaults() override{
        m_table_widget->restore_defaults();
    }
    virtual void update_ui() override{
        m_table_widget->update_ui();
    }

private:
    EditableTableWidget* m_table_widget = nullptr;
};

ConfigWidget* TestPathMakerTable::make_ui(QWidget& parent){
    return new TestPathMakerTableWidget(parent, *this);
}


}
}
