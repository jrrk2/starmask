#include "ImageReader.h"

// PCL includes
#include <pcl/Image.h>
#include <pcl/XISF.h>
#include <pcl/String.h>
#include <pcl/Property.h>

// Try to include FITS from the modules directory
#ifdef __has_include
    #if __has_include(<pcl/modules/file-formats/FITS/FITS.h>)
        #include <pcl/modules/file-formats/FITS/FITS.h>
        #define HAS_PCL_FITS 1
    #endif
#endif

// Qt includes
#include <QDebug>
#include <QFileInfo>

class ImageReaderPrivate
{
public:
    ImageData imageData;
    QString lastError;
    
    bool readXISF(const QString& filePath) {
        try {
            pcl::XISFReader reader;
            pcl::String pclPath(filePath.toUtf8().constData());
            
            reader.Open(pclPath);
            
            if (reader.NumberOfImages() == 0) {
                lastError = "No images found in XISF file";
                return false;
            }
            
            // Read the first image
            pcl::Image pclImage;
            reader.ReadImage(pclImage);
            
            // Extract image dimensions and format info
            imageData.width = pclImage.Width();
            imageData.height = pclImage.Height();
            imageData.channels = pclImage.NumberOfChannels();
            imageData.format = "XISF";
            
            // Determine color space
            if (imageData.channels == 1) {
                imageData.colorSpace = "Grayscale";
            } else if (imageData.channels == 3) {
                imageData.colorSpace = "RGB";
            } else {
                imageData.colorSpace = QString("Multi-channel (%1)").arg(imageData.channels);
            }
            
            // Copy pixel data
            size_t totalPixels = static_cast<size_t>(imageData.width) * imageData.height * imageData.channels;
            imageData.pixels.resize(totalPixels);
            
            float* dst = imageData.pixels.data();
            for (int c = 0; c < imageData.channels; ++c) {
                for (int y = 0; y < imageData.height; ++y) {
                    for (int x = 0; x < imageData.width; ++x) {
                        *dst++ = static_cast<float>(pclImage.Pixel(x, y, c));
                    }
                }
            }
            
            // Extract metadata
            imageData.metadata.append(QString("Dimensions: %1 × %2 × %3")
                                    .arg(imageData.width).arg(imageData.height).arg(imageData.channels));
            
            // Get XISF-specific metadata (simplified approach)
            imageData.metadata.append(QString("Format: XISF"));
            
            // Try to read some common properties (simplified)
            try {
                pcl::PropertyArray properties = reader.ReadImageProperties();
                for (const auto& prop : properties) {
                    QString propName = QString::fromUtf8(prop.Id().c_str());
                    // Just show the property name for now, since the value access API varies
                    imageData.metadata.append(QString("Property: %1").arg(propName));
                }
            } catch (...) {
                // If property reading fails, just continue
                imageData.metadata.append("Properties: (unable to read)");
            }
            
            reader.Close();
            qDebug() << "Successfully read XISF file:" << filePath;
            return true;
            
        } catch (const pcl::Error& e) {
            lastError = QString("PCL XISF Error: %1").arg(e.Message().c_str());
            qDebug() << lastError;
            return false;
        } catch (const std::exception& e) {
            lastError = QString("XISF Error: %1").arg(e.what());
            qDebug() << lastError;
            return false;
        }
    }
    
