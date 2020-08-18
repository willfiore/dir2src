#include <array>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <sstream>

#define NOMINMAX
#include <Windows.h>

bool ReadFile(std::string_view file_path, std::vector<uint8_t>* output_buffer) {
    HANDLE h_input_file = ::CreateFile(
        file_path.data(),      // lpFileName
        GENERIC_READ,          // dwDesiredAccess
        0,                     // dwShareMode
        NULL,                  // lpSecurityAttributes
        OPEN_EXISTING,         // dwCreeationDisposition
        FILE_ATTRIBUTE_NORMAL, // dwFlagsAndAttributes
        NULL                   // hTemplateFile
    );

    if (h_input_file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open input file: %lu", GetLastError());
        return false;
    }

    DWORD file_size = ::GetFileSize(h_input_file, nullptr);

    output_buffer->resize(static_cast<size_t>(file_size));

    DWORD number_of_bytes_read;
    BOOL read_success = ::ReadFile(h_input_file, output_buffer->data(), file_size, &number_of_bytes_read, NULL);

    if (!read_success) {
        fprintf(stderr, "Failed to read input file: %lu", GetLastError());
        CloseHandle(h_input_file);
        return false;
    }

    return true;
}

std::string CodeFriendlyString(std::string str) {
    for (auto& c : str) {
        if (!std::isalnum(c)) {
            c = '_';
        }
    }

    while (!std::isalnum(str.front())) {
        str.erase(0, 1);
    }

    if (std::isdigit(str.front())) {
        str = "_" + str;
    }

    return str;
}

std::vector<std::string> SplitString(std::string string, const std::string& delimiter) {
    std::vector<std::string> ret;

    size_t delimiter_idx = 0;
    while (delimiter_idx != std::string::npos) {
        delimiter_idx = string.find(delimiter);
        std::string token = string.substr(0, delimiter_idx);

        if (!token.empty()) {
            ret.push_back(token);
        }
        string.erase(0, delimiter_idx + 1);
    }

    return ret;
}

std::string NormalizeDirectoryString(std::string string) {
    if (string.empty()) return string;

    for (auto& c : string) {
        if (c == '/') c = '\\';
    }

    if (string.back() != '\\') {
        string.push_back('\\');
    }

    return string;
}

bool WriteFile(std::string_view file_path, std::string_view contents) {
    HANDLE h_output_file = ::CreateFile(
        file_path.data(),      // lpFileName
        GENERIC_WRITE,         // dwDesiredAccess
        0,                     // dwShareMode
        NULL,                  // lpSecurityAttributes
        CREATE_ALWAYS,         // dwCreeationDisposition
        FILE_ATTRIBUTE_NORMAL, // dwFlagsAndAttributes
        NULL                   // hTemplateFile
    );

    if (h_output_file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to create output file: %lu", GetLastError());
        CloseHandle(h_output_file);
        return false;
    }

    DWORD number_of_bytes_written;

    BOOL write_success = ::WriteFile(
        h_output_file,
        contents.data(),
        (DWORD)contents.size(),
        &number_of_bytes_written,
        NULL
    );

    if (!write_success) {
        fprintf(stderr, "Failed to write output file: %lu", GetLastError());
        CloseHandle(h_output_file);
        return false;
    }

    return true;
}

struct CommandLineOption {

    enum class Id {
        HELP,
        ROOT_NAMESPACE,
        PRINT_OUTPUT_FILES,
        MAX
    } id;

    std::string long_name;
    std::string short_name;
    std::string description;
    std::string default;

    enum class Type {
        BOOLEAN,
        STRING,
    } type;
};

// Must be in same order as Id enum
CommandLineOption command_line_options[] = {
    CommandLineOption {
        .id = CommandLineOption::Id::HELP,
        .long_name = "help",
        .short_name = "h",
        .description = "print this summary",
        .default = "0",
        .type = CommandLineOption::Type::BOOLEAN,
    },
    CommandLineOption {
        .id = CommandLineOption::Id::ROOT_NAMESPACE,
        .long_name = "root-namespace",
        .short_name = "n",
        .description = "name of root namespace in output",
        .default = "Bin",
        .type = CommandLineOption::Type::STRING,
    },
    CommandLineOption {
        .id = CommandLineOption::Id::PRINT_OUTPUT_FILES,
        .long_name = "print-output-files",
        .short_name = "p",
        .description = "print absolute paths of output source files\ne.g. to feed into build systems",
        .default = "0",
        .type = CommandLineOption::Type::BOOLEAN,
    },
};

