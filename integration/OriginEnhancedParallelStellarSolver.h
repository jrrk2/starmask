// Enhanced ParallelStellarSolver that uses Origin acceleration

class OriginEnhancedParallelStellarSolver : public ParallelStellarSolver
{
    Q_OBJECT

public:
    OriginEnhancedParallelStellarSolver(int maxConcurrent = 8, QObject *parent = nullptr)
        : ParallelStellarSolver(maxConcurrent, parent) 
    {
        m_originInterface = new OriginStellarSolverInterface(this);
        
        connect(m_originInterface, &OriginStellarSolverInterface::originHintsExtracted,
                this, &OriginEnhancedParallelStellarSolver::onOriginHintsExtracted);
    }

    // Override addJob to handle TIFF files
    void addJob(const QString& filename) override {
        QFileInfo fileInfo(filename);
        
        if (fileInfo.suffix().toLower() == "tiff" || fileInfo.suffix().toLower() == "tif") {
            addOriginTIFFJob(filename);
        } else {
            // Use parent implementation for FITS files
            ParallelStellarSolver::addJob(filename);
        }
    }
    
    void addOriginTIFFJob(const QString& tiffFilename) {
        OriginStellarSolverJob job;
        job.filename = tiffFilename;
        job.jobId = m_enhancedJobs.size() + 1;
        
        // Load and extract Origin hints
        if (m_originInterface->loadOriginTIFF(tiffFilename, job)) {
            m_enhancedJobs.append(job);
            qDebug() << "Added Origin TIFF job:" << QFileInfo(tiffFilename).fileName();
        } else {
            qDebug() << "Failed to load Origin TIFF:" << tiffFilename;
        }
    }

private slots:
    void onOriginHintsExtracted(const OriginStellarSolverJob& job) {
        qDebug() << "Origin hints ready for job" << job.jobId << ":" << job.objectName;
        // Job is ready for processing with acceleration
    }

    // Override solver creation to use Origin hints
    void startOriginTIFFJob(const OriginStellarSolverJob& job) {
        StellarSolver* solver = new StellarSolver(this);
        
        // Configure with your existing settings
        solver->setProperty("ProcessType", SSolver::SOLVE);
        solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
        solver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
        solver->setIndexFolderPaths(m_indexPaths);
        solver->setParameters(m_commonParams);
        
        // Convert TIFF to StellarSolver format
        FITSImage::Statistic stats;
        std::vector<uint8_t> buffer;
        
        if (m_originInterface->convertTIFFToStellarSolverFormat(job.filename, stats, buffer)) {
            // Store buffer to keep it alive
            m_imageBuffers[solver] = std::move(buffer);
            
            // Load into solver
            if (solver->loadNewImageBuffer(stats, m_imageBuffers[solver].data())) {
                // Configure with Origin hints
                m_originInterface->configureSolverWithOriginHints(solver, job);
                
                // Start solving
                connect(solver, &StellarSolver::finished, 
                        this, &OriginEnhancedParallelStellarSolver::onOriginSolverFinished);
                
                m_activeOriginSolvers[solver] = job;
                solver->start();
                
                qDebug() << "Started Origin TIFF solving for" << job.objectName;
            } else {
                qDebug() << "Failed to load image buffer into solver";
                solver->deleteLater();
            }
        } else {
            qDebug() << "Failed to convert TIFF for solver";
            solver->deleteLater();
        }
    }
    
    void onOriginSolverFinished() {
        StellarSolver* solver = qobject_cast<StellarSolver*>(sender());
        if (!solver || !m_activeOriginSolvers.contains(solver)) {
            return;
        }
        
        OriginStellarSolverJob job = m_activeOriginSolvers[solver];
        
        if (solver->solvingDone() && solver->hasWCSData()) {
            FITSImage::Solution solution = solver->getSolution();
            
            qDebug() << "✅ Origin TIFF solved:" << job.objectName;
            qDebug() << "   Solution: RA=" << solution.ra << "°, Dec=" << solution.dec << "°";
            qDebug() << "   Scale:" << solution.pixscale << "arcsec/pixel";
            
            // Analyze hint accuracy
            m_originInterface->analyzeResults(job, solution);
            
            // TODO: Write results to output file
            
        } else {
            qDebug() << "❌ Origin TIFF solve failed:" << job.objectName;
        }
        
        // Cleanup
        m_activeOriginSolvers.remove(solver);
        m_imageBuffers.remove(solver);
        solver->deleteLater();
    }

private:
    OriginStellarSolverInterface* m_originInterface;
    QList<OriginStellarSolverJob> m_enhancedJobs;
    QHash<StellarSolver*, OriginStellarSolverJob> m_activeOriginSolvers;
};
