#pragma once
#include "FormulaEngine.h"
#include "Models.h"
#include <map>
#include <string>
#include <vector>

namespace phy104 {

/// صفحة واحدة من نتائج البحث.
struct FormulaPage {
    std::vector<const Formula*> items;
    int pageIndex = 0;           // يبدأ من 0، ومقيد بصفحة صالحة
    int pageCount = 1;
    std::size_t total = 0;       // عدد المعادلات قبل التقسيم
};

/// إدارة البحث، تصفية الفصول، ترتيب الفصول، والتقسيم إلى صفحات لمتصفح المعادلات.
/// C++ خالص (بدون Qt)، لذا تبقى الواجهة بسيطة وكل شيء هنا قابل للاختبار.
class FormulaManager {
public:
    /// `engine` يجب أن يبقى حياً طالما المدير موجود (لأنه يملك كائنات Formula التي نشير إليها).
    FormulaManager(const FormulaEngine& engine, std::vector<Chapter> chapters);

    /// الفصول مرتبة حسب ترتيب الدورة.
    const std::vector<Chapter>& chapters() const { return m_chapters; }
    std::string chapterTitle(const std::string& chapterId) const;

    /// بحث غير حساس لحالة الأحرف في اسم المعادلة ومعرفها ("snell law" يجد id snell_law؛ كل الكلمات يجب أن تطابق).
    /// استعلام فارغ = كل شيء. `chapterId` فارغ = كل الفصول.
    /// النتيجة مرتبة حسب ترتيب الفصل ثم حسب الاسم.
    std::vector<const Formula*> search(const std::string& query, const std::string& chapterId = "") const;

    /// عدد المعادلات في كل فصل تطابق `query` (للتسميات مثل "Diffraction (6)").
    std::map<std::string, int> countByChapter(const std::string& query) const;

    /// تقسم `results` إلى صفحات بحجم `pageSize` (الحد الأدنى 1). يتم تقييد `pageIndex`.
    static FormulaPage paginate(const std::vector<const Formula*>& results, int pageIndex, int pageSize);

    /// تحويل إلى أحرف صغيرة، '_' -> ' '، مسافات فردية. تُستخدم للمطابقة.
    static std::string normalize(const std::string& text);

private:
    int chapterOrder(const std::string& chapterId) const;

    const FormulaEngine& m_engine;
    std::vector<Chapter> m_chapters;
};

} // namespace phy104