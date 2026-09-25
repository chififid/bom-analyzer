#include "io/bom_csv_reader.h"
#include "domain/bom_validation.h"

#include <QFile>
#include <QHash>
#include <QStringConverter>
#include <QStringList>

#include <utility>

namespace bom {
namespace {

enum class CsvColumn { BuildingId, ProfileName, LengthMm, Cost, Quantity, Count };

constexpr qsizetype columnIndex(CsvColumn column)
{
    return static_cast<qsizetype>(column);
}

QString columnName(CsvColumn column)
{
    switch (column) {
    case CsvColumn::BuildingId: return QString::fromLatin1(bom::fields::buildingId);
    case CsvColumn::ProfileName: return QString::fromLatin1(bom::fields::profileName);
    case CsvColumn::LengthMm: return QString::fromLatin1(bom::fields::lengthMm.name);
    case CsvColumn::Cost: return QString::fromLatin1(bom::fields::cost.name);
    case CsvColumn::Quantity: return QString::fromLatin1(bom::fields::quantity.name);
    case CsvColumn::Count: break;
    }
    return {};
}

const QStringList expectedHeader = [] {
    QStringList names;
    for (qsizetype index = 0; index < columnIndex(CsvColumn::Count); ++index) {
        names.push_back(columnName(static_cast<CsvColumn>(index)));
    }
    return names;
}();

Error fieldError(ErrorCode code, qsizetype line, qsizetype columnIndex, const QString& message)
{
    return {code, line,
        columnIndex < expectedHeader.size() ? expectedHeader[columnIndex] : QString{}, message};
}

// Input newlines must be normalized to LF; quoted fields may span lines.
Result<QStringList> readRecord(
    const QString& text, qsizetype& offset, qsizetype& line)
{
    enum class State { Start, Unquoted, Quoted, AfterQuote };
    State state = State::Start;
    const qsizetype startLine = line;
    QStringList fields;
    QString field;

    while (offset < text.size()) {
        const QChar character = text[offset++];
        if (state == State::Quoted) {
            if (character == u'"') {
                if (offset < text.size() && text[offset] == u'"') {
                    // A doubled quote belongs to the value, not the field boundary.
                    field += u'"';
                    ++offset;
                } else {
                    state = State::AfterQuote;
                }
            } else {
                field += character;
                if (character == u'\n') {
                    ++line;
                }
            }
        } else if (character == u',' || character == u'\n') {
            fields.push_back(field);
            field.clear();
            state = State::Start;
            if (character == u'\n') {
                ++line;
                return fields;
            }
        } else if (state == State::AfterQuote) {
            return fieldError(ErrorCode::InvalidCsv, startLine, fields.size(),
                QStringLiteral("Expected a comma or record end after the closing quote."));
        } else if (character == u'"') {
            if (state != State::Start) {
                return fieldError(ErrorCode::InvalidCsv, startLine, fields.size(),
                    QStringLiteral("A quoted field must start with a quote; escape internal quotes as \"\"."));
            }
            state = State::Quoted;
        } else {
            field += character;
            state = State::Unquoted;
        }
    }

    if (state == State::Quoted) {
        return fieldError(ErrorCode::InvalidCsv, startLine, fields.size(),
            QStringLiteral("The quoted field is not closed."));
    }
    fields.push_back(field);
    return fields;
}

Result<ProfileItem> readItem(const QStringList& fields, qsizetype line)
{
    ProfileItem item;
    item.profileName = fields[columnIndex(CsvColumn::ProfileName)];
    bool valid = false;
    item.lengthMm = fields[columnIndex(CsvColumn::LengthMm)].toDouble(&valid);
    if (!valid) {
        return fieldError(ErrorCode::InvalidData, line, columnIndex(CsvColumn::LengthMm),
            QStringLiteral("%1 must be a number within the double range. Use a decimal point.")
                .arg(columnName(CsvColumn::LengthMm)));
    }

    item.cost = fields[columnIndex(CsvColumn::Cost)].toDouble(&valid);
    if (!valid) {
        return fieldError(ErrorCode::InvalidData, line, columnIndex(CsvColumn::Cost),
            QStringLiteral("%1 must be a number within the double range. Use a decimal point.")
                .arg(columnName(CsvColumn::Cost)));
    }

    item.quantity = fields[columnIndex(CsvColumn::Quantity)].toInt(&valid, 10);
    if (!valid) {
        return fieldError(ErrorCode::InvalidData, line, columnIndex(CsvColumn::Quantity),
            QStringLiteral("%1 must be an integer within the int range.").arg(columnName(CsvColumn::Quantity)));
    }
    auto validation = validateItem(item);
    if (auto* error = std::get_if<Error>(&validation)) {
        error->line = line;
        return *error;
    }
    return item;
}

}

Result<BomDocument> parseBomCsv(const QByteArray& data)
{
    // Stateless reports an incomplete UTF-8 sequence at the end of the file too.
    QStringDecoder decoder(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    QString text = decoder(data);
    if (decoder.hasError()) {
        return Error{ErrorCode::InvalidEncoding, 0, {}, QStringLiteral("The file must contain valid UTF-8 text.")};
    }
    if (text.contains(QChar::Null)) {
        return Error{ErrorCode::InvalidEncoding, 0, {}, QStringLiteral("The CSV contains a NUL character. Check its UTF-8 encoding.")};
    }
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(u'\r', u'\n');
    if (text.isEmpty()) {
        return Error{ErrorCode::InvalidCsv, 1, {}, QStringLiteral("The CSV header is missing.")};
    }

    qsizetype offset = 0;
    qsizetype line = 1;
    const auto header = readRecord(text, offset, line);
    if (const auto* error = std::get_if<Error>(&header)) {
        return *error;
    }
    const auto& headerFields = std::get<QStringList>(header);
    if (headerFields != expectedHeader) {
        return Error{ErrorCode::InvalidCsv, 1, {},
            QStringLiteral("Expected the header %1 in this order.").arg(expectedHeader.join(u','))};
    }

    BomDocument document;
    QHash<QString, std::size_t> buildingIndices;
    quint64 nextItemId = 1;
    while (offset < text.size()) {
        const qsizetype startLine = line;
        const auto record = readRecord(text, offset, line);
        if (const auto* error = std::get_if<Error>(&record)) {
            return *error;
        }
        const auto& fields = std::get<QStringList>(record);
        if (fields.size() != expectedHeader.size()) {
            return Error{ErrorCode::InvalidCsv, startLine, {},
                QStringLiteral("Expected %1 fields, got %2.")
                    .arg(expectedHeader.size()).arg(fields.size())};
        }
        const QString& buildingId = fields[columnIndex(CsvColumn::BuildingId)];
        auto validation = validateBuildingId(buildingId);
        if (auto* error = std::get_if<Error>(&validation)) {
            error->line = startLine;
            return *error;
        }
        auto parsedItem = readItem(fields, startLine);
        if (const auto* error = std::get_if<Error>(&parsedItem)) {
            return *error;
        }

        const auto existing = buildingIndices.constFind(buildingId);
        std::size_t buildingIndex;
        if (existing == buildingIndices.cend()) {
            buildingIndex = document.boms.size();
            buildingIndices.insert(buildingId, buildingIndex);
            document.boms.push_back(Bom{buildingId, {}});
        } else {
            buildingIndex = existing.value();
        }
        auto& item = std::get<ProfileItem>(parsedItem);
        item.id = ItemId{nextItemId++};
        document.boms[buildingIndex].items.push_back(std::move(item));
    }
    return document;
}

Result<BomDocument> readBomCsv(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return Error{ErrorCode::FileAccess, 0, {},
            QStringLiteral("Could not open the file for reading: %1").arg(filePath)};
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return Error{ErrorCode::FileAccess, 0, {},
            QStringLiteral("Could not read the file: %1").arg(filePath)};
    }
    return parseBomCsv(data);
}

}
