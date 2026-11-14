#pragma once
#include "FormulaEngine.h"
#include "Models.h"
#include <map>
#include <string>
#include <vector>

namespace phy104 {

/// One page of search results.
struct FormulaPage {
    std::vector<const Formula*> items;
    int pageIndex = 0;           // 0-based, already clamped to a valid page
    int pageCount = 1;
    std::size_t total = 0;       // number of formulas before paging
};

/// Search, chapter filtering, chapter sorting and paging for the formula browser.
/// Pure C++ (no Qt), so the UI stays thin and everything here is unit tested.
class FormulaManager {
public:
    /// `engine` must outlive the manager (it owns the Formula objects we point to).
    FormulaManager(const FormulaEngine& engine, std::vector<Chapter> chapters);

    /// Chapters in course order.
    const std::vector<Chapter>& chapters() const { return m_chapters; }
    std::string chapterTitle(const std::string& chapterId) const;

    /// Case-insensitive search in formula NAME and ID ("snell law" finds id snell_law; all words
    /// must match). Empty query = everything. `chapterId` empty = all chapters.
    /// Result is sorted by chapter order, then by name.
    std::vector<const Formula*> search(const std::string& query, const std::string& chapterId = "") const;

    /// How many formulas of each chapter match `query` (for "Diffraction (6)" style labels).
    std::map<std::string, int> countByChapter(const std::string& query) const;

    /// Cuts `results` into pages of `pageSize` (minimum 1). pageIndex is clamped.
    static FormulaPage paginate(const std::vector<const Formula*>& results, int pageIndex, int pageSize);

    /// Lower-case, '_' -> ' ', single spaces. Used for matching.
    static std::string normalize(const std::string& text);

private:
    int chapterOrder(const std::string& chapterId) const;

    const FormulaEngine& m_engine;
    std::vector<Chapter> m_chapters;
};

} // namespace phy104
