#include "Variant.hpp"

#include "Cr2w.hpp"
#include "Naming.hpp"
#include "Oodle.hpp"

namespace
{
// Makes a rewritten copy: decompress, replace the paths, store uncompressed
bool Rewrite(const Archive::File& aSource, const std::vector<Cr2w::Replacement>& aReplacements, uint64_t aNewHash,
             Archive::File& aOut, std::string& aError)
{
    std::vector<uint8_t> main;
    if (aSource.segments.empty() ||
        !Oodle::DecompressSegment(aSource.segments[0].data, aSource.segments[0].size, main, aError))
    {
        if (aError.empty())
            aError = "no segments";
        return false;
    }
    std::vector<std::vector<uint8_t>> buffers;
    for (size_t i = 1; i < aSource.segments.size(); ++i)
    {
        std::vector<uint8_t> buffer;
        if (!Oodle::DecompressSegment(aSource.segments[i].data, aSource.segments[i].size, buffer, aError))
        {
            aError = "buffer " + std::to_string(i) + ": " + aError;
            return false;
        }
        buffers.push_back(std::move(buffer));
    }
    if (Cr2w::ReplacePaths(main, buffers, aReplacements, aError) < 0)
        return false;

    aOut = {};
    aOut.hash = aNewHash;
    aOut.inlineBuffers = aSource.inlineBuffers;
    aOut.segments.push_back({static_cast<uint32_t>(main.size()), std::move(main)});
    for (auto& buffer : buffers)
    {
        const auto size = static_cast<uint32_t>(buffer.size());
        aOut.segments.push_back({size, std::move(buffer)});
    }
    return true;
}
} // namespace

bool Variant::IsConvertible(AdCatalog::FileKind aKind)
{
    return aKind == AdCatalog::FileKind::Image || aKind == AdCatalog::FileKind::Atlas ||
           aKind == AdCatalog::FileKind::Widget;
}

Variant::Decision Variant::Decide(const std::vector<const AdCatalog::File*>& aProvided, const FileReader& aReadMod,
                                  const std::function<bool(uint64_t)>& aIsAdded)
{
    std::unordered_set<uint64_t> hashes;
    for (const auto* file : aProvided)
    {
        if (file->kind == AdCatalog::FileKind::Image)
            return {true, false, "replaces an image"};
        hashes.insert(Naming::PathHash(file->path));
    }

    std::unordered_map<uint64_t, Archive::File> files;
    if (!aReadMod(hashes, files))
        return {true, true, "cannot read the mod's files"};
    for (const auto* file : aProvided)
    {
        const auto it = files.find(Naming::PathHash(file->path));
        std::vector<uint8_t> main;
        std::vector<std::string> imports;
        std::string error;
        const bool read = it != files.end() && !it->second.segments.empty() &&
                          Oodle::DecompressSegment(it->second.segments[0].data, it->second.segments[0].size, main,
                                                   error) &&
                          Cr2w::ReadImports(main, imports, error);
        if (!read)
            return {true, true, std::string("cannot read ") + file->path + (error.empty() ? "" : ": " + error)};
        for (const auto& path : imports)
        {
            if (!path.empty() && aIsAdded(Naming::PathHash(path)))
                return {true, false, "references " + path + ", which a mod adds"};
        }
    }
    return {false, false, "only changes .inkatlas or .inkwidget files that use the game's own pictures"};
}

std::vector<bool> Variant::ChooseOwners(const std::vector<const AdCatalog::Ad*>& aAds)
{
    std::unordered_set<const AdCatalog::Ad*> owned;
    std::vector<bool> owners;
    for (const auto* ad : aAds)
        owners.push_back(owned.insert(ad).second);
    return owners;
}

void Variant::KeepOwnerFiles(const std::vector<const AdCatalog::File*>& aProvided,
                             std::unordered_set<uint64_t>& aRestore)
{
    for (const auto* file : aProvided)
        aRestore.erase(Naming::PathHash(file->path));
}

const std::vector<const char*>& Variant::AllDistricts()
{
    static const std::vector<const char*> districts = {
        "Adverts.AllAdvertisements.advertisements", "Adverts.ArasakaWF.advertisements",
        "Adverts.BadLand.advertisements",           "Adverts.CharterHill.advertisements",
        "Adverts.CityCenter.advertisements",        "Adverts.Glen.advertisements",
        "Adverts.JapanTown.advertisements",         "Adverts.JapanTownCold.advertisements",
        "Adverts.Kabuki.advertisements",            "Adverts.LittleChina.advertisements",
        "Adverts.NorthIndustrial.advertisements",   "Adverts.NorthOak.advertisements",
        "Adverts.PacificaCV_WWE.advertisements",    "Adverts.Rancho.advertisements",
        "Adverts.SantoDomingo.advertisements",      "Adverts.VistaDelRay.advertisements",
        "Adverts.WellSpring.advertisements",        "Adverts.Spaceport.advertisements",
    };
    return districts;
}