    bool readFITS(const QString& filePath) {
#ifdef HAS_PCL_FITS
        try {
            pcl::FITSReader reader;
            pcl::String pclPath(filePath.toUtf8().constData());
            
            reader.Open(pclPath);
            
            // Read the primary image
            pcl::Image pclImage;
            reader.ReadImage(pclImage);
            
            // Extract image dimensions and format info
            imageData.width = pclImage.Width();
            imageData.height = pclImage.Height();
            imageData.channels = pclImage.NumberOfChannels();
            imageData.format = "FITS";
            
            // Determine color space
            if (imageData.channels == 1) {
                imageData.colorSpace = "Grayscale";
            } else if (imageData.channels == 3) {
                imageData.colorSpace = "RGB";
            } else {
                imageData.colorSpace = QString("Multi-channel (%1)").arg(imageData.channels);
            }
            
            // Copy pixel data
            size_t totalPixels = static_cast<size_t>(imageData.width) * imageData.height * imageData.channels;
            imageData.pixels.resize(totalPixels);
            
            float* dst = imageData.pixels.data();
            for (int c = 0; c < imageData.channels; ++c) {
                for (int y = 0; y < imageData.height; ++y) {
                    for (int x = 0; x < imageData.width; ++x) {
                        *dst++ = static_cast<float>(pclImage.Pixel(x, y, c));
                    }
                }
            }
            
            // Extract FITS metadata
            imageData.metadata.append(QString("Dimensions: %1 × %2 × %3")
                                    .arg(imageData.width).arg(imageData.height).arg(imageData.channels));
            
            // Read FITS keywords
            pcl::FITSKeywordArray keywords = reader.ReadFITSKeywords();
            for (const auto& keyword : keywords) {
                QString name = QString::fromUtf8(keyword.name.c_str());
                QString value = QString::fromUtf8(keyword.value.c_str());
                QString comment = QString::fromUtf8(keyword.comment.c_str());
                
                if (!name.isEmpty() && !value.isEmpty()) {
                    QString entry = QString("%1: %2").arg(name, value);
                    if (!comment.isEmpty()) {
                        entry += QString(" (%1)").arg(comment);
                    }
                    imageData.metadata.append(entry);
                }
            }
            
            reader.Close();
            qDebug() << "Successfully read FITS file:" << filePath;
            return true;
            
        } catch (const pcl::Error& e) {
            lastError = QString("PCL FITS Error: %1").arg(e.Message().c_str());
            qDebug() << lastError;
            return false;
        } catch (const std::exception& e) {
            lastError = QString("FITS Error: %1").arg(e.what());
            qDebug() << lastError;
            return false;
        }
#else
        // FITS support not available
        lastError = "FITS support not available in this PCL build";
        qDebug() << lastError;
        return false;
#endif
    }
};

// Public interface implementation
ImageReader::ImageReader()
    : d(std::make_unique<ImageReaderPrivate>())
{
}

ImageReader::~ImageReader() = default;

bool ImageReader::readFile(const QString& filePath)
{
    d->imageData.clear();
    d->lastError.clear();
    
    if (filePath.isEmpty()) {
        d->lastError = "Empty file path";
        return false;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        d->lastError = QString("File does not exist: %1").arg(filePath);
        return false;
    }
    
    QString fileType = detectFileType(filePath);
    qDebug() << "Detected file type:" << fileType << "for" << filePath;
    
    if (fileType == "XISF") {
        return d->readXISF(filePath);
    } else if (fileType == "FITS") {
        return d->readFITS(filePath);
    } else {
        d->lastError = QString("Unsupported file format: %1").arg(fileType);
        return false;
    }
}

const ImageData& ImageReader::imageData() const
{
    return d->imageData;
}

bool ImageReader::hasImage() const
{
    return d->imageData.isValid();
}

void ImageReader::clear()
{
    d->imageData.clear();
    d->lastError.clear();
}

QString ImageReader::lastError() const
{
    return d->lastError;
}

QStringList ImageReader::supportedFormats()
{
#ifdef HAS_PCL_FITS
    return {"XISF", "FITS"};
#else
    return {"XISF"};
#endif
}

QString ImageReader::formatFilter()
{
#ifdef HAS_PCL_FITS
    return "Image Files (*.xisf *.fits *.fit *.fts);;XISF Files (*.xisf);;FITS Files (*.fits *.fit *.fts);;All Files (*)";
#else
    return "Image Files (*.xisf);;XISF Files (*.xisf);;All Files (*)";
#endif
}

QString ImageReader::detectFileType(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    if (suffix == "xisf") {
        return "XISF";
    } else if (suffix == "fits" || suffix == "fit" || suffix == "fts") {
        return "FITS";  // Will return error when trying to read
    }
    
    // Try to detect by file content if extension is unclear
    // This could be enhanced with magic number detection
    return "Unknown";
}