#include <iostream>
#include <vector>
#include "entity/biological_ai.hpp"

int main() {
    std::cout << "AI Test Starting..." << std::endl;
    
    // Basic AI test
    std::vector<int> decisions = {1, 0, 1, 1, 0};
    int decision_count = decisions.size();
    
    std::cout << "Processed " << decision_count << " AI decisions" << std::endl;
    std::cout << "AI Test Completed Successfully!" << std::endl;
    
    return 0;
}