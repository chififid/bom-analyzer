#pragma once

#include <QString>

#include <vector>

namespace bom {

enum class ItemId : quint64 { Invalid = 0 };

struct ProfileItem
{
    QString profileName;
    double lengthMm{}; // Per-piece length.
    double cost{};     // Price of one whole piece.
    int quantity{};
    ItemId id{ItemId::Invalid}; // Unique within a document; preserved when copying or editing.
};

struct Bom
{
    QString buildingId;
    std::vector<ProfileItem> items;
};

struct BomDocument
{
    std::vector<Bom> boms;
};

}
