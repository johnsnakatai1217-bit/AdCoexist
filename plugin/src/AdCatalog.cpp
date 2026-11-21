#include "AdCatalog.hpp"

namespace
{
#include "generated/AdCatalog.inc"
} // namespace

const AdCatalog::Catalog& AdCatalog::Get()
{
    return CatalogData;
}
