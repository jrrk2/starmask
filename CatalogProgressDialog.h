#include <QDebug>
#include <QString>

// Add a progress dialog for large catalog operations
class CatalogProgressDialog : public QProgressDialog
{
public:
    CatalogProgressDialog(QWidget* parent = nullptr) 
        : QProgressDialog("Loading catalog...", "Cancel", 0, 0, parent)
    {
        setWindowTitle("Catalog Query");
        setWindowModality(Qt::WindowModal);
        setMinimumDuration(500); // Show after 500ms
    }
    
    void updateProgress(const QString& status, int value = 0, int maximum = 0) {
        setLabelText(status);
        if (maximum > 0) {
            setMaximum(maximum);
            setValue(value);
        }
        QApplication::processEvents();
    }
};
