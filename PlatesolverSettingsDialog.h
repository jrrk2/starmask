// PlatesolverSettingsDialog.h
#ifndef PLATESOLVER_SETTINGS_DIALOG_H
#define PLATESOLVER_SETTINGS_DIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QDialogButtonBox>

class PlatesolverSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlatesolverSettingsDialog(QWidget* parent = nullptr);

    // Getters
    QString getAstrometryPath() const { return m_astrometryPathEdit->text(); }
    QString getIndexPath() const { return m_indexPathEdit->text(); }
    QString getConfigFile() const { return m_configFileEdit->text(); }
    double getMinScale() const { return m_minScaleSpinBox->value(); }
    double getMaxScale() const { return m_maxScaleSpinBox->value(); }
    int getTimeout() const { return m_timeoutSpinBox->value(); }
    int getMaxStars() const { return m_maxStarsSpinBox->value(); }
    double getLogOddsThreshold() const { return m_logOddsSpinBox->value(); }
    bool getVerbose() const { return m_verboseCheckBox->isChecked(); }

    // Setters  
    void setAstrometryPath(const QString& path) { m_astrometryPathEdit->setText(path); }
    void setIndexPath(const QString& path) { m_indexPathEdit->setText(path); }
    void setConfigFile(const QString& path) { m_configFileEdit->setText(path); }
    void setScaleRange(double min, double max) {
        m_minScaleSpinBox->setValue(min);
        m_maxScaleSpinBox->setValue(max);
    }
    void setTimeout(int seconds) { m_timeoutSpinBox->setValue(seconds); }
    void setMaxStars(int count) { m_maxStarsSpinBox->setValue(count); }
    void setLogOddsThreshold(double threshold) { m_logOddsSpinBox->setValue(threshold); }
    void setVerbose(bool verbose) { m_verboseCheckBox->setChecked(verbose); }

private slots:
    void browseIndexPath();
    void browseConfigFile();
    void resetToDefaults();

private:
    void setupUI();
    
    // UI elements
    QLineEdit* m_astrometryPathEdit;
    QLineEdit* m_indexPathEdit;
    QPushButton* m_indexPathBrowseButton;
    QLineEdit* m_configFileEdit;
    QPushButton* m_configFileBrowseButton;
    QDoubleSpinBox* m_minScaleSpinBox;
    QDoubleSpinBox* m_maxScaleSpinBox;
    QSpinBox* m_timeoutSpinBox;
    QSpinBox* m_maxStarsSpinBox;
    QDoubleSpinBox* m_logOddsSpinBox;
    QCheckBox* m_verboseCheckBox;
    QDialogButtonBox* m_buttonBox;
};

#endif // PLATESOLVER_SETTINGS_DIALOG_H
