/*
 * Baseline implementation using std::ifstream.
 *
 * Usage:
 *   ./baseline <filename>
 *
 * Example:
 *   ./baseline data/sample.bin
 *
 */
#include <arpa/inet.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc == 2) {
        std::string filename = argv[1];
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << "\n";
            return EXIT_FAILURE;
        }

        uint16_t length_be;
        uint64_t message_count = 0;

        uint64_t add_order_count = 0;
        uint64_t order_delete_count = 0;
        uint64_t order_cancel_count = 0;
        uint64_t order_replace_count = 0;
        uint64_t unknown_message_count = 0;

        std::vector<char> message;

        while (file.read(reinterpret_cast<char*>(&length_be), sizeof(length_be))) {
            uint16_t length = ntohs(length_be);

            if (length == 0) {
                std::cerr << "Invalid zero-length message\n";
                return EXIT_FAILURE;
            }

            message.resize(length);

            if (!file.read(message.data(), length)) {
                std::cerr << "Truncated message body\n";
                return EXIT_FAILURE;
            }

            char type = message[0];

            switch (type) {
            case 'A':
                add_order_count++;
                break;
            case 'F':
                add_order_count++;
                break;
            case 'D':
                order_delete_count++;
                break;
            case 'X':
                order_cancel_count++;
                break;
            case 'U':
                order_replace_count++;
                break;
            default:
                unknown_message_count++;
                break;
            }

            message_count++;
        }
        std::cout << "Message count: " << message_count << "\n";
        std::cout << "Add order count: " << add_order_count << "\n";
        std::cout << "Order delete count: " << order_delete_count << "\n";
        std::cout << "Order cancel count: " << order_cancel_count << "\n";
        std::cout << "Order replace count: " << order_replace_count << "\n";
        std::cout << "Unknown message count: " << unknown_message_count << "\n";

    } else {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}