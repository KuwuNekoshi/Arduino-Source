/*  Common Color Check
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

// #include "CommonFramework/ImageTools/SolidColorTest.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLA_CommonColorCheck.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLA{


bool is_LA_dark_blue(const ImageStats& stats){
    return (stats.average.sum() <= 300 && stats.stddev.sum() <= 15 &&
        stats.average.b > stats.average.r && stats.average.b > stats.average.g
    );
}


}
}
}
