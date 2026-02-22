#ifndef PROGRESS_BAR_H
#define PROGRESS_BAR_H

#include <string>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace etca {

/**
 * @brief Progress bar with ETA calculation and nice formatting
 */
class ProgressBar {
public:
    ProgressBar(const std::string& task_name, size_t total_steps = 100)
        : task_name_(task_name), total_steps_(total_steps), current_step_(0),
          start_time_(std::chrono::high_resolution_clock::now()),
          last_update_time_(start_time_), width_(50) {
    }
    
    /**
     * @brief Update progress (0.0 to 1.0)
     */
    void update(double progress) {
        current_step_ = static_cast<size_t>(progress * total_steps_);
        current_step_ = std::min(current_step_, total_steps_);
        render();
    }
    
    /**
     * @brief Increment progress by one step
     */
    void increment() {
        if (current_step_ < total_steps_) {
            current_step_++;
            render();
        }
    }
    
    /**
     * @brief Set progress to a specific step
     */
    void set_step(size_t step) {
        current_step_ = std::min(step, total_steps_);
        render();
    }
    
    /**
     * @brief Mark as complete
     */
    void complete() {
        current_step_ = total_steps_;
        render();
        std::cout << "\n";
    }
    
    /**
     * @brief Get elapsed time in seconds
     */
    double get_elapsed() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(now - start_time_).count();
    }
    
    /**
     * @brief Get estimated time remaining
     */
    double get_eta() const {
        if (current_step_ == 0) return 0.0;
        double elapsed = get_elapsed();
        double progress = static_cast<double>(current_step_) / total_steps_;
        if (progress <= 0.0) return 0.0;
        double total_estimated = elapsed / progress;
        return total_estimated - elapsed;
    }
    
    /**
     * @brief Check if terminal supports colors (simple check)
     */
    static bool supports_colors() {
        #ifdef _WIN32
            // Windows 10+ supports ANSI colors
            return true;  // Assume true for modern Windows
        #else
            const char* term = std::getenv("TERM");
            return term != nullptr && std::string(term) != "dumb";
        #endif
    }

private:
    void render() {
        auto now = std::chrono::high_resolution_clock::now();
        double elapsed_since_last = std::chrono::duration<double>(now - last_update_time_).count();
        
        // Throttle updates to ~10 FPS
        if (elapsed_since_last < 0.1 && current_step_ < total_steps_) {
            return;
        }
        last_update_time_ = now;
        
        double progress = static_cast<double>(current_step_) / total_steps_;
        size_t filled = static_cast<size_t>(progress * width_);
        
        // Clear line and move cursor to start
        std::cout << "\r\033[K";
        
        // Task name
        std::cout << "\033[1m" << task_name_ << "\033[0m ";
        
        // Progress bar
        std::cout << "[";
        if (supports_colors()) {
            std::cout << "\033[32m";  // Green
        }
        for (size_t i = 0; i < filled; ++i) {
            std::cout << "█";
        }
        if (supports_colors()) {
            std::cout << "\033[0m";
        }
        for (size_t i = filled; i < width_; ++i) {
            std::cout << "░";
        }
        std::cout << "] ";
        
        // Percentage
        std::cout << std::setw(5) << std::fixed << std::setprecision(1) 
                  << (progress * 100.0) << "%";
        
        // ETA (hide when progress >= 95% to avoid "counting up" as elapsed grows)
        if (current_step_ < total_steps_) {
            if (progress >= 0.95) {
                std::cout << " finishing...";
            } else {
                double eta = get_eta();
                if (eta > 0 && eta < 3600) {
                    std::cout << " ETA: " << format_time(eta);
                } else {
                    std::cout << " ETA: --";
                }
            }
        } else {
            std::cout << " Done!";
        }
        
        std::cout.flush();
    }
    
    static std::string format_time(double seconds) {
        if (seconds < 60) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << seconds << "s";
            return oss.str();
        }
        int minutes = static_cast<int>(seconds / 60);
        double secs = seconds - (minutes * 60);
        std::ostringstream oss;
        oss << minutes << "m " << std::fixed << std::setprecision(1) << secs << "s";
        return oss.str();
    }
    
    std::string task_name_;
    size_t total_steps_;
    size_t current_step_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point last_update_time_;
    size_t width_;
};

} // namespace etca

#endif // PROGRESS_BAR_H
