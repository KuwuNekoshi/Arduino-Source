/*  String Select
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 *      This option is thread-safe.
 *
 */

#ifndef PokemonAutomation_StringSelect_H
#define PokemonAutomation_StringSelect_H

#include <vector>
#include <map>
#include <QIcon>
#include "ConfigOption.h"

namespace PokemonAutomation{


class StringSelectOption : public ConfigOption{
public:
    StringSelectOption(
        std::string label,
        const std::vector<std::string>& cases,
        const std::string& default_case
    );
    StringSelectOption(
        std::string label,
        std::vector<std::pair<std::string, QIcon>> cases,
        const std::string& default_case
    );

    explicit operator size_t() const{ return m_current.load(std::memory_order_relaxed); }
    explicit operator const std::string&() const{ return m_case_list[(size_t)*this].first; }

    virtual void load_json(const JsonValue& json) override;
    virtual JsonValue to_json() const override;

    virtual void restore_defaults() override;

    virtual ConfigWidget* make_ui(QWidget& parent) override;

private:
    friend class StringSelectWidget;
    const std::string m_label;
    std::vector<std::pair<std::string, QIcon>> m_case_list;
    std::map<std::string, size_t> m_case_map;
    size_t m_default;

    std::atomic<size_t> m_current;
};





}
#endif
