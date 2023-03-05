/*  Audio Passthrough Pair (Qt separate thread)
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "Common/Cpp/Concurrency/SpinPause.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "AudioPassthroughPairQt.h"
#include "AudioPassthroughPairQtThread.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{


void AudioPassthroughPairQtThread::add_listener(FFTListener& listener){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->add_listener(listener);
}
void AudioPassthroughPairQtThread::remove_listener(FFTListener& listener){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->remove_listener(listener);
}


AudioPassthroughPairQtThread::AudioPassthroughPairQtThread(Logger& logger)
    : m_logger(logger)
    , m_body(nullptr)
{
    start();

    //  Wait for the thread to fully start up and construct the body.
    while (m_body.load(std::memory_order_acquire) == nullptr){
        pause();
    }
}
AudioPassthroughPairQtThread::~AudioPassthroughPairQtThread(){
    m_body.store(nullptr, std::memory_order_relaxed);
    quit();
    wait();
}
void AudioPassthroughPairQtThread::run(){
    GlobalSettings::instance().REALTIME_THREAD_PRIORITY0.set_on_this_thread();

    AudioPassthroughPairQt body(m_logger);
    m_body.store(&body, std::memory_order_relaxed);
    exec();

    //  Wait until we are in the destructor before destroying the body.
    while (m_body.load(std::memory_order_acquire) != nullptr){
        pause();
    }
}


void AudioPassthroughPairQtThread::reset(
    const std::string& file,
    const AudioDeviceInfo& output, double output_volume
){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->reset(file, output, output_volume);
}
void AudioPassthroughPairQtThread::reset(
    const AudioDeviceInfo& input, AudioChannelFormat format,
    const AudioDeviceInfo& output, double output_volume
){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->reset(input, format, output, output_volume);
}
void AudioPassthroughPairQtThread::clear_audio_source(){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->clear_audio_source();
}
void AudioPassthroughPairQtThread::set_audio_source(const std::string& file){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->set_audio_source(file);
}
void AudioPassthroughPairQtThread::set_audio_source(const AudioDeviceInfo& device, AudioChannelFormat format){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->set_audio_source(device, format);
}
void AudioPassthroughPairQtThread::clear_audio_sink(){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->clear_audio_sink();
}
void AudioPassthroughPairQtThread::set_audio_sink(const AudioDeviceInfo& device, double volume){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->set_audio_sink(device, volume);
}
void AudioPassthroughPairQtThread::set_sink_volume(double volume){
    AudioPassthroughPairQt* body = m_body.load(std::memory_order_relaxed);
    body->set_sink_volume(volume);
}




}
