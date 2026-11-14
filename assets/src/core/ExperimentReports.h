#pragma once
// كل ما تعرضه شاشات التجارب، كبيانات خام: سجلات LCD، ملاحظات، أخطاء الحقول، وسلاسل البيانات.
// لا يوجد Qt هنا - الصفحات ترسم فقط ما تعيده هذه الدوال.
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace phy104 {

/// سجل حساب واحد بنمط Casio.
struct LcdRegister {
    std::string title = "CALCULATION REGISTER";
    std::string mode = "DEG";           // يظهر كـ "● DEG" أو "● OK"
    bool resultFirst = false;           // القيمة الكبيرة تحت العنوان مباشرة (لسجلات الأخطاء)
    std::vector<std::string> lines;
    std::string resultLabel;            // تسمية صغيرة فوق القيمة الكبيرة ("" = لا يوجد)
    std::string resultValue;
    std::vector<std::string> extraLines;// سطور تحت القيمة الكبيرة
    std::string footer = "MEM: 01        PREC: 4 DP";
    bool isError = false;
};

/// ملاحظة توضيحية (بطاقة صفراء أو حمراء).
struct Notice {
    std::string title;
    std::string body;
    bool error = false;                 // بطاقة حمراء
};

/// مشكلة في حقل إدخال واحد (تظهر تحت الحقل باللون الأحمر).
struct FieldIssue {
    std::string field;                  // "d", "theta1", "f" ... (الصفحة تربطها بالودجت)
    std::string message;                // "Enter d > 0 mm."
    std::string label;                  // اسم قصير لـ "Restore d = 0.250 mm"
};

/// التقرير الكامل لتجربة واحدة: السجلات، الملاحظات، الأخطاء، والعنوان.
struct Report {
    bool valid = true;                  // false: لم يتم حساب أي شيء
    bool paused = false;                // true: إدخال خاطئ، الصفحة تعرض نمط التوقف
    std::string errorCode;              // "E_INPUT_RANGE", "E_FOCAL_PLANE", "E_EXIT_TIR" ...
    std::vector<LcdRegister> registers;
    std::vector<Notice> notices;
    std::vector<FieldIssue> issues;
    std::string headline;               // "θ₂ = 48.5904°"
    std::string footnote;
    std::string statusText;             // شريط الحالة: "Valid outcome: TIR"
    LcdRegister retained;               // ملخص مضغوط يستخدم كسجل "LAST VALID" لاحقاً
    std::vector<std::pair<std::string, std::string>> parameters;   // name -> "value unit"
};

/// جدول من الأرقام (NaN = "غير معرف"، مثل الانعكاس الكلي).
struct DataSeries {
    std::string id;
    std::string name;
    std::string description;            // "Glass → air · n₁ = 1.5000 · n₂ = 1.0000"
    std::string note;                   // "sin θ is dimensionless"
    std::vector<std::string> headers;
    std::vector<std::vector<double>> rows;
};

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// مدخلات تجربة الانكسار.
struct RefractionInput {
    double n1 = 1.5, n2 = 1.0, theta1 = 30.0;
    bool tirMode = false;               // true: شاشة TIR (الحالة الواحدة)
    std::string name1 = "Crown glass", name2 = "Air";
};

/// مدخلات تجربة العدسات والمرايا.
struct ImagingInput {
    double f = 10.0, s = 30.0, h = 3.0; // سم. f إشارة: + عدسة مقربة / مرآة مقعرة، - عدسة مبعدة / مرآة محدبة
    bool mirrorMode = false;
};

/// مدخلات تجربة المنشور.
struct PrismInput {
    double nPrism = 1.5, nMedium = 1.0, apex = 60.0, incidence = 48.5904, wavelengthNm = 589.3;
    bool dispersion = false;
};

/// مدخلات تجربة الشق المزدوج.
struct SlitInput {
    double wavelengthNm = 532.0, separationMm = 0.25, screenM = 1.5;
    int order = 2;
};

/// ما تعرفه الصفحة عن آخر نتيجة صحيحة (لنمط التوقف).
struct ReportContext {
    const Report* lastValid = nullptr;
    std::string runLabel;               // "RUN 021" أو ""
};

namespace ExperimentReports {

// دوال بناء التقارير لكل تجربة
Report refraction(const RefractionInput& in, const ReportContext& ctx = {});     // كلا الوضعين
Report imaging(const ImagingInput& in, const ReportContext& ctx = {});
Report prism(const PrismInput& in, const ReportContext& ctx = {});
Report doubleSlit(const SlitInput& in, const ReportContext& ctx = {});

/// زاوية المقارنة المستخدمة للحالة B عندما يتحكم المستخدم في الحالة A.
double defaultTirAngle(double n1, double n2);

// دوال توليد بيانات الرسوم البيانية (Sweeps)
DataSeries refractionSweep(const RefractionInput& in);      // θ₁ 0..35°, θ₂, sin θ₁, sin θ₂
DataSeries tirSweep(const RefractionInput& in);             // θ₁ 0..90°, θ₂ (فارغ بعد θc)
DataSeries imagingSweep(const ImagingInput& in);            // dₒ, dᵢ, m
DataSeries prismSweep(const PrismInput& in);                // i₁ 35..65°, δ
DataSeries doubleSlitSweep(const SlitInput& in);            // y (mm), I/I₀

/// نتيجة الانحدار الخطي (Linear Regression).
struct LinearFit { double slope = 0, intercept = 0, r2 = 0; bool ok = false; };
LinearFit linearFit(const std::vector<double>& x, const std::vector<double>& y);

/// وصف الحجم: "DIMINISHED (¼×)"، "ENLARGED (2.5×)"، "SAME SIZE (1×)"
std::string sizeDescription(double magnification);

} // namespace ExperimentReports
} // namespace phy104