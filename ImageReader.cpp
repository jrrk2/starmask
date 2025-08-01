#include "ImageReader.h"

// Initialize mock PCL API before including PCL headers
#include "PCLMockAPI.h"

// PCL includes
#include <pcl/Image.h>
#include <pcl/XISF.h>
#include <pcl/String.h>
#include <pcl/Property.h>
#include <pcl/api/APIInterface.h>

// FITS support - we confirmed this works
#include <FITS/FITS.h>
#define HAS_PCL_FITS 1
#include <TIFF/TIFF.h>
#define HAS_PCL_TIFF 1

// Qt includes
#include <QDebug>
#include <QFileInfo>

class ImageReaderPrivate
{
public:
    ImageData imageData;
    QString lastError;
    bool mockInitialized = false;
    
    void initializePCLMock() {
        if (!mockInitialized) {
            qDebug() << "Initializing PCL Mock API...";
            
            // Initialize the mock API
            pcl_mock::SetDebugLogging(false); // Set to true for debug output
            pcl_mock::InitializeMockAPI();
            
            // Create API interface with mock function resolver
            if (!API) {
                API = new pcl::APIInterface(pcl_mock::GetMockFunctionResolver());
            }
            
            mockInitialized = true;
            qDebug() << "PCL Mock API initialized successfully";
        }
    }
    
    bool readXISF(const QString& filePath) {
        initializePCLMock();
        
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
	    //	    reader.Setindex(0);
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

    bool readTIFF(const QString& filePath)
    {
	try {
	    pcl::TIFFReader reader;
	    reader.Open(pcl::String(filePath.toStdString().c_str()));

	    pcl::Image pclImage;
	    reader.ReadImage(pclImage);

	    int width = pclImage.Width();
	    int height = pclImage.Height();
	    int channels = pclImage.NumberOfChannels();

	    imageData.width = width;
	    imageData.height = height;
	    imageData.channels = channels;
	    imageData.pixels.resize(width * height * channels);

	    for (int c = 0; c < channels; ++c) {
		const float* plane = pclImage.PixelData(c);
		for (int i = 0; i < width * height; ++i) {
		    imageData.pixels[i + c * width * height] = plane[i];
		}
	    }

	    return true;

	} catch (const pcl::Exception& e) {
	    qWarning() << "TIFF load failed:" << e.Message().c_str();
	    return false;
	}
    }

    bool readFITS(const QString& filePath) {
        initializePCLMock();
        
        try {
            qDebug() << "Attempting to read FITS file:" << filePath;
            
            pcl::FITSReader reader;
	    ((pcl::FITSReader &)reader).SetIndex(0);
            pcl::String pclPath(filePath.toUtf8().constData());
            
            qDebug() << "Opening FITS file...";
            reader.Open(pclPath);
            
            qDebug() << "FITS file opened successfully, reading image...";
            
            // Try reading image with different approaches if first fails
            pcl::Image pclImage;
            try {
                reader.ReadImage(pclImage);
                qDebug() << "Image read successfully with standard method";
            } catch (const pcl::Error& e) {
                qDebug() << "Standard read failed, trying alternative approach:" << e.Message().c_str();
                
                // Try to read without WCS processing
                try {
                    reader.Close();
                    reader.Open(pclPath);  // Reopen
                    
                    // Create image manually if PCL's ReadImage fails due to WCS issues
                    // This is a workaround for files with corrupted WCS keywords
                    throw e;  // For now, just re-throw to see the exact error
                } catch (...) {
                    throw;  // Re-throw the original error
                }
            }
            
            qDebug() << "Image read successfully, dimensions:" << pclImage.Width() << "x" << pclImage.Height();
            
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
            
            qDebug() << "Copying pixel data for" << totalPixels << "pixels...";
            
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
            
            // Read FITS keywords with better error handling
            try {
                qDebug() << "Reading FITS keywords...";
                pcl::FITSKeywordArray keywords = reader.ReadFITSKeywords();
                qDebug() << "Found" << keywords.Length() << "FITS keywords";
                
                for (const auto& keyword : keywords) {
                    QString name = QString::fromUtf8(keyword.name.c_str());
                    QString value = QString::fromUtf8(keyword.value.c_str());
                    QString comment = QString::fromUtf8(keyword.comment.c_str());
                    
                    if (!name.isEmpty()) {
                        QString entry = QString("%1: %2").arg(name, value.isEmpty() ? "(empty)" : value);
                        if (!comment.isEmpty()) {
                            entry += QString(" (%1)").arg(comment);
                        }
                        imageData.metadata.append(entry);
                    }
                }
            } catch (const pcl::Error& e) {
                qDebug() << "Error reading FITS keywords:" << e.Message().c_str();
                imageData.metadata.append(QString("FITS Keywords: Error - %1").arg(e.Message().c_str()));
            } catch (...) {
                qDebug() << "Unknown error reading FITS keywords";
                imageData.metadata.append("FITS Keywords: (unable to read - unknown error)");
            }
            
            reader.Close();
            qDebug() << "Successfully read FITS file:" << filePath;
            return true;
            
        } catch (const pcl::Error& e) {
            lastError = QString("PCL FITS Error: %1\n\nThis FITS file may have corrupted WCS keywords or unsupported format variations.\nThe file structure appears valid but PCL cannot process it.\n\nNote: Using mock PCL API - some features may be limited.").arg(e.Message().c_str());
            qDebug() << "PCL FITS Error:" << e.Message().c_str();
            qDebug() << "File path:" << filePath;
            return false;
        } catch (const std::exception& e) {
            lastError = QString("FITS Error: %1").arg(e.what());
            qDebug() << "Standard FITS Error:" << e.what();
            qDebug() << "File path:" << filePath;
            return false;
        } catch (...) {
            lastError = QString("Unknown error reading FITS file: %1").arg(filePath);
            qDebug() << "Unknown error reading FITS file:" << filePath;
            return false;
        }
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
    } else if (fileType == "TIFF") {
        return d->readTIFF(filePath);
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

void ImageReader::setImageData(const ImageData& imageData)
{
    d->imageData = imageData;
    d->lastError.clear();
}

QString ImageReader::lastError() const
{
    return d->lastError;
}

QStringList ImageReader::supportedFormats()
{
  return {"XISF", "FITS", "TIFF"};
}

QString ImageReader::formatFilter()
{
    return "Image Files (*.xisf *.fits *.fit *.fts *.tiff *.tif);;XISF Files (*.xisf);;FITS Files (*.fits *.fit *.fts);;TIFF Files (*.tiff *.tif);;All Files (*)";
}

QString ImageReader::detectFileType(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    if (suffix == "xisf") {
        return "XISF";
    } else if (suffix == "fits" || suffix == "fit" || suffix == "fts") {
        return "FITS";
    } else if (suffix == "tiff" || suffix == "tif") {
        return "TIFF";
    }
    
    // Try to detect by file content if extension is unclear
    // This could be enhanced with magic number detection
    return "Unknown";
}
