#pragma once
#include <string>
#include <vector>

namespace phy104 {

/// One rule of the Study Hub: the formula and the sentence that explains when/why it holds.
struct StudyRule {
    std::string formula;       // "n1 sin th1 = n2 sin th2"
    std::string explain;       // "The product n.sin(theta) is the same on both sides of the surface."
};

/// A constant that belongs to a topic ("n water" = "1.333").
struct StudyConstant {
    std::string name;
    std::string value;
};

/// One chapter of the Study Hub, as it is written in data/study_hub.json (a JSON array: the order is the course order).
struct StudyTopic {
    std::string id;
    std::string title;
    std::string chapter;
    std::string summary;
    std::vector<StudyRule> rules;
    std::vector<StudyConstant> constants;
};

/// User-authored Study Hub notes; the summary is kept separately from the full note text.
struct StudyHubNote {
    std::string text;
    std::string summary;
    bool operator==(const StudyHubNote&) const = default;
};

} // namespace phy104
