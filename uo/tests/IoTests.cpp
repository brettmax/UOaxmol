// SPDX-License-Identifier: BSD-2-Clause
#include "TestUtil.h"
#include "doctest.h"

#include "uo/io/BinaryReader.h"
#include "uo/io/ClientVersion.h"
#include "uo/io/UOFile.h"

using namespace uo;
using namespace uotest;

TEST_CASE("client versions parse and order")
{
    CHECK(parseClientVersion("7.0.15.1") == makeVersion(7, 0, 15, 1));
    CHECK(parseClientVersion("2.0.0") == cv::CV_200);
    CHECK(parseClientVersion("4.0.11d") == cv::CV_4011D);
    CHECK_FALSE(parseClientVersion("7..1").has_value());
    CHECK_FALSE(parseClientVersion("banana").has_value());
    CHECK(*parseClientVersion("7.0.9.0") >= cv::CV_7090);
    CHECK(*parseClientVersion("2.0.3") < cv::CV_500A);
    CHECK(clientVersionToString(makeVersion(7, 0, 15, 1)) == "7.0.15.1");
    CHECK(clientVersionToString(cv::CV_4011D) == "4.0.11d");
}

TEST_CASE("binary reader handles both endians and never reads past the end")
{
    Bytes b{0x01, 0x02, 0x03, 0x04, 'a', 'b', 0, 'z'};
    io::BinaryReader r(b);
    CHECK(r.readU16LE() == 0x0201);
    CHECK(r.readU16BE() == 0x0304);
    CHECK(r.readASCII(4) == "ab");
    CHECK(r.remaining() == 0);
    CHECK(r.readU32BE() == 0);
    CHECK(r.overflowed());
}

TEST_CASE("UOP name hash matches lookup3 hashlittle2")
{
    // Reference vectors from Bob Jenkins' lookup3.c driver5 with pc = pb = 0; the UO variant
    // returns (b << 32) | c.
    CHECK(io::UopFile::hash("Four score and seven years ago") == 0xCE7226E617770551ull);
    CHECK(io::UopFile::formatName("build/artlegacymul/%08u.tga", 42) == "build/artlegacymul/00000042.tga");
}

TEST_CASE("MUL index entries resolve to data ranges")
{
    TempDir dir;
    Bytes data{'x', 'x', 'h', 'e', 'l', 'l', 'o'};
    Bytes idx;
    le32(idx, 2), le32(idx, 5), le32(idx, (10u << 16) | 20u);  // entry 0
    le32(idx, 0xFFFFFFFF), le32(idx, 0), le32(idx, 0);         // entry 1: absent

    io::MulFile f(dir.write("test.mul", data), dir.write("testidx.mul", idx));
    REQUIRE(f.load());
    REQUIRE(f.count() == 2);
    auto e = f.read(0);
    CHECK(std::string(e.begin(), e.end()) == "hello");
    CHECK(f.entry(0)->width == 10);
    CHECK(f.entry(0)->height == 20);
    CHECK(f.entry(1) == nullptr);
    CHECK(f.read(99).empty());
}

TEST_CASE("UOP archives resolve entries by hashed virtual name")
{
    TempDir dir;
    const std::string pattern = "build/test/%08u.bin";
    Bytes payload{'u', 'o', 'p', '!'};

    Bytes f;
    le32(f, 0x50594D);  // magic
    le32(f, 5);         // version
    le32(f, 0xFD23EC43);
    le64(f, 40);  // first block
    le32(f, 100);
    le32(f, 1);
    while (f.size() < 40)
        f.push_back(0);

    std::size_t dataAt = 40 + 4 + 8 + 34;
    le32(f, 1);  // files in block
    le64(f, 0);  // next block
    le64(f, dataAt);
    le32(f, 0);                                   // header length
    le32(f, static_cast<std::uint32_t>(payload.size()));  // compressed
    le32(f, static_cast<std::uint32_t>(payload.size()));  // decompressed
    le64(f, io::UopFile::hash(io::UopFile::formatName(pattern, 7)));
    le32(f, 0);
    le16(f, 0);
    f.insert(f.end(), payload.begin(), payload.end());

    io::UopFile uop(dir.write("test.uop", f), pattern);
    REQUIRE(uop.load());
    auto e = uop.read(7);
    CHECK(std::string(e.begin(), e.end()) == "uop!");
    CHECK(uop.read(6).empty());
}

#include "uo/io/Compression.h"

TEST_CASE("zlib inflate round-trips and rejects garbage")
{
    Bytes plain;
    for (int i = 0; i < 5000; ++i)
        plain.push_back(static_cast<std::uint8_t>(i * 7));
    Bytes packed = io::deflate(plain);
    REQUIRE_FALSE(packed.empty());
    CHECK(packed.size() < plain.size());

    Bytes out;
    CHECK(io::inflate(packed, out, plain.size()));
    CHECK(out == plain);
    CHECK(io::inflate(packed, out));  // unknown size grows the buffer
    CHECK(out == plain);

    Bytes junk{1, 2, 3, 4};
    CHECK_FALSE(io::inflate(junk, out));
}
