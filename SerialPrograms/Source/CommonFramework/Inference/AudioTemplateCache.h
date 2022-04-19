/*  Audio Template Cache
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_CommonFramework_AudioTemplateCache_H
#define PokemonAutomation_CommonFramework_AudioTemplateCache_H

#include <map>
#include "Common/Cpp/SpinLock.h"

class QString;

namespace PokemonAutomation{

class AudioTemplate;



class AudioTemplateCache{
public:
    // path: the path of the basename of the audio template file relative to RESOURCE_PATH().
    // sample_rate: target sample rate. Used to locate the audio template file with the desired sample rate.
    //
    // The function will first try to load the file using .wav extension. If .wav not available, load .mp3.
    // e.g. if `path` is "PokemonLA/ShinySound" and `sample_rate` is 48000, it will load
    // RESOURCE_PATH/PokemonLA/ShinySound-48000.wav if it exists. If not, it will load
    // RESOURCE_PATH/PokemonLA/ShinySound-48000.mp3 instead.
    // Won't throw if cannot read or parse the template file. Return nullptr in this case.
    const AudioTemplate* get_nothrow(const QString& path, size_t sample_rate);
    // See comment of AudioTemplateCache::get_nothrow().
    // Throw a FileException if cannot read or parse the template file.
    const AudioTemplate& get_throw(const QString& path, size_t sample_rate);

    static AudioTemplateCache& instance();

private:
    ~AudioTemplateCache();
    AudioTemplateCache();

    const AudioTemplate* get_nothrow_internal(const QString& full_path_no_ext, size_t sample_rate);


private:
    SpinLock m_lock;
    std::map<QString, AudioTemplate> m_cache;
};






}
#endif
