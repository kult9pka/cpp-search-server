#pragma once
#include <chrono>
#include <string>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

class LogDuration {

public:
    LogDuration() = default;

	explicit LogDuration(const std::string& some_string, std::ostream& out):text(some_string), out_(out) {
	}

    explicit LogDuration(const std::string& some_string):text(some_string) {
    }
    
	~LogDuration() {
		using namespace std::string_literals;
		const std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
		const std::chrono::steady_clock::duration duration = end_time - start_time;
        double dur = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        out_ << text << ": "s << dur << " ms"s << std::endl;
	}
private:
	const std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    const std::string text = "Operation time";
    std::ostream& out_ = std::cerr;
};