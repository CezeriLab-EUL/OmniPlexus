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
              << " <json1> [<json2> ...] <header_output_dir> <source_output_dir>\n";
    std::cout << "  " << programName
              << " --validate-only <json1> [<json2> ...]\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName
              << " devices/IndicatorBoard.json include/smartdrive/generated/ src/generated/\n";
    std::cout << "  " << programName
              << " devices/IndicatorBoard.json devices/OtherBoard.json include/smartdrive/generated/ src/generated/\n";
    std::cout << "  " << programName
              << " --validate-only devices/IndicatorBoard.json devices/OtherBoard.json\n\n";
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
    std::cout << TOOL_NAME << "\n";
    std::cout << std::string(40, '-') << "\n";

    const bool validateOnly = (argc >= 2 && std::string(argv[1]) == "--validate-only");

    // Need at least: [--validate-only] <json1> [<headerDir> <sourceDir>]
    if (validateOnly) {
        if (argc < 3) {
            std::cerr << "Error: --validate-only requires at least one JSON file\n";
            printUsage(argv[0]);
            return 1;
        }
    } else {
        if (argc < 4) {
            std::cerr << "Error: expected at least one JSON file plus header and source output dirs\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Last two args are output dirs (unless validate-only)
    // All args between argv[1] and the output dirs are JSON files
    const std::string headerOutputDir = validateOnly ? "" : argv[argc - 2];
    const std::string sourceOutputDir = validateOnly ? "" : argv[argc - 1];

    const int jsonArgStart = 1 + (validateOnly ? 1 : 0);
    const int jsonArgEnd   = validateOnly ? argc : argc - 2;

    std::vector<json> allData;

    // Read and validate each JSON
    bool anyError = false;
    for (int i = jsonArgStart; i < jsonArgEnd; ++i) {
        const std::string jsonPath = argv[i];
        std::cout << "\nReading: " << jsonPath << "\n";

        std::ifstream jsonFile(jsonPath);
        if (!jsonFile.is_open()) {
            std::cerr << "Error: Could not open file: " << jsonPath << "\n";
            anyError = true;
            continue;
        }

        json data;
        try {
            data = json::parse(jsonFile);
        } catch (const json::parse_error& e) {
            std::cerr << "Error: Failed to parse JSON: " << jsonPath << "\n";
            std::cerr << "  " << e.what() << "\n";
            anyError = true;
            continue;
        }
        jsonFile.close();
        std::cout << "JSON parsed successfully.\n";

        std::cout << "\nValidating: " << jsonPath << "...\n";
        const ValidationResult validation = Validator::validate(data);
        printValidationResults(validation);

        if (!validation.valid) {
            anyError = true;
        } else {
            allData.push_back(data);
        }
    }

    if (anyError) {
        std::cerr << "\nGeneration aborted due to errors. Fix the issues above and try again.\n\n";
        return 1;
    }

    // Cross-device validation
    std::cout << "\nRunning cross-device validation...\n";
    const ValidationResult crossResult = Validator::validateCrossDevice(allData);
    printValidationResults(crossResult);
    if (!crossResult.valid) {
        std::cerr << "\nGeneration aborted due to cross-device errors.\n\n";
        return 1;
    }

    size_t totalCommands = 0;
    for (const auto& d : allData) totalCommands += d["commands"].size();

    if (validateOnly) {
        std::cout << "\n✓ Validation passed! "
                  << totalCommands << " command(s) validated across "
                  << allData.size() << " device(s).\n";
        std::cout << "(--validate-only mode: no files generated)\n\n";
        return 0;
    }

    // Generation
    std::cout << "\nGenerating files...\n";
    try {
        Generator::generate(allData, headerOutputDir, sourceOutputDir);
    } catch (const std::exception& e) {
        std::cerr << "\nError during generation: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n✓ Done! " << totalCommands << " command(s) processed across "
              << allData.size() << " device(s).\n\n";
    return 0;
}