void PrintHelp() {
    printf(R"(
Usage:

    dir2src [OPTIONS] <input-path> <output-path>

Options:

)");

    constexpr size_t flag_str_length = 32;

    for (const auto& flag : command_line_options) {

        std::stringstream ss;

        ss << "    ";

        if (!flag.short_name.empty()) {
            ss << "-" << flag.short_name;

            if (!flag.long_name.empty()) {
                ss << ", ";
            }

        } else {
            ss << "    ";

            if (flag.long_name.empty()) {
                ss << "  ";
            }
        }

        if (!flag.long_name.empty()) {
            ss << "--" << flag.long_name;
        }

        std::string spacing_str(flag_str_length - ss.str().size(), ' ');
        ss << spacing_str;

        std::vector<std::string> descriptions = SplitString(flag.description, "\n");

        ss << descriptions[0];

        if (descriptions.size() > 0) {
            for (size_t i = 1; i < descriptions.size(); ++i) {
                ss << "\n" << std::string(flag_str_length, ' ');
                ss << descriptions[i];
            }
        }

        if (!flag.default.empty() && flag.type != CommandLineOption::Type::BOOLEAN) {
            ss << " [default: \"" << flag.default << "\"]";
        }

        ss << "\n";

        printf("%s", ss.str().c_str());
    }

    printf("\n");
}

