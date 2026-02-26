//
// Created by dunamis on 17/02/2026.
//

// Usage:
//   CommandGenerator <json_file> <header_output_dir> <source_output_dir>
//   CommandGenerator --validate-only <json_file>
//

#include <iostream>
#include <fstream>
#include <string>
#include "lib/nlohmann/json.hpp"
#include "Validator.h"
#include "Generator.h"

using json = nlohmann::json;

static const std::string TOOL_NAME    = "SmartDrive CommandGenerator";

// ─────────────────────────────────────────────
// Print usage instructions
// ─────────────────────────────────────────────
void printUsage(const char* programName) {
    std::cout << "\nUsage:\n";
    std::cout << "  " << programName
              << " <json_file> <header_output_dir> <source_output_dir>\n";
    std::cout << "  " << programName
              << " --validate-only <json_file>\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName
              << " devices/IndicatorBoard.json include/smartdrive/generated/ src/generated/\n";
    std::cout << "  " << programName
              << " --validate-only devices/IndicatorBoard.json\n\n";
}

// Print validation results
void printValidationResults(const ValidationResult& result) {
    if (!result.warnings.empty()) {
        std::cout << "\n";
        for (const auto& warning : result.warnings) {
            std::cout << "  ⚠  WARNING: " << warning << "\n";
        }
    }

    if (!result.errors.empty()) {
        std::cout << "\n";
        for (const auto& error : result.errors) {
            std::cerr << "  ✗  ERROR: " << error << "\n";
        }
    }

    std::cout << "\nValidation result: "
              << result.errors.size()   << " error(s), "
              << result.warnings.size() << " warning(s)\n";
}

int main(int argc, char* argv[]) {

    std::cout << TOOL_NAME  << "\n";
    std::cout << std::string(40, '-') << "\n";

    //Argument Parsing

    const bool validateOnly = (argc >= 2 && std::string(argv[1]) == "--validate-only");

    if (validateOnly) {
        if (argc != 3) {
            std::cerr << "Error: --validate-only requires exactly one argument (json file)\n";
            printUsage(argv[0]);
            return 1;
        }
    } else {
        if (argc != 4) {
            std::cerr << "Error: expected 3 arguments\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    const std::string jsonPath        = validateOnly ? argv[2] : argv[1];
    const std::string headerOutputDir = validateOnly ? ""       : argv[2];
    const std::string sourceOutputDir = validateOnly ? ""       : argv[3];

    // Read JSON File

    std::cout << "\nReading: " << jsonPath << "\n";

    std::ifstream jsonFile(jsonPath);
    if (!jsonFile.is_open()) {
        std::cerr << "Error: Could not open file: " << jsonPath << "\n";
        return 1;
    }

    json data;
    try {
        data = json::parse(jsonFile);
    } catch (const json::parse_error& e) {
        std::cerr << "Error: Failed to parse JSON\n";
        std::cerr << "  " << e.what() << "\n";
        return 1;
    }
    jsonFile.close();

    std::cout << "JSON parsed successfully.\n";

    // Validation

    std::cout << "\nValidating commands...\n";

    const ValidationResult validation = Validator::validate(data);
    printValidationResults(validation);

    if (!validation.valid) {
        std::cerr << "\nGeneration aborted due to errors. Fix the issues above and try again.\n\n";
        return 1;
    }

    // Count commands for summary
    const size_t commandCount = data["commands"].size();

    if (validateOnly) {
        std::cout << "\n✓ Validation passed! "
                  << commandCount << " command(s) validated.\n";
        std::cout << "(--validate-only mode: no files generated)\n\n";
        return 0;
    }

    //Code Generation

    std::cout << "\nGenerating files...\n";

    try {
        Generator::generate(data, headerOutputDir, sourceOutputDir);
    } catch (const std::exception& e) {
        std::cerr << "\nError during generation: " << e.what() << "\n";
        return 1;
    }

    //Summary

    std::cout << "\n✓ Done! " << commandCount << " command(s) processed.\n\n";

    return 0;
}