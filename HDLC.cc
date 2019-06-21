#ifndef HDLC_H
#define HDLC_H

#include "Array.h"
#include <array>
#include <cstdint>
#include <vector>

struct BasicHeader
{
    static constexpr uint8_t flag = 0xFE;
    uint8_t address{};
    uint8_t control{};

    static constexpr std::size_t size = sizeof(flag) + sizeof(address) + sizeof(control);

    constexpr auto serialize() const
    {
        return ByteArray::byteArray(flag, address, control);
    }
};

struct BasicData
{
    std::array<uint8_t, 8> info{};

    template <typename T>
    constexpr operator T()
    {
        return ByteArray::getInteger<T, sizeof(T)>(info);
    }
    constexpr auto serialize() const
    {
        return info;
    }
};

template <typename T>
constexpr uint16_t CRC16(T const& data)
{
    return ByteArray::getInteger<uint16_t,2>(data) + 0x4E;
}

struct BasicFooter
{
    uint16_t fcs{};
    static constexpr uint8_t flag = 0xFE;

    template <typename T>
    constexpr BasicFooter(T const& data) : fcs(CRC16(data))
    {
    }
    constexpr auto serialize() const
    {
        return ByteArray::byteArray(fcs, flag);
    }
    static constexpr std::size_t size = sizeof(fcs) + sizeof(flag);
};

template <class Header, class Data, class Footer>
struct HDLC
{
    Header header{};
    Data data{};
    Footer footer{data.info};
    static constexpr std::size_t size = Header::size + Data::size + Footer::size;

    constexpr auto serialize() const
    {
        return RangedArray::merge(header.serialize(), data.serialize(), footer.serialize());
    }
};

using HDLCFrame = HDLC<BasicHeader, BasicData, BasicFooter>;

HDLCFrame my_frame{0xCE, 0x01, {0xDE, 0xAD, 0xBE, 0xEF, 0xFA, 0xCE, 0xB0, 0xA7}};

#endif

