/*  Audio Constants
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_AudioConstants_H
#define PokemonAutomation_AudioConstants_H

#include <cmath>

//  Largest FFT that is no more than 1/10 of a second.
const int FFT_LENGTH_POWER_OF_TWO = 12;

const int NUM_FFT_SAMPLES = 1 << FFT_LENGTH_POWER_OF_TWO;
//const int NUM_FFT_WINDOWS = 100;
const int FFT_SLIDING_WINDOW_STEP = NUM_FFT_SAMPLES/4;


#endif