int main(int argc, const char* argv[]) {

    if (argc < 3) {
        PrintHelp();
        return 0;
    }

    std::array<std::string, (size_t)CommandLineOption::Id::MAX> args;

    // Populate default args
    for (size_t i = 0; i < (size_t)CommandLineOption::Id::MAX; ++i) {
        args[i] = command_line_options[i].default;
    }

    bool print_help = false;
    std::string unknown_arg;

    // Parse command line arguments
    for (int i = 1; i < argc - 2; ++i) {
        std::string arg = argv[i];

        const CommandLineOption* command_line_option = nullptr;

        for (const auto& option : command_line_options) {
            if (arg == "-" + option.short_name ||
                arg == "--" + option.long_name) {
                command_line_option = &option;
                break;
            }
        }

        if (command_line_option && command_line_option->id == CommandLineOption::Id::HELP) {
            print_help = true;
        }

        if (command_line_option == nullptr) {
            unknown_arg = arg;
            continue;
        }

        if (command_line_option->type == CommandLineOption::Type::BOOLEAN) {
            args[static_cast<size_t>(command_line_option->id)] = "1";
        }
        else if (command_line_option->type == CommandLineOption::Type::STRING) {
            // Read ahead
            ++i;
            if (i >= argc - 2) {
                fprintf(stderr, "Missing value for option %s\n", arg.c_str());
                return 1;
            }

            std::string val = argv[i];
            args[(size_t)command_line_option->id] = val;
        }
    }

    if (print_help) {
        PrintHelp();
        return 0;
    }

    if (!unknown_arg.empty()) {
        fprintf(stderr, "Unknown option \"%s\"\n", unknown_arg.c_str());
        return 1;
    }

    DWORD cwd_length = ::GetCurrentDirectory(0, NULL);
    std::string cwd(cwd_length, '\0');
    ::GetCurrentDirectory(cwd_length, cwd.data());
    cwd.back() = '\\'; // replace null terminator with backslash

    std::string root_input_path  = NormalizeDirectoryString(argv[argc - 2]);
    std::string root_output_path = NormalizeDirectoryString(argv[argc - 1]);

    std::vector<std::string> root_input_directories = SplitString(root_input_path, "\\");
    std::vector<std::string> root_output_directories = SplitString(root_output_path, "\\");

    std::vector<std::string> open_directory_list{ root_input_path };

    std::vector<std::string> header_namespaces;

    std::stringstream ss_header_file;
    ss_header_file << R"(// AUTOGENERATED

#pragma once

#include <array>
#include <cstdint>

namespace )";

    ss_header_file << args[(size_t)CommandLineOption::Id::ROOT_NAMESPACE] << " {\n\n";

    while (!open_directory_list.empty()) {
        std::string dir = NormalizeDirectoryString(open_directory_list.back());
        open_directory_list.pop_back();

        std::string search_path = dir;
        if (search_path.back() != '*') {
            search_path.push_back('*');
        }

        WIN32_FIND_DATA find_data = {};
        HANDLE h_find_file = ::FindFirstFile(search_path.c_str(), &find_data);

        while (h_find_file != INVALID_HANDLE_VALUE) {

            if (!strcmp(find_data.cFileName, ".") ||
                !strcmp(find_data.cFileName, "..")) {

                BOOL found_next_file = ::FindNextFile(h_find_file, &find_data);
                if (!found_next_file) break;
                continue;
            }

            std::string relative_path = dir + find_data.cFileName;

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                open_directory_list.push_back(relative_path);
            }
            else {
                
                std::vector<uint8_t> file_data;
                ReadFile(relative_path, &file_data);

                std::vector<std::string> directories = SplitString(dir, "\\");
                std::vector<std::string> output_directories = directories;
                output_directories.erase(output_directories.begin(), output_directories.begin() + root_input_directories.size());

                std::vector<std::string> namespaces = output_directories;

                output_directories.insert(output_directories.begin(), root_output_directories.begin(), root_output_directories.end());

                for (auto& str : namespaces) {
                    str = CodeFriendlyString(str);
                }

                std::string s = dir;

                std::stringstream ss_cpp_file;
                ss_cpp_file << R"(// AUTOGENERATED

#include <array>
#include <cstdint>

namespace )";

                ss_cpp_file << args[(size_t)CommandLineOption::Id::ROOT_NAMESPACE] << " {\n";

                for (const auto& n : namespaces) {
                    ss_cpp_file << "namespace " << n << " {\n";
                }

                ss_cpp_file << "\nstd::array<uint8_t, ";

                std::string array_name = CodeFriendlyString(find_data.cFileName);

                ss_cpp_file << file_data.size() << "> " << array_name << " = {\n\n";

                constexpr size_t split = 12;

                for (size_t i = 0; i < file_data.size(); ++i) {

                    if (i % split == 0) {
                        ss_cpp_file << "    ";
                    }

                    uint8_t c = file_data[i];

                    if (c < 10) ss_cpp_file << "00";
                    else if (c < 100) ss_cpp_file << "0";

                    ss_cpp_file << (int)c;

                    if (i != file_data.size() - 1) {
                        ss_cpp_file << ",";

                        if ((i + 1) % split == 0) {
                            ss_cpp_file << "\n";
                        }
                        else {
                            ss_cpp_file << " ";
                        }
                    }
                }

                ss_cpp_file << "\n\n};\n\n";

                for (auto it = namespaces.rbegin(); it != namespaces.rend(); ++it) {
                    ss_cpp_file << "} // end of namespace " << *it << "\n";
                }
                ss_cpp_file << "} // end of namespace Bin\n";

                std::string output_data = ss_cpp_file.str();

                std::string file_output_path = root_output_path + dir;
                std::vector<std::string> file_output_directory_list = SplitString(file_output_path, "\\");

                // Create nested output directories
                std::string current_directory_path;
                for (const auto& directory : output_directories) {
                    current_directory_path += directory + "\\";
                    ::CreateDirectory(current_directory_path.c_str(), NULL);
                }

                std::string path = current_directory_path + find_data.cFileName + ".cpp";
                ::WriteFile(current_directory_path + find_data.cFileName + ".cpp", output_data);

                if (args[(size_t)CommandLineOption::Id::PRINT_OUTPUT_FILES] == "1") {
                    printf("%s%s\n", cwd.c_str(), path.c_str());
                }

                // Populate namespaces for header
                for (size_t i = 0; i < header_namespaces.size(); ++i) {
                    if (header_namespaces[i] != namespaces[i] || namespaces.size() <= i) {
                        size_t diff = header_namespaces.size() - namespaces.size() + 1;

                        for (size_t j = 0; j < diff; ++j) {
                            ss_header_file << "\n}\n";
                        }

                        header_namespaces.resize(i);
                        break;
                    }
                }

                for (size_t i = header_namespaces.size(); i < namespaces.size(); ++i) {
                    header_namespaces.push_back(namespaces[i]);
                    ss_header_file << "\nnamespace " << namespaces[i] << " {\n\n";
                }

                ss_header_file << "extern std::array<uint8_t, " << file_data.size() << "> " << array_name << ";\n";
            }

            BOOL found_next_file = ::FindNextFile(h_find_file, &find_data);
            if (!found_next_file) break;
        }
    }

    for (size_t i = 0; i < header_namespaces.size() + 1; ++i) {
        ss_header_file << "\n}\n";
    }

    std::string header_output_data = ss_header_file.str();
    ::WriteFile(root_output_path + "bin.h", header_output_data);

    return 0;
}