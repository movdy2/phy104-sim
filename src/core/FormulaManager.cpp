#include "FormulaManager.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

namespace phy104 {

FormulaManager::FormulaManager(const FormulaEngine& engine, std::vector<Chapter> chapters)
    : m_engine(engine), m_chapters(std::move(chapters)) {
    std::stable_sort(m_chapters.begin(), m_chapters.end(),
                     [](const Chapter& a, const Chapter& b) { return a.order < b.order; });
}

std::string FormulaManager::normalize(const std::string& text) {
    std::string out;
    bool lastSpace = true;
    for (unsigned char ch : text) {
        const char c = (ch == '_' || ch == '-') ? ' ' : static_cast<char>(std::tolower(ch));
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!lastSpace) out += ' ';
            lastSpace = true;
        } else {
            out += c;
            lastSpace = false;
        }
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

int FormulaManager::chapterOrder(const std::string& chapterId) const {
    for (const auto& c : m_chapters)
        if (c.id == chapterId) return c.order;
    return std::numeric_limits<int>::max();            // unknown chapters go last
}

std::string FormulaManager::chapterTitle(const std::string& chapterId) const {
    for (const auto& c : m_chapters)
        if (c.id == chapterId) return c.title;
    return chapterId;
}

std::vector<const Formula*> FormulaManager::search(const std::string& query,
                                                   const std::string& chapterId) const {
    std::vector<std::string> words;
    {
        std::istringstream in(normalize(query));
        for (std::string w; in >> w;) words.push_back(w);
    }

    std::vector<const Formula*> result;
    for (const auto& f : m_engine.formulas()) {
        if (!chapterId.empty() && f.chapter != chapterId) continue;
        const std::string haystack = normalize(f.name) + " " + normalize(f.id);
        const bool all = std::all_of(words.begin(), words.end(), [&](const std::string& w) {
            return haystack.find(w) != std::string::npos;
        });
        if (all) result.push_back(&f);
    }
    std::stable_sort(result.begin(), result.end(), [&](const Formula* a, const Formula* b) {
        const int oa = chapterOrder(a->chapter), ob = chapterOrder(b->chapter);
        if (oa != ob) return oa < ob;
        return normalize(a->name) < normalize(b->name);
    });
    return result;
}

std::map<std::string, int> FormulaManager::countByChapter(const std::string& query) const {
    std::map<std::string, int> counts;
    for (const Formula* f : search(query)) ++counts[f->chapter];
    return counts;
}

FormulaPage FormulaManager::paginate(const std::vector<const Formula*>& results, int pageIndex, int pageSize) {
    FormulaPage page;
    pageSize = std::max(1, pageSize);
    page.total = results.size();
    page.pageCount = std::max(1, static_cast<int>((results.size() + pageSize - 1) / pageSize));
    page.pageIndex = std::clamp(pageIndex, 0, page.pageCount - 1);
    const std::size_t begin = static_cast<std::size_t>(page.pageIndex) * pageSize;
    const std::size_t end = std::min(results.size(), begin + pageSize);
    for (std::size_t i = begin; i < end; ++i) page.items.push_back(results[i]);
    return page;
}

} // namespace phy104
