#include <SFML/Copyright.hpp> // LICENSE AND COPYRIGHT (C) INFORMATION

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#define MINIMP3_IMPLEMENTATION // Minimp3 control define, places implementation in this file.
#ifndef NOMINMAX
#define NOMINMAX // To avoid windows.h and min/max issue
#endif
#define MINIMP3_NO_STDIO // Minimp3 control define, eliminate file manipulation code which is useless here

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4242 4244 4267 4456 4706)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

#include <minimp3_ex.h>

#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

#undef NOMINMAX
#undef MINIMP3_NO_STDIO

#include "SFML/Audio/SoundFileReaderMp3.hpp"

#include "SFML/System/Err.hpp"
#include "SFML/System/InputStream.hpp"

#include "SFML/Base/Algorithm.hpp"
#include "SFML/Base/Assert.hpp"

#include <cstdint>
#include <cstring>


namespace
{
////////////////////////////////////////////////////////////
[[nodiscard]] std::size_t readCallback(void* ptr, std::size_t size, void* data)
{
    auto* stream = static_cast<sf::InputStream*>(data);
    return stream->read(ptr, size).valueOr(static_cast<std::size_t>(-1));
}


////////////////////////////////////////////////////////////
[[nodiscard]] int seekCallback(std::uint64_t offset, void* data)
{
    auto*                    stream   = static_cast<sf::InputStream*>(data);
    const sf::base::Optional position = stream->seek(static_cast<std::size_t>(offset));
    return position.hasValue() ? 0 : -1;
}


////////////////////////////////////////////////////////////
[[nodiscard]] bool hasValidId3Tag(const std::uint8_t* header)
{
    return std::memcmp(header, "ID3", 3) == 0 &&
           !((header[5] & 15) || (header[6] & 0x80) || (header[7] & 0x80) || (header[8] & 0x80) || (header[9] & 0x80));
}

} // namespace

namespace sf::priv
{
////////////////////////////////////////////////////////////
struct SoundFileReaderMp3::Impl
{
    mp3dec_io_t   io{};
    mp3dec_ex_t   decoder{};
    std::uint64_t numSamples{}; // Decompressed audio storage size
    std::uint64_t position{};   // Position in decompressed audio buffer
};


////////////////////////////////////////////////////////////
bool SoundFileReaderMp3::check(InputStream& stream)
{
    std::uint8_t header[10];

    if (base::Optional readResult = stream.read(header, sizeof(header));
        !readResult.hasValue() || *readResult != sizeof(header))
        return false;

    if (hasValidId3Tag(header))
        return true;

    if (hdr_valid(header))
        return true;

    return false;
}


////////////////////////////////////////////////////////////
SoundFileReaderMp3::SoundFileReaderMp3()
{
    m_impl->io.read = readCallback;
    m_impl->io.seek = seekCallback;
}


////////////////////////////////////////////////////////////
SoundFileReaderMp3::~SoundFileReaderMp3()
{
    mp3dec_ex_close(&m_impl->decoder);
}


////////////////////////////////////////////////////////////
base::Optional<SoundFileReader::Info> SoundFileReaderMp3::open(InputStream& stream)
{
    // Init IO callbacks
    m_impl->io.read_data = &stream;
    m_impl->io.seek_data = &stream;

    base::Optional<Info> result; // Use a single local variable for NRVO

    // Init mp3 decoder
    mp3dec_ex_open_cb(&m_impl->decoder, &m_impl->io, MP3D_SEEK_TO_SAMPLE);
    if (!m_impl->decoder.samples)
        return result; // Empty optional

    // Retrieve the music attributes
    Info& info        = result.emplace();
    info.channelCount = static_cast<unsigned int>(m_impl->decoder.info.channels);
    info.sampleRate   = static_cast<unsigned int>(m_impl->decoder.info.hz);
    info.sampleCount  = m_impl->decoder.samples;

    // MP3 only supports mono/stereo channels
    switch (info.channelCount)
    {
        case 0:
            priv::err() << "No channels in MP3 file";
            break;
        case 1:
            info.channelMap = {SoundChannel::Mono};
            break;
        case 2:
            info.channelMap = {SoundChannel::SideLeft, SoundChannel::SideRight};
            break;
        default:
            priv::err() << "MP3 files with more than 2 channels not supported";
            SFML_BASE_ASSERT(false);
            break;
    }

    m_impl->numSamples = info.sampleCount;
    return result;
}


////////////////////////////////////////////////////////////
void SoundFileReaderMp3::seek(std::uint64_t sampleOffset)
{
    m_impl->position = base::min(sampleOffset, m_impl->numSamples);
    mp3dec_ex_seek(&m_impl->decoder, m_impl->position);
}


////////////////////////////////////////////////////////////
std::uint64_t SoundFileReaderMp3::read(std::int16_t* samples, std::uint64_t maxCount)
{
    std::uint64_t toRead = base::min(maxCount, m_impl->numSamples - m_impl->position);
    toRead               = std::uint64_t{mp3dec_ex_read(&m_impl->decoder, samples, static_cast<std::size_t>(toRead))};
    m_impl->position += toRead;
    return toRead;
}

} // namespace sf::priv
