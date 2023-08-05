/*  Tera Roller
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <set>
#include <sstream>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/Tools/StatsTracking.h"
#include "CommonFramework/Tools/VideoResolutionCheck.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
//#include "Pokemon/Pokemon_Notification.h"
#include "Pokemon/Inference/Pokemon_NameReader.h"
#include "PokemonSV/Inference/Battles/PokemonSV_TeraBattleMenus.h"
#include "PokemonSV/Inference/Pokedex/PokemonSV_PokedexShinyDetector.h"
#include "PokemonSV/PokemonSV_Settings.h"
#include "PokemonSV/Inference/Tera/PokemonSV_TeraCardDetector.h"
#include "PokemonSV/Inference/Tera/PokemonSV_TeraSilhouetteReader.h"
#include "PokemonSV/Inference/Tera/PokemonSV_TeraTypeReader.h"
//#include "PokemonSV/Inference/PokemonSV_MainMenuDetector.h"
#include "PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "PokemonSV/Programs/PokemonSV_Navigation.h"
#include "PokemonSV/Programs/TeraRaids/PokemonSV_TeraRoutines.h"
#include "PokemonSV_TeraRoller.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

using namespace Pokemon;


TeraRoller_Descriptor::TeraRoller_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:TeraRoller",
        STRING_POKEMON + " SV", "Tera Roller",
        "ComputerControl/blob/master/Wiki/Programs/PokemonSV/TeraRoller.md",
        "Roll Tera raids to find shiny " + STRING_POKEMON + ".",
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}
struct TeraRoller_Descriptor::Stats : public StatsTracker{
    Stats()
        : m_skips(m_stats["Date Skips"])
        , m_raids(m_stats["Raids"])
        , m_skipped(m_stats["Skipped"])
        , m_errors(m_stats["Errors"])
        , m_shinies(m_stats["Shinies"])
    {
        m_display_order.emplace_back("Date Skips");
        m_display_order.emplace_back("Raids");
        m_display_order.emplace_back("Skipped");
        m_display_order.emplace_back("Errors", true);
        m_display_order.emplace_back("Shinies", true);
    }
    std::atomic<uint64_t>& m_skips;
    std::atomic<uint64_t>& m_raids;
    std::atomic<uint64_t>& m_skipped;
    std::atomic<uint64_t>& m_errors;
    std::atomic<uint64_t>& m_shinies;
};
std::unique_ptr<StatsTracker> TeraRoller_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}



TeraRollerOpponentFilter::TeraRollerOpponentFilter()
    : GroupOption("Opponent Filter", LockWhileRunning::UNLOCKED)
    , SKIP_HERBA(
        "<b>Skip Non-Herba Raids:</b><br>"
        "Skip raids that don't have the possibility to reward all types of Herba Mystica. This won't stop the program when Herba Mystica is found, it will only increase your chances to find it.",
        LockWhileRunning::UNLOCKED,
        false
    )
    , MIN_STARS(
        "<b>Min Stars:</b><br>Skip raids with less than this many stars.",
        LockWhileRunning::UNLOCKED,
        1, 1, 7
    )
    , MAX_STARS(
        "<b>Max Stars:</b><br>Skip raids with more than this many stars to save time since you're likely to lose.",
        LockWhileRunning::UNLOCKED,
        4, 1, 7
    )
{
    PA_ADD_OPTION(SKIP_HERBA);
    PA_ADD_OPTION(MIN_STARS);
    PA_ADD_OPTION(MAX_STARS);
}

bool TeraRollerOpponentFilter::should_battle(size_t stars, const std::string& pokemon) const{
    if (stars < MIN_STARS || stars > MAX_STARS){
        return false;
    }

    static const std::set<std::string> fivestar{
        "gengar", "glalie", "amoonguss", "dondozo", "palafin", "finizen", "blissey", "eelektross", "driftblim", "cetitan"
    };
    static const std::set<std::string> sixstar{
        "blissey", "vaporeon", "amoonguss", "farigiraf", "cetitan", "dondozo"
    };

    if (SKIP_HERBA){
        if (fivestar.find(pokemon) != fivestar.end()){
            return true;
        }
        if (sixstar.find(pokemon) != sixstar.end()){
            return true;
        }
        return false;
    }

    return true;
}


TeraRoller::TeraRoller()
    : LANGUAGE(
        "<b>Game Language:</b>",
        PokemonNameReader::instance().languages(),
        LockWhileRunning::UNLOCKED
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATION_NONSHINY(
        "Non-Shiny Encounter",
        true, false,
        {"Notifs"},
        std::chrono::seconds(3600)
    )
    , NOTIFICATION_SHINY(
        "Shiny Encounter",
        true, true, ImageAttachmentMode::JPG,
        {"Notifs", "Showcase"}
    )
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_NONSHINY,
        &NOTIFICATION_SHINY,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(FILTER);
    PA_ADD_OPTION(NOTIFICATIONS);
}


void TeraRoller::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context){
    assert_16_9_720p_min(env.logger(), env.console);

    TeraRoller_Descriptor::Stats& stats = env.current_stats<TeraRoller_Descriptor::Stats>();

    if (FILTER.MIN_STARS > FILTER.MAX_STARS){
        throw UserSetupError(env.console, "Error in the settings, \"Min Stars\" is bigger than \"Max Stars\".");
    }

    if (FILTER.SKIP_HERBA && FILTER.MAX_STARS < 5){
        throw UserSetupError(env.console, "Error in the settings, Skip Non-Herba Raids is checked but Max Stars is less than 5.");
    }

    //  Connect the controller.
    pbf_press_button(context, BUTTON_LCLICK, 10, 10);

    bool first = true;

    while (true){
        env.update_stats();
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE);

        if (!first){
            day_skip_from_overworld(env.console, context);
            pbf_wait(context, GameSettings::instance().RAID_SPAWN_DELAY);
            context.wait_for_all_requests();
            stats.m_skips++;
        }
        first = false;

        if (open_raid(env.console, context)){
            stats.m_raids++;
            env.update_stats();
        }else{
            continue;
        }
        context.wait_for(std::chrono::milliseconds(500));

        VideoSnapshot screen = env.console.video().snapshot();
        TeraCardReader reader(COLOR_RED);
        size_t stars = reader.stars(screen);
        if (stars == 0){
            dump_image(env.logger(), env.program_info(), "ReadStarsFailed", *screen.frame);
        }

        VideoOverlaySet overlay_set(env.console);

        TeraSilhouetteReader silhouette_reader;
        silhouette_reader.make_overlays(overlay_set);
        ImageMatch::ImageMatchResult silhouette = silhouette_reader.read(screen);
        silhouette.log(env.logger(), 100);
        std::string best_silhouette = silhouette.results.empty() ? "UnknownSilhouette" : silhouette.results.begin()->second;
        if (silhouette.results.empty()){
            dump_image(env.logger(), env.program_info(), "ReadSilhouetteFailed", *screen.frame);
        }
        else if (PreloadSettings::debug().IMAGE_TEMPLATE_MATCHING){
            dump_debug_image(env.logger(), "PokemonSV/TeraSelfFarmer/" + best_silhouette, "", screen);
        }

        TeraTypeReader type_reader;
        type_reader.make_overlays(overlay_set);
        ImageMatch::ImageMatchResult type = type_reader.read(screen);
        type.log(env.logger(), 100);
        std::string best_type = type.results.empty() ? "UnknownType" : type.results.begin()->second;
        if (type.results.empty()){
            dump_image(env.logger(), env.program_info(), "ReadTypeFailed", *screen.frame);
        }
        else if (PreloadSettings::debug().IMAGE_TEMPLATE_MATCHING){
            dump_debug_image(env.logger(), "PokemonSV/TeraSelfFarmer/" + best_type, "", screen);
        }

        {
            std::string log = "Detected a " + std::to_string(stars) + "* " + best_type + " " + best_silhouette;
            env.console.overlay().add_log(log, COLOR_GREEN);
            env.log(log);
        }

        if (!FILTER.should_battle(stars, best_silhouette)) {
            env.log("Skipping raid...", COLOR_ORANGE);
            stats.m_skipped++;
            close_raid(env.program_info(), env.console, context);
            continue;
        }

        pbf_press_dpad(context, DPAD_DOWN, 10, 10);
        pbf_mash_button(context, BUTTON_A, 250); // Enter raid alone
        overlay_set.clear();

        TeraBattleMenuWatcher battle_menu(COLOR_CYAN);
        context.wait_for_all_requests();

        int ret = wait_until(
            env.console, context,
            std::chrono::seconds(100),
            {battle_menu}
        );

        if (ret == 0){
            env.console.log("Detected tera battle menu...");

            battle_menu.move_to_slot(env.console, context, 2);
            run_from_tera_battle(env.program_info(), env.console, context);
            context.wait_for_all_requests();

            open_pokedex_from_overworld(env.program_info(), env.console, context);
            open_recently_battled_from_pokedex(env.program_info(), env.console, context);

            pbf_wait(context, 200);

            // Loop through all 5 candidates of recently battled pokemon for shinies
            for(int i = 0; i < 5; i++){
                PokedexShinyWatcher shiny_detector;
                context.wait_for_all_requests();

                int ret2 = wait_until(
                    env.console, context,
                    std::chrono::seconds(1),
                    {shiny_detector}
                );

                if (ret2 == 0){
                    env.console.log("Detected shiny!");

                    stats.m_shinies += 1;
                    env.update_stats();

                    leave_phone_to_overworld(env.program_info(), env.console, context);
                    save_game_from_overworld(env.program_info(), env.console, context);

                    throw ProgramFinishedException();
                } else {
                    env.console.log("Not shiny...");
                }

                if (i < 4){
                    pbf_press_dpad(context, DPAD_RIGHT, 10, 20);
                }
            }
            leave_phone_to_overworld(env.program_info(), env.console, context);

            pbf_wait(context, 50);
        } else {
            env.console.log("Failed to enter tera raid after 100 seconds.");
        }

        {
            std::stringstream ss;
            ss << "You encountered a " << stars << "* " << best_type << " " << best_silhouette << " raid";
            env.log(ss.str());
            env.console.overlay().add_log(ss.str(), COLOR_GREEN);
        }
    }

    env.update_stats();
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}











}
}
}
