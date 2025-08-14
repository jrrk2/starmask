#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <getopt.h>

// Astrometry.net headers - using the exact same headers as engine-main.c
extern "C" {
#include "astrometry/engine.h"
#include "astrometry/onefield.h"
#include "astrometry/solver.h"
#include "astrometry/starxy.h"
#include "astrometry/matchobj.h"
#include "astrometry/starutil.h"
  // #include "astrometry/log.h"
#include "astrometry/errors.h"
#include "astrometry/bl.h"
#include "astrometry/an-opts.h"
#include "astrometry/gslutils.h"
#include "astrometry/ioutils.h"
#include "astrometry/fileutils.h"
#include "astrometry/xylist.h"
#include "astrometry/fitstable.h"
#include "astrometry/fitsioutils.h"
#include "astrometry/sip_qfits.h"
}

struct StarPosition {
    double x, y, flux;
    StarPosition(double x = 0, double y = 0, double flux = 1.0) 
        : x(x), y(y), flux(flux) {}
};

struct DirectSolveOptions {
    std::string indexPath = "/opt/homebrew/share/astrometry";
    std::string configFile = "";  // Use default config resolution
    double minScale = 0.1;      // arcsec/pixel
    double maxScale = 60.0;     // arcsec/pixel  
    int imageWidth = 4000;
    int imageHeight = 3000;
    int maxStars = 200;
    double searchRadius = 15.0;  // degrees
    double logOddsThreshold = 14.0;
    bool verbose = false;
    
    // Optional field center guess for faster solving
    bool hasGuess = false;
    double raGuess = 0.0;       // degrees
    double decGuess = 0.0;      // degrees
    
    // Depths to try (corresponds to how many stars to use)
    std::vector<int> depths = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    
    // CPU time limit in seconds
    float cpuLimit = 60.0;
};

struct SolveResult {
    bool solved = false;
    tan_t wcs;
    std::string indexUsed;
    std::string message;
};

class AstrometryDirectSolver {
private:
    engine_t* engine;
    bool initialized;
    char wcs_template[256];
    int template_fd;
    
public:
    AstrometryDirectSolver() : engine(nullptr), initialized(false) {
        // Initialize GSL and logging like engine-main does
        gslutils_use_error_system();
        loginit();
        errors_log_to(stderr);
    }
    
    ~AstrometryDirectSolver() {
        cleanup();
    }
    
    bool initialize(const DirectSolveOptions& options) {
        if (initialized) {
            std::cerr << "Engine already initialized" << std::endl;
            return false;
        }
        
        // Create engine exactly like engine-main.c does
        engine = engine_new();
        if (!engine) {
            std::cerr << "Failed to create astrometry engine" << std::endl;
            return false;
        }
        
        // Set CPU limit
        engine->cpulimit = options.cpuLimit;
        
        if (options.verbose) {
            logverbose();
            std::cout << "Created astrometry engine" << std::endl;
        }
        
        // Parse config file (using the same logic as engine-main.c)
        std::string configFile = options.configFile;
        if (configFile.empty()) {
            configFile = findDefaultConfigFile();
        }
        
        if (!configFile.empty() && configFile != "none") {
            if (options.verbose) {
                std::cout << "Using config file: " << configFile << std::endl;
            }
            
            if (engine_parse_config_file(engine, configFile.c_str())) {
                std::cerr << "Failed to parse config file: " << configFile << std::endl;
                engine_free(engine);
                engine = nullptr;
                return false;
            }
        }
        
        // Add the index search path if specified
        if (!options.indexPath.empty()) {
            engine_add_search_path(engine, options.indexPath.c_str());
            
            if (options.verbose) {
                std::cout << "Added index search path: " << options.indexPath << std::endl;
            }
            
            // Auto-load indexes from the path
            if (engine_autoindex_search_paths(engine)) {
                std::cerr << "Failed to load indexes from path: " << options.indexPath << std::endl;
                engine_free(engine);
                engine = nullptr;
                return false;
            }
        }
        
        // Check that we have indexes loaded
        if (!pl_size(engine->indexes)) {
            std::cerr << "No index files found! Check your index path and config file." << std::endl;
            engine_free(engine);
            engine = nullptr;
            return false;
        }
        
        if (options.verbose) {
            std::cout << "Loaded " << pl_size(engine->indexes) << " index files" << std::endl;
        }
        
        // Set default field width constraints
        if (engine->minwidth <= 0.0) engine->minwidth = 0.1;
        if (engine->maxwidth <= 0.0) engine->maxwidth = 180.0;
        
        // Set default depths if not specified in config
        if (!il_size(engine->default_depths)) {
            for (int depth : options.depths) {
                il_append(engine->default_depths, depth);
            }
        }
        
        initialized = true;
        return true;
    }
    
