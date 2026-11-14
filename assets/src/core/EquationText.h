#pragma once
#include <string>

namespace phy104::EquationText {

/// LaTeX -> plain math the ExpressionEvaluator understands.
///   n_1 \sin\theta_1 = n_2 \sin\theta_2   ->   n_1*sin(theta_1)=n_2*sin(theta_2)
///   \frac{1}{s}+\frac{1}{s'}=\frac{1}{f}  ->   ((1)/(s))+((1)/(sp))=((1)/(f))
/// Supports \frac \sqrt \left \right \cdot \times \pi, Greek letters, \sin \cos \tan ..., ^ _ and implicit
/// multiplication. Throws std::runtime_error with a readable message for anything else.
std::string latexToExpression(const std::string& latex);

/// Plain math -> LaTeX for the live preview.
///   n1*sin(theta1)=n2*sin(theta2)   ->   n_{1}\sin\left(\theta_{1}\right)=n_{2}\sin\left(\theta_{2}\right)
/// Division becomes \frac, sqrt becomes \sqrt, Greek names become commands. Throws on syntax errors.
std::string expressionToLatex(const std::string& expression);

} // namespace phy104::EquationText
