// Small tests that run without the game. They use synthetic data and cover the archive format,
// CR2W path replacement, CRCs, naming, the manifest, the conversion and the report. tools/package.ps1 runs them after the build (ctest).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <RED4ext/ResourcePath.hpp>

#include "AdCatalog.hpp"
#include "Archive.hpp"
#include "Cr2w.hpp"
#include "Manifest.hpp"
#include "Naming.hpp"
#include "Report.hpp"
#include "Util.hpp"
#include "Variant.hpp"

namespace fs = std::filesystem;

namespace
{
int g_failures = 0;

#define CHECK(aCondition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(aCondition))                                                                                             \
        {                                                                                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #aCondition);                                          \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (false)

std::vector<uint8_t> Bytes(std::string_view aText)
{
    return {aText.begin(), aText.end()};
}

using Util::Get;
using Util::Set;

std::vector<uint8_t> ReadAll(const fs::path& aPath)
{
    std::ifstream in(aPath, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

uint32_t Crc32(const std::vector<uint8_t>& aData, size_t aOffset, size_t aSize)
{
    return Cr2w::Crc32(aData.data() + aOffset, aSize);
}

// ---- CRC ------------------------------------------------------------------------------------------------------

void TestCrc()
{
    const auto check = Bytes("123456789");
    // Standard check values (CRC-32/IEEE and CRC-64/XZ)
    CHECK(Cr2w::Crc32(check.data(), check.size()) == 0xCBF43926u);
    CHECK(Archive::Crc64(check.data(), check.size()) == 0x995DC9BBDF1939FAull);
}

// ---- Archive -------------------------------------------------------------------------------------------------

void TestArchiveRoundTrip()
{
    const auto path = fs::temp_directory_path() / "adcoexist_unittest.archive";

    std::vector<Archive::File> files(2);
    files[0].hash = 0x30;
    files[0].segments.push_back({5, Bytes("hello")});
    files[1].hash = 0x10;
    files[1].inlineBuffers = 1;
    files[1].segments.push_back({100, Bytes("KARK-like compressed bytes")}); // segment copied as stored
    files[1].segments.push_back({3, Bytes("abc")});

    std::string error;
    CHECK(Archive::Write(path, files, error));

    // Header and index layout
    const auto raw = ReadAll(path);
    CHECK(raw.size() % 0x1000 == 0);
    CHECK(Get<uint32_t>(raw, 0) == 0x52414452); // RDAR
    CHECK(Get<uint32_t>(raw, 4) == 12);
    const auto indexPos = static_cast<size_t>(Get<uint64_t>(raw, 8));
    const auto indexSize = Get<uint32_t>(raw, 16);
    CHECK(indexPos % 0x1000 == 0);
    CHECK(Get<uint64_t>(raw, 32) == raw.size());
    const auto tableSize = Get<uint32_t>(raw, indexPos + 4);
    CHECK(indexSize == tableSize + 8);
    CHECK(Get<uint64_t>(raw, indexPos + 8) == Archive::Crc64(raw.data() + indexPos + 16, tableSize - 8));

    // Read back: sorted by hash, segments as written
    std::vector<Archive::File> read;
    CHECK(Archive::Read(path, read, error));
    CHECK(read.size() == 2);
    if (read.size() == 2)
    {
        CHECK(read[0].hash == 0x10 && read[1].hash == 0x30);
        CHECK(read[0].inlineBuffers == 1);
        CHECK(read[0].segments.size() == 2);
        if (read[0].segments.size() == 2)
        {
            CHECK(read[0].segments[0].size == 100);
            CHECK(read[0].segments[0].data == Bytes("KARK-like compressed bytes"));
            CHECK(read[0].segments[1].data == Bytes("abc"));
        }
        CHECK(read[1].segments.size() == 1 && read[1].segments[0].data == Bytes("hello"));
    }

    // Read only the given hashes
    const std::unordered_set<uint64_t> only{0x30};
    CHECK(Archive::Read(path, read, error, &only));
    CHECK(read.size() == 1 && read[0].hash == 0x30);

    // Read only the hashes in the index
    std::vector<uint64_t> hashes;
    CHECK(Archive::ReadHashes(path, hashes, error));
    CHECK(hashes == (std::vector<uint64_t>{0x10, 0x30}));

    // Not an archive
    {
        std::ofstream(path, std::ios::binary | std::ios::trunc) << "not an archive at all, just some text";
    }
    CHECK(!Archive::Read(path, read, error));
    CHECK(!Archive::ReadHashes(path, hashes, error));

    std::error_code ec;
    fs::remove(path, ec);
}

// ---- CR2W ------------------------------------------------------------------------------------------------------

// Builds a minimal CR2W file with only a string table and a buffer table
std::vector<uint8_t> MakeCr2w(const std::string& aStrings, uint32_t aBufferCount)
{
    constexpr size_t tablesEnd = 40 + 10 * 12;
    std::vector<uint8_t> main(tablesEnd, 0);
    Set<uint32_t>(main, 0, 0x57325243); // CR2W
    Set<uint32_t>(main, 4, 195);

    const auto stringsOffset = static_cast<uint32_t>(main.size());
    main.insert(main.end(), aStrings.begin(), aStrings.end());
    Set<uint32_t>(main, 40, stringsOffset);
    Set<uint32_t>(main, 44, static_cast<uint32_t>(aStrings.size()));

    const auto bufferTable = static_cast<uint32_t>(main.size());
    main.resize(main.size() + aBufferCount * 24, 0);
    Set<uint32_t>(main, 40 + 5 * 12, bufferTable);
    Set<uint32_t>(main, 40 + 5 * 12 + 4, aBufferCount);

    Set<uint32_t>(main, 24, static_cast<uint32_t>(main.size())); // objectsEnd
    return main;
}

uint32_t HeaderCrc(const std::vector<uint8_t>& aMain)
{
    std::vector<uint8_t> header(aMain.begin(), aMain.begin() + 32);
    const uint32_t deadBeef = 0xDEADBEEF;
    header.insert(header.end(), reinterpret_cast<const uint8_t*>(&deadBeef),
                  reinterpret_cast<const uint8_t*>(&deadBeef) + 4);
    header.insert(header.end(), aMain.begin() + 36, aMain.begin() + 40 + 10 * 12);
    return Crc32(header, 0, header.size());
}

void TestCr2wReplacePaths()
{
    const std::string from = "base\\x\\a.xbm";
    const std::string to = "adco\\y\\a.xbm"; // same length
    auto main = MakeCr2w(std::string("\0", 1) + from + std::string("\0other\0", 7), 2);
    std::vector<std::vector<uint8_t>> buffers{Bytes("zz" + from + "zz"), Bytes("no path here")};

    std::string error;
    CHECK(Cr2w::ReplacePaths(main, buffers, {{from, to}}, error) == 2);

    const auto stringsOffset = Get<uint32_t>(main, 40);
    const auto stringsSize = Get<uint32_t>(main, 44);
    const std::string strings(reinterpret_cast<const char*>(main.data() + stringsOffset), stringsSize);
    CHECK(strings.find(to) != std::string::npos);
    CHECK(strings.find(from) == std::string::npos);
    CHECK(buffers[0] == Bytes("zz" + to + "zz"));

    // Checksums and the buffer table
    CHECK(Get<uint32_t>(main, 48) == Crc32(main, stringsOffset, stringsSize));
    const auto objectsEnd = Get<uint32_t>(main, 24);
    const auto bufferTable = Get<uint32_t>(main, 40 + 5 * 12);
    uint32_t position = objectsEnd;
    for (uint32_t i = 0; i < 2; ++i)
    {
        const size_t entry = bufferTable + i * 24;
        const auto size = static_cast<uint32_t>(buffers[i].size());
        CHECK(Get<uint32_t>(main, entry + 8) == position);
        CHECK(Get<uint32_t>(main, entry + 12) == size);
        CHECK(Get<uint32_t>(main, entry + 16) == size);
        CHECK(Get<uint32_t>(main, entry + 20) == Crc32(buffers[i], 0, size));
        position += size;
    }
    CHECK(Get<uint32_t>(main, 40 + 5 * 12 + 8) == Crc32(main, bufferTable, 2 * 24));
    CHECK(Get<uint32_t>(main, 28) == position); // buffersEnd
    CHECK(Get<uint32_t>(main, 32) == HeaderCrc(main));
}

void TestCr2wRejects()
{
    std::string error;
    auto main = MakeCr2w(std::string("\0base\\x\\a.xbm\0", 14), 0);
    std::vector<std::vector<uint8_t>> buffers;
    CHECK(Cr2w::ReplacePaths(main, buffers, {{"base\\x\\a.xbm", "longer\\path\\a.xbm"}}, error) == -1); // different length

    auto notCr2w = Bytes(std::string(200, 'x'));
    CHECK(Cr2w::ReplacePaths(notCr2w, buffers, {}, error) == -1);

    std::vector<std::vector<uint8_t>> extraBuffer{Bytes("x")};
    CHECK(Cr2w::ReplacePaths(main, extraBuffer, {}, error) == -1); // buffer count doesn't match the table
}
// ---- Naming -------------------------------------------------------------------------------------------------

void TestPathHash()
{
    // Same normalization as the game's ResourcePath
    for (const char* path : {"base\\gameplay\\gui\\world\\adverts\\naranjita\\naranjita_atlas.xbm",
                             "Base\\Gameplay\\GUI\\a.XBM", "base/gameplay//gui\\\\a.xbm", "\\\\base\\a.xbm",
                             "\"base\\a.xbm\"", "'base\\a.xbm'", "adcoexist\\0dccafac2a19c9da\\advt\\x\\y.inkwidget"})
    {
        CHECK(Naming::PathHash(path) == RED4ext::ResourcePath::HashSanitized(path));
    }
    CHECK(Naming::PathHash("") == 0);
    const std::string longPath(300, 'a');
    CHECK(Naming::PathHash(longPath) == RED4ext::ResourcePath::HashSanitized(longPath.c_str()));
}

void TestNaming()
{
    // ID seen in a real game run
    CHECK(Naming::ArchiveId("archive\\pc\\mod\\AdCoexistTest A\\adcoexist_test.archive") == "0dccafac2a19c9da");
    CHECK(Naming::ArchiveId("ARCHIVE\\pc\\mod\\AdCoexistTest A\\adcoexist_test.archive") == "0dccafac2a19c9da");
    CHECK(Naming::ArchiveId("archive\\pc\\mod\\AdCoexistTest B\\adcoexist_test.archive").substr(0, 6) == "0ac78e");

    const std::string id = "0123456789abcdef";
    const std::string base = "base\\gameplay\\gui\\world\\adverts\\naranjita\\naranjita.inkwidget";
    const auto baseNew = Naming::NewPath(base, id);
    CHECK(baseNew == "adcoexist\\0123456789abcdef\\advt\\naranjita\\naranjita.inkwidget");
    CHECK(baseNew.size() == base.size());
    const std::string ep1 = "ep1\\gameplay\\gui\\world\\adverts\\montana\\montana.xbm";
    const auto ep1New = Naming::NewPath(ep1, id);
    CHECK(ep1New == "adcoexist\\0123456789abcdef\\adv\\montana\\montana.xbm");
    CHECK(ep1New.size() == ep1.size());
    CHECK(Naming::NewPath("base\\other\\a.xbm", id).empty());
    CHECK(Naming::NewPath(base, "short").empty()); // lengths don't match

    CHECK(Naming::RecordStem("Kotone Naranjita-1.v2") == "Kotone_Naranjita_1_v2");
    CHECK(Naming::VariantRecordName("basegame_KotoneNaranjita", "Adverts.Naranjita_Animated") ==
          "Adverts.adcoexist_basegame_KotoneNaranjita_Naranjita_Animated");

    const auto stems = Naming::AssignRecordStems({
        {"archive\\pc\\mod\\AdCoexistTest A\\adcoexist_test.archive", "adcoexist_test"},
        {"archive\\pc\\mod\\AdCoexistTest B\\adcoexist_test.archive", "adcoexist_test"},
        {"archive\\pc\\mod\\basegame_sakagura.archive", "basegame_sakagura"},
    });
    CHECK(stems.at("archive\\pc\\mod\\AdCoexistTest A\\adcoexist_test.archive") == "adcoexist_test_0dccaf");
    CHECK(stems.at("archive\\pc\\mod\\AdCoexistTest B\\adcoexist_test.archive") == "adcoexist_test_0ac78e");
    CHECK(stems.at("archive\\pc\\mod\\basegame_sakagura.archive") == "basegame_sakagura");
}

// ---- Manifest ------------------------------------------------------------------------------------------------------

void TestManifest()
{
    Manifest::Inputs inputs;
    inputs.versionMajor = 0;
    inputs.versionMinor = 15;
    inputs.versionPatch = 1;
    inputs.catalogGameVersion = "2.50";
    inputs.catalogTweakDbChecksum = 0x6dee4a6e;
    inputs.catalogTweakDbEp1Checksum = 0x6dee4a6e;
    inputs.tweakDbChecksum = 0x6dee4a6e;
    inputs.mods.push_back({"archive\\pc\\mod\\a.archive", 1597440, 132923815293828676});
    const auto fingerprint = Manifest::Fingerprint(inputs);
    CHECK(fingerprint == "adcoexist-manifest 1\n"
                         "adcoexist 0.15.1\n"
                         "catalog 2.50 6dee4a6e 6dee4a6e\n"
                         "tweakdb 6dee4a6e\n"
                         "mod 1597440 132923815293828676 archive\\pc\\mod\\a.archive\n");

    Manifest::Outputs outputs;
    outputs.archiveSize = 5017600;
    outputs.yamlSize = 5105;
    const auto manifest = Manifest::Compose(fingerprint, outputs);
    CHECK(manifest == fingerprint + "--\noutput 5017600 5105\n");
    CHECK(Manifest::Decide(manifest, fingerprint, true, outputs) == Manifest::Reuse::Generated);
    CHECK(Manifest::Decide(manifest, fingerprint, false, outputs) == Manifest::Reuse::No); // generated files missing

    auto changed = outputs;
    changed.yamlSize += 1;
    CHECK(Manifest::Decide(manifest, fingerprint, true, changed) == Manifest::Reuse::No); // generated file size changed

    auto otherInputs = inputs;
    otherInputs.mods[0].time += 1;
    CHECK(Manifest::Decide(manifest, Manifest::Fingerprint(otherInputs), true, outputs) == Manifest::Reuse::No);

    Manifest::Outputs none;
    none.empty = true;
    CHECK(Manifest::Decide(Manifest::Compose(fingerprint, none), fingerprint, false, outputs) ==
          Manifest::Reuse::Nothing);
    CHECK(Manifest::Decide("", fingerprint, true, outputs) == Manifest::Reuse::No);
    CHECK(Manifest::Decide(manifest + "x", fingerprint, true, outputs) == Manifest::Reuse::No); // corrupted manifest

    // Why the previous result isn't reused
    CHECK(Manifest::ExplainNo("", fingerprint, true, outputs) == "no previous result");
    CHECK(Manifest::ExplainNo(manifest, Manifest::Fingerprint(otherInputs), true, outputs) ==
          "changed: mod 1597440 132923815293828677 archive\\pc\\mod\\a.archive");
    auto added = inputs;
    added.mods.push_back({"archive\\pc\\mod\\b.archive", 1, 2});
    CHECK(Manifest::ExplainNo(manifest, Manifest::Fingerprint(added), true, outputs) ==
          "changed: mod 1 2 archive\\pc\\mod\\b.archive");
    auto removed = inputs;
    removed.mods.clear();
    CHECK(Manifest::ExplainNo(manifest, Manifest::Fingerprint(removed), true, outputs) ==
          "changed: a mod was removed");
    CHECK(Manifest::ExplainNo(manifest, fingerprint, false, outputs) == "the generated files are missing");
    CHECK(Manifest::ExplainNo(manifest, fingerprint, true, changed) ==
          "the generated files changed (expected output 5017600 5105, found output 5017600 5106)");
    CHECK(Manifest::ExplainNo("adcoexist-manifest 1\n", fingerprint, true, outputs) == "changed: adcoexist 0.15.1");
}

// ---- Conversion of one ad ------------------------------------------------------------------------------------------------

Archive::File RawFile(const std::string& aPath, std::vector<std::vector<uint8_t>> aSegments)
{
    Archive::File file;
    file.hash = Naming::PathHash(aPath);
    for (auto& data : aSegments)
    {
        const auto size = static_cast<uint32_t>(data.size());
        file.segments.push_back({size, std::move(data)}); // raw (zsize == size), so Oodle isn't needed
    }
    return file;
}

Variant::FileReader Reader(const std::vector<Archive::File>& aFiles, std::vector<uint64_t>* aAsked = nullptr)
{
    return [aFiles, aAsked](const std::unordered_set<uint64_t>& aHashes,
                            std::unordered_map<uint64_t, Archive::File>& aOut) {
        for (auto hash : aHashes)
        {
            if (aAsked)
                aAsked->push_back(hash);
            bool found = false;
            for (const auto& file : aFiles)
            {
                if (file.hash == hash)
                {
                    aOut[hash] = file;
                    found = true;
                }
            }
            if (!found)
                return false;
        }
        return true;
    };
}

std::string Text(const Archive::File& aFile)
{
    std::string text;
    for (const auto& segment : aFile.segments)
        text.append(segment.data.begin(), segment.data.end());
    return text;
}

void TestVariant()
{
    const std::string folder = "base\\gameplay\\gui\\world\\adverts\\testad\\";
    const std::string image = folder + "testad.xbm";
    const std::string image2 = folder + "testad_1080p.xbm";
    const std::string atlas = folder + "testad.inkatlas";
    const std::string widget = folder + "testad.inkwidget";
    const std::string widgetAnimated = folder + "testad_animated.inkwidget";

    const AdCatalog::File files[] = {
        {image.c_str(), AdCatalog::FileKind::Image},
        {image2.c_str(), AdCatalog::FileKind::Image},
        {atlas.c_str(), AdCatalog::FileKind::Atlas},
        {widget.c_str(), AdCatalog::FileKind::Widget},
        {widgetAnimated.c_str(), AdCatalog::FileKind::Widget},
    };
    const char* const districts[] = {"Adverts.Kabuki.advertisements", "Adverts.Glen.advertisements"};
    const AdCatalog::Record mixed[] = {
        {"Adverts.TestAd", widget.c_str(), districts, 2},
        {"Adverts.TestAd_Animated", widgetAnimated.c_str(), nullptr, 0}, // no districts (an ad with only some records in districts)
    };
    const AdCatalog::Ad ad{folder.c_str(), files, 5, mixed, 2};

    // The vanilla .inkatlas references the images, and the .inkwidget references the .inkatlas
    const auto vanilla = std::vector<Archive::File>{
        RawFile(atlas, {MakeCr2w(std::string("\0", 1) + image + std::string("\0", 1) + image2 + std::string("\0", 1), 0)}),
        RawFile(widget, {MakeCr2w(std::string("\0", 1) + atlas + std::string("\0", 1), 1), Bytes("pkg " + atlas)}),
        RawFile(widgetAnimated, {MakeCr2w(std::string("\0", 1) + atlas + std::string("\0", 1), 0)}),
    };
    // The mod replaces only one image
    const auto mod = std::vector<Archive::File>{RawFile(image, {Bytes("header"), Bytes("mod pixels")})};

    const std::string id = "0123456789abcdef";
    Variant::Request request;
    request.ad = &ad;
    request.provided = {&files[0]};
    request.archiveId = id;
    request.recordStem = "testmod";

    Variant::Result result;
    std::string error;
    CHECK(Variant::Build(request, Reader(mod), Reader(vanilla), result, error));

    const auto newImage = Naming::NewPath(image, id);
    const auto newAtlas = Naming::NewPath(atlas, id);
    const auto newWidget = Naming::NewPath(widget, id);
    CHECK(result.files.size() == 4); // 1 image, .inkatlas, 2 .inkwidget
    for (const auto& file : result.files)
    {
        if (file.hash == Naming::PathHash(newImage))
        {
            CHECK(Text(file) == "headermod pixels"); // images are copied as they are
        }
        else if (file.hash == Naming::PathHash(newAtlas))
        {
            CHECK(Text(file).find(newImage) != std::string::npos); // the image replaced by the mod points to the new path
            CHECK(Text(file).find(image2) != std::string::npos);   // images not replaced stay vanilla
        }
        else if (file.hash == Naming::PathHash(newWidget))
        {
            CHECK(Text(file).find(newAtlas) != std::string::npos);
            CHECK(Text(file).find(atlas) == std::string::npos);
            CHECK(file.segments.size() == 2 && Text(file).find("pkg " + newAtlas) != std::string::npos);
        }
        else
        {
            CHECK(file.hash == Naming::PathHash(Naming::NewPath(widgetAnimated, id)));
        }
    }
    CHECK(result.restore == (std::unordered_set<uint64_t>{Naming::PathHash(image)}));

    // Only records in districts are added, to the same districts as vanilla
    CHECK(result.recordNames == (std::vector<std::string>{"Adverts.adcoexist_testmod_TestAd"}));
    CHECK(result.yaml == "Adverts.adcoexist_testmod_TestAd:\n"
                         "  $base: Adverts.TestAd\n"
                         "  resource: " + newWidget + "\n"
                         "Adverts.Kabuki.advertisements:\n"
                         "  - !prepend Adverts.adcoexist_testmod_TestAd\n"
                         "Adverts.Glen.advertisements:\n"
                         "  - !prepend Adverts.adcoexist_testmod_TestAd\n");

    // An ad without districts is added to all 18 districts
    const AdCatalog::Record none[] = {{"Adverts.TestAd", widget.c_str(), nullptr, 0}};
    const AdCatalog::Ad noDistrictAd{folder.c_str(), files, 5, none, 1};
    request.ad = &noDistrictAd;
    CHECK(Variant::Build(request, Reader(mod), Reader(vanilla), result, error));
    CHECK(result.recordNames.size() == 1);
    size_t prepends = 0;
    for (size_t at = 0; (at = result.yaml.find("!prepend", at)) != std::string::npos; ++at)
        ++prepends;
    CHECK(prepends == Variant::AllDistricts().size());
    CHECK(Variant::AllDistricts().size() == 18);

    // If the mod also has the .inkatlas, the mod's version is rewritten instead of the vanilla one
    const auto modWithAtlas = std::vector<Archive::File>{
        RawFile(image, {Bytes("header"), Bytes("mod pixels")}),
        RawFile(atlas, {MakeCr2w(std::string("\0", 1) + image + std::string("\0mod atlas\0", 11), 0)}),
    };
    request.ad = &ad;
    request.provided = {&files[0], &files[2]};
    std::vector<uint64_t> askedVanilla;
    CHECK(Variant::Build(request, Reader(modWithAtlas), Reader(vanilla, &askedVanilla), result, error));
    for (auto hash : askedVanilla)
        CHECK(hash != Naming::PathHash(atlas));
    for (const auto& file : result.files)
    {
        if (file.hash == Naming::PathHash(newAtlas))
            CHECK(Text(file).find("mod atlas") != std::string::npos);
    }
    CHECK(result.restore.contains(Naming::PathHash(atlas))); // the .inkatlas replaced by the mod is also restored

    // Fails if a vanilla file is missing
    CHECK(!Variant::Build(request, Reader(mod), Reader({}), result, error));
}
// ---- Util ------------------------------------------------------------------------------------------------

void TestUtil()
{
    // Variable-length integers: 1 byte (0x1B = 27), signed (0x9B = -27, as used for string lengths), 2 bytes (6 + 7 bits)
    size_t pos = 0;
    CHECK(Util::ReadVlq({0x1B}, pos) == 27 && pos == 1);
    pos = 0;
    CHECK(Util::ReadVlq({0x9B}, pos) == -27 && pos == 1);
    pos = 0;
    CHECK(Util::ReadVlq({0x41, 0x02}, pos) == (1 | (2 << 6)) && pos == 2);

    // FNV-1a 64 without normalization (used for names with uppercase letters, such as type names)
    CHECK(Util::Fnv1a64("") == 0xCBF29CE484222325ull);
    CHECK(Util::Fnv1a64("array:TweakDBID") != Util::Fnv1a64("array:tweakdbid"));
    CHECK(Naming::PathHash("array:TweakDBID") == Util::Fnv1a64("array:tweakdbid"));

    CHECK(Util::Lower("AbC\\Z") == "abc\\z");
    CHECK(Util::EndsWith("a.inkwidget", ".inkwidget") && !Util::EndsWith("x", ".inkwidget"));
}

// ---- Report ----------------------------------------------------------------------------------------------

void TestReport()
{
    // Lines that fit are all kept, indented
    CHECK(Report::FitLines({"a", "b"}, 100) == "  a\n  b\n");

    // Lines that don't fit are counted in the last line
    std::vector<std::string> many(500, "archive\\pc\\mod\\Some_Ad_Replacer.archive: 12 ad(s)");
    const auto fitted = Report::FitLines(many, 1000);
    CHECK(fitted.size() <= 1000);
    CHECK(fitted.find("more\n") != std::string::npos);

    // A long line is cut without splitting a UTF-8 character (each "あ" is 3 bytes)
    std::string japanese;
    for (int i = 0; i < 200; ++i)
        japanese += "あ";
    const auto cut = Report::FitLines({japanese}, 1000);
    CHECK(cut.size() <= Report::LineLength + 3);
    const auto body = cut.substr(2, cut.size() - 2 - 4); // without the indent and "...\n"
    CHECK(body.size() % 3 == 0);

    // The game directory is hidden, ignoring case
    CHECK(Report::HidePath("read D:\\Games\\Cyberpunk 2077\\archive\\pc\\content\\basegame_1.archive: bad",
                           "d:\\games\\cyberpunk 2077") == "read <game>\\archive\\pc\\content\\basegame_1.archive: bad");
    CHECK(Report::HidePath("no path", "d:\\games") == "no path");

    // The summary and the whole report stay within the limits, even with many mods and problems
    std::vector<Report::ConvertedMod> mods(300, {"archive\\pc\\mod\\Some_Ad_Replacer.archive", 12});
    std::vector<std::string> problems(300, "error: archive\\pc\\mod\\Broken.archive: some_ad: unexpected data");
    const auto summary = Report::Summary("2026-11-23 01:00:00", mods, problems);
    CHECK(summary.size() <= Report::SummaryLength);
    CHECK(summary.find("Problems at that conversion:") != std::string::npos);

    Report::Start start;
    start.time = "2026-11-23 01:00:00";
    start.adcoexist = "1.1.0";
    start.game = "2.5.0";
    start.catalog = "2.50, matches the game";
    start.red4ext = "1.31.1";
    start.tweakXl = "1.12.2";
    start.modManager = "MO2";
    start.modArchives = 300;
    start.result = "mods unchanged, reused the last conversion";
    start.problems = problems;
    const auto report = Report::Compose(start, summary);
    CHECK(report.size() <= Report::MaxLength);
    CHECK(report.find("Installed with: MO2") != std::string::npos);
    CHECK(report.find("Last conversion: 2026-11-23") != std::string::npos);

    // A summary that is too long (changed by hand) is cut
    CHECK(Report::Compose(start, std::string(10000, 'x')).size() <= Report::MaxLength);

    // Without a summary and problems
    start.problems.clear();
    const auto plain = Report::Compose(start, "");
    CHECK(plain.find("Problems at this start: none") != std::string::npos);
    CHECK(plain.find("Last conversion: not available") != std::string::npos);
    CHECK(Report::Summary("t", {}, {}).find("Converted mods: none") != std::string::npos);
}
} // namespace

int main()
{
    TestCrc();
    TestArchiveRoundTrip();
    TestCr2wReplacePaths();
    TestCr2wRejects();
    TestPathHash();
    TestNaming();
    TestManifest();
    TestVariant();
    TestUtil();
    TestReport();
    if (g_failures == 0)
        std::printf("all tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
