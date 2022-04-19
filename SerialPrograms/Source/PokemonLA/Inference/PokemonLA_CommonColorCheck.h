/*  Common Color Check
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 * 
 *  Several checks for commonly used color in LA. These checks are used by various inference detector classes.
 */

#ifndef PokemonAutomation_PokemonLA_MapDetector_H
#define PokemonAutomation_PokemonLA_MapDetector_H

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/InferenceInfra/VisualInferenceCallback.h"

namespace PokemonAutomation{

struct ImageStats;

namespace NintendoSwitch{
namespace PokemonLA{

// The dark blue used as the background of the title text on normal and surprise diallgue boxes.
// The color is also used as the selected tab on the top of the screen in the main menu space
// (entered by pressing DPAD_UP from overworld).
bool is_LA_dark_blue(const ImageStats& stats);



}
}
}
#endif
