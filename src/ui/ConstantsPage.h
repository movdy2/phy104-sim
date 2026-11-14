#pragma once
#include "Repositories.h"
#include <QWidget>
#include <vector>

class QLabel;
class QLineEdit;
class QTableWidget;

namespace phy104 {

/// Page 4: physical constants with search, description and "copy" button.
class ConstantsPage : public QWidget {
    Q_OBJECT
public:
    explicit ConstantsPage(const IConstantsRepository& repository, QWidget* parent = nullptr);

private slots:
    void applyFilter(const QString& text);
    void onSelectionChanged();
    void copySelected();

private:
    int selectedIndex() const;

    std::vector<PhysicsConstant> m_constants;
    QLineEdit* m_search;
    QTableWidget* m_table;
    QLabel* m_description;
};

} // namespace phy104
