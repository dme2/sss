#include <iostream>
#include <fstream>
#include <vector>

int main() {
    std::ifstream file("output_test", std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file" << std::endl;
        return 1;
    }

    // Get the file size
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the bytes from the file
    std::vector<char> bytes(fileSize);
    file.read(&bytes[0], fileSize);
    file.close();

    // Convert the bytes to floats
    std::vector<float> floats;
    for (size_t i = 0; i < bytes.size(); i += sizeof(float)) {
        float value;
        std::memcpy(&value, &bytes[i], sizeof(float));
        floats.push_back(value);
    }

    // Print the floats
    for (float value : floats) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    return 0;
}
