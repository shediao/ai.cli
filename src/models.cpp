#include "ai/models.h"

#include <iostream>

#include "ai/args.h"
#include "ai/openai.h"

namespace ai {

int models() {
  try {
    OpenAIClient client;
    auto model_list = client.models();
    for (auto& model : model_list) {
      std::cout << model << '\n';
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}

}  // namespace ai