    SolveResult solve(const std::vector<StarPosition>& stars, const DirectSolveOptions& options) {
        SolveResult result;
        
        if (!initialized) {
            result.message = "Engine not initialized";
            return result;
        }
        
        if (stars.empty()) {
            result.message = "No stars provided";
            return result;
        }
        
        // Create job directly from stars without temporary files
        job_t* job = createJobDirectly(stars, options);
        if (!job) {
            result.message = "Failed to create job from star data";
            return result;
        }
        
        if (options.verbose) {
            std::cout << "Created job with " << stars.size() << " stars" << std::endl;
            std::cout << "Image dimensions: " << options.imageWidth << "x" << options.imageHeight << std::endl;
            std::cout << "Scale range: " << options.minScale << " - " << options.maxScale << " arcsec/pixel" << std::endl;
        }
        
        // Run the job using the engine (this is the core solving)
        if (options.verbose) {
            std::cout << "Running astrometry engine..." << std::endl;
        }
        
        int solve_result = engine_run_job(engine, job);
        
        if (solve_result == 0) {
            tan_t *rslt = tan_read_header_file(wcs_template, &(result.wcs));
            result.solved = rslt != NULL;
        } else {
            result.message = "Engine failed to run job";
        }
        
        // Clean up
        job_free(job);
        
        return result;
    }
    
    void cleanup() {
        if (engine) {
            engine_free(engine);
            engine = nullptr;
        }
        initialized = false;
    }

private:
    std::string findDefaultConfigFile() {
        // Replicate the config file search logic from engine-main.c
        std::vector<std::string> tryPaths = {
            "/opt/homebrew/etc/astrometry.cfg",  // Common on macOS with Homebrew
            "/usr/local/astrometry/etc/astrometry.cfg",
            "/etc/astrometry.cfg",
            "../etc/astrometry.cfg",
            "./astrometry.cfg"
        };
        
        for (const std::string& path : tryPaths) {
            if (file_exists(path.c_str())) {
                return path;
            }
        }
        
        std::cerr << "Warning: No config file found, using built-in defaults" << std::endl;
        return "none";
    }
    
    job_t* createJobDirectly(const std::vector<StarPosition>& stars, const DirectSolveOptions& options) {
        // Create a job_t structure directly from the star data
        // This avoids creating temporary files
        
        // Allocate job structure
        job_t* job = (job_t*)calloc(1, sizeof(job_t));
        if (!job) {
            return nullptr;
        }
        
        // Initialize data structures
        job->scales = dl_new(8);
        job->depths = il_new(8);
        job->include_default_scales = TRUE;
        job->use_radec_center = FALSE;
        
        // Set scale constraints
        dl_append(job->scales, options.minScale);
        dl_append(job->scales, options.maxScale);
        
        // Set depths
        for (int depth : options.depths) {
            il_append(job->depths, depth);
        }
        
        // Set field center guess if provided
        if (options.hasGuess) {
            job->use_radec_center = TRUE;
            job->ra_center = options.raGuess;
            job->dec_center = options.decGuess;
            job->search_radius = options.searchRadius;
        }
        
        // Initialize the onefield_t structure (bp = blind plate)
        onefield_init(&job->bp);
        
        // Create in-memory xylist from our stars
        if (!createInMemoryXylist(job, stars, options)) {
            job_free(job);
            return nullptr;
        }
        
        // Set up the solver within the onefield structure
        setupOnefieldSolver(job, options);
        
        // Add indexes from the engine to the onefield
        for (int i = 0; i < (int)pl_size(engine->indexes); i++) {
            index_t* index = (index_t*)pl_get(engine->indexes, i);
            onefield_add_loaded_index(&job->bp, index);
        }
        
        // Set solver parameters
        job->bp.solver.funits_lower = options.minScale;
        job->bp.solver.funits_upper = options.maxScale;
        job->bp.cpulimit = options.cpuLimit;
        strcpy(wcs_template, "wcs_template_XXXXXX");
        template_fd = mkstemp(wcs_template);
        job->bp.wcs_template = strdup(wcs_template);
        
        // Set log odds threshold
        solver_set_keep_logodds(&job->bp.solver, options.logOddsThreshold);
        
        return job;
    }
    
