// What every registry shares (ContentRegistry.hpp): sources and problems.

#include "ContentRegistry.hpp"

#include <format>

namespace Runic::Content {

namespace {

constexpr std::string_view kFileLineFormat = "{}:{}";
constexpr std::string_view kUnknownSource = "an unknown source";

} // namespace

std::string ContentSource::ToText() const {
    if (!file.empty()) {
        return line == kNoLine ? file
                               : std::vformat(kFileLineFormat, std::make_format_args(file, line));
    }
    return origin.empty() ? std::string(kUnknownSource) : origin;
}

ContentSource FromFile(std::string_view file, std::uint32_t line) {
    ContentSource source;
    source.file = std::string(file);
    source.line = line;
    return source;
}

ContentSource FromCode(std::string_view origin) {
    ContentSource source;
    source.origin = std::string(origin);
    return source;
}

namespace Detail {

void RegistryBase::ReportDuplicate(const ContentId &id, const ContentSource &second,
                                   const ContentSource &first) {
    ContentProblem problem;
    problem.kind = ContentProblemKind::Duplicate;
    problem.id = std::string(id.Text());
    problem.source = second;
    problem.firstSource = first;
    problem.message = std::format("{} {} is defined twice: at {}, and first at {} (kept)", m_Kind,
                                  id.Text(), second.ToText(), first.ToText());
    m_Problems.push_back(std::move(problem));
}

void RegistryBase::ReportUnknown(std::string_view id, const ContentSource &from) {
    ContentProblem problem;
    problem.kind = ContentProblemKind::Unknown;
    problem.id = std::string(id);
    problem.source = from;
    problem.message =
        std::format("{} names {} {}, which nothing defines", from.ToText(), m_Kind, id);
    m_Problems.push_back(std::move(problem));
}

void RegistryBase::ReportMalformed(std::string_view text, const ContentSource &from,
                                   const Core::Error &why) {
    ContentProblem problem;
    problem.kind = ContentProblemKind::Malformed;
    problem.id = std::string(text);
    problem.source = from;
    problem.message = std::format("{} names {} by {}", from.ToText(), m_Kind, why.message);
    m_Problems.push_back(std::move(problem));
}

} // namespace Detail

} // namespace Runic::Content