bool Variant::Build(const Request& aRequest, const FileReader& aReadMod, const FileReader& aReadVanilla,
                    Result& aResult, std::string& aError)
{
    const auto& ad = *aRequest.ad;
    aResult = {};

    // Original path -> new path: the mod's files, and the .inkatlas and .inkwidget files that are copied and rewritten
    std::vector<Cr2w::Replacement> replacements;
    std::unordered_map<uint64_t, std::string> newPathOf;
    auto addMapping = [&](const char* aPath) {
        const auto hash = Naming::PathHash(aPath);
        if (newPathOf.contains(hash))
            return true;
        auto newPath = Naming::NewPath(aPath, aRequest.archiveId);
        if (newPath.empty())
        {
            aError = std::string("no path rule for ") + aPath;
            return false;
        }
        replacements.emplace_back(aPath, newPath);
        newPathOf[hash] = std::move(newPath);
        return true;
    };

    std::unordered_set<uint64_t> providedHashes;
    for (const auto* file : aRequest.provided)
    {
        if (!addMapping(file->path))
            return false;
        providedHashes.insert(Naming::PathHash(file->path));
    }
    std::unordered_set<uint64_t> rewriteHashes;
    for (uint16_t f = 0; f < ad.fileCount; ++f)
    {
        const auto kind = ad.files[f].kind;
        if (kind != AdCatalog::FileKind::Atlas && kind != AdCatalog::FileKind::Widget)
            continue;
        if (!addMapping(ad.files[f].path))
            return false;
        rewriteHashes.insert(Naming::PathHash(ad.files[f].path));
    }

    // The mod's files, and the vanilla .inkatlas and .inkwidget files the mod doesn't have
    std::unordered_map<uint64_t, Archive::File> sources;
    if (!aReadMod(providedHashes, sources))
    {
        aError = "cannot read the mod's files";
        return false;
    }
    std::unordered_set<uint64_t> fromVanilla;
    for (auto hash : rewriteHashes)
    {
        if (!sources.contains(hash))
            fromVanilla.insert(hash);
    }
    if (!aReadVanilla(fromVanilla, sources))
    {
        aError = std::string("vanilla files of ") + ad.folder + " not found";
        return false;
    }

    // Images go to the new path as they are. .inkatlas and .inkwidget files are rewritten
    for (auto& [hash, file] : sources)
    {
        const auto& newPath = newPathOf[hash];
        if (rewriteHashes.contains(hash))
        {
            Archive::File rewritten;
            if (!Rewrite(file, replacements, Naming::PathHash(newPath), rewritten, aError))
            {
                aError = newPath + ": " + aError;
                return false;
            }
            aResult.files.push_back(std::move(rewritten));
        }
        else
        {
            file.hash = Naming::PathHash(newPath);
            aResult.files.push_back(std::move(file));
        }
    }

    // TweakDB: records in districts go to the same districts as vanilla. If no record is in a district, all districts
    bool anyDistrict = false;
    for (uint16_t r = 0; r < ad.recordCount; ++r)
        anyDistrict |= ad.records[r].districtCount > 0;
    for (uint16_t r = 0; r < ad.recordCount; ++r)
    {
        const auto& record = ad.records[r];
        if (anyDistrict && record.districtCount == 0)
            continue; // a record without districts in an ad that has other records in districts
        const auto name = Naming::VariantRecordName(aRequest.recordStem, record.name);
        aResult.yaml += name + ":\n";
        aResult.yaml += "  $base: " + std::string(record.name) + "\n";
        aResult.yaml += "  resource: " + newPathOf[Naming::PathHash(record.widget)] + "\n";
        const auto& all = AllDistricts();
        const size_t count = anyDistrict ? record.districtCount : all.size();
        for (size_t d = 0; d < count; ++d)
        {
            aResult.yaml += std::string(anyDistrict ? record.districts[d] : all[d]) + ":\n";
            aResult.yaml += "  - !prepend " + name + "\n";
        }
        aResult.recordNames.push_back(name);
    }

    aResult.restore = std::move(providedHashes);
    return true;
}