    bool createInMemoryXylist(job_t* job, const std::vector<StarPosition>& stars, const DirectSolveOptions& options) {
        // Create a temporary file to hold the xylist data
        char tempName[] = "/tmp/astro_stars_XXXXXX.xyls";
        int fd = mkstemps(tempName, 5);  // 5 = length of ".xyls"
        if (fd == -1) {
            std::cerr << "Failed to create temporary xylist file" << std::endl;
            return false;
        }
        close(fd);
        
        std::string tempFilename(tempName);
        
        // Create xylist for writing
        xylist_t* ls = xylist_open_for_writing(tempFilename.c_str());
        if (!ls) {
            std::cerr << "Failed to create xylist for writing" << std::endl;
            unlink(tempFilename.c_str());
            return false;
        }
        
        // Write primary header with image dimensions
        qfits_header* primary_hdr = xylist_get_primary_header(ls);
        if (primary_hdr) {
            char width_str[32], height_str[32];
            snprintf(width_str, sizeof(width_str), "%d", options.imageWidth);
            snprintf(height_str, sizeof(height_str), "%d", options.imageHeight);
            qfits_header_add(primary_hdr, "IMAGEW", width_str, "Image width in pixels", NULL);
            qfits_header_add(primary_hdr, "IMAGEH", height_str, "Image height in pixels", NULL);
        }
        
        if (xylist_write_primary_header(ls)) {
            std::cerr << "Failed to write primary header" << std::endl;
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return false;
        }
        
        // Start a new field (extension)
        if (xylist_next_field(ls)) {
            std::cerr << "Failed to create new field" << std::endl;
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return false;
        }
        
        // Set up the field header with image dimensions
        qfits_header* field_hdr = xylist_get_header(ls);
        if (field_hdr) {
            char width_str[32], height_str[32];
            snprintf(width_str, sizeof(width_str), "%d", options.imageWidth);
            snprintf(height_str, sizeof(height_str), "%d", options.imageHeight);
            qfits_header_add(field_hdr, "IMAGEW", width_str, "Image width in pixels", NULL);
            qfits_header_add(field_hdr, "IMAGEH", height_str, "Image height in pixels", NULL);
        }
        
        // Write the field header
        if (xylist_write_header(ls)) {
            std::cerr << "Failed to write field header" << std::endl;
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return false;
        }
        
        // Write all star data
        for (const auto& star : stars) {
            if (xylist_write_one_row_data(ls, star.x, star.y, star.flux, 0.0)) {
                std::cerr << "Failed to write star data" << std::endl;
                xylist_close(ls);
                unlink(tempFilename.c_str());
                return false;
            }
        }
        
        // Fix the header (update NAXIS2 with actual row count)
        if (xylist_fix_header(ls)) {
            std::cerr << "Failed to fix field header" << std::endl;
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return false;
        }
        
        // Fix the primary header
        if (xylist_fix_primary_header(ls)) {
            std::cerr << "Failed to fix primary header" << std::endl;
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return false;
        }
        
        // Close the writing xylist
        if (xylist_close(ls)) {
            std::cerr << "Failed to close xylist after writing" << std::endl;
            unlink(tempFilename.c_str());
            return false;
        }
        
        // Now open the file for reading
        xylist_t* read_ls = xylist_open(tempFilename.c_str());
        if (!read_ls) {
            std::cerr << "Failed to open xylist for reading" << std::endl;
            unlink(tempFilename.c_str());
            return false;
        }
        
        // Open the first field (extension 1)
        if (xylist_open_field(read_ls, 1)) {
            std::cerr << "Failed to open field 1" << std::endl;
            xylist_close(read_ls);
            unlink(tempFilename.c_str());
            return false;
        }
        
        // Store the xylist in the job structure
        job->bp.xyls = read_ls;
        job->bp.fieldfname = strdup(tempFilename.c_str());
        
        // Set up field information
        onefield_add_field(&job->bp, 1);  // Field 1 (first extension) - returns void
        
        job->bp.fieldnum = 1;
        job->bp.fieldid = 1;
        
        return true;
    }
    
    void setupOnefieldSolver(job_t* job, const DirectSolveOptions& options) {
        onefield_t* bp = &job->bp;
        
        // Initialize solver within the onefield
        solver_set_default_values(&bp->solver);
        
        // Set up basic solving parameters
        bp->logratio_tosolve = options.logOddsThreshold;
        bp->quad_size_fraction_lo = DEFAULT_QSF_LO;
        bp->quad_size_fraction_hi = DEFAULT_QSF_HI;
        
        // Set timeouts
        bp->cpulimit = options.cpuLimit;
        bp->total_cpulimit = options.cpuLimit;
        
        // Configure how many solutions we want
        bp->nsolves = 1;  // Stop after finding one solution
        bp->best_hit_only = TRUE;
        
        // Initialize solution storage
        bp->solutions = bl_new(16, sizeof(MatchObj));
    }
};

int astrometry_direct(std::vector<StarPosition> stars);